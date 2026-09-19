# DMIP - DMOD IP Protocol

## Overview

DMIP is the IP layer: building and parsing IPv4/IPv6 headers, the IPv4
header checksum, TTL (IPv4) / Hop Limit (IPv6) handling, identification
generation, fragmentation/reassembly for both families, and sending/
receiving actual packets on the wire. `dmip_addr_t` - the address type
every module in this tree uses - is re-exported here from dmroute, which
owns the real definition (see "Address type" below).

## Why a whole module for this

Header build/parse, checksums, TTL/hop-limit, identification and
fragmentation are all things *any* module that sends or receives IP
packets needs (dmroute forwarding a packet, dmudp, a future ICMP module),
and none of them are specific to a particular interface or routing
decision. Keeping them here means every consumer gets exactly one
implementation to depend on, instead of each reimplementing (and quietly
diverging on) header layout, checksum math, or fragment offset units.

## Address type: re-exported from dmroute, not defined here

dmip used to own `dmip_addr_t` outright, back when it had no dependencies
of its own. Once dmip needed to actually *send* a packet - which means
asking dmroute which interface/gateway to use - that stopped working:
dmroute would have needed dmip for the address type, and dmip would have
needed dmroute to send, a cycle.

The fix was to move the real definition to dmroute (`dmroute_addr_t`/
`dmroute_family_t` in `dmroute.h`) - the base of this tree's module graph,
with no dependencies of its own - and have `dmip.h` re-typedef it back:

```c
typedef dmroute_family_t dmip_family_t;
#define dmip_family_none dmroute_family_none
#define dmip_family_v4   dmroute_family_v4
#define dmip_family_v6   dmroute_family_v6
typedef dmroute_addr_t dmip_addr_t;
```

Every existing dmip.h consumer (starting with dmip.c itself) keeps working
under the `dmip_*` names completely unchanged - it's the same type either
way, just owned by dmroute. See dmroute's own docs for the full rationale
from dmroute's side.

## No options, no extension headers

`dmip_v4_build_header()` always emits a 20-byte header with IHL=5 (no
options) - real-world IPv4 options are rare enough on ordinary traffic
that supporting them in the *builder* would be speculative complexity for
a use case nothing in this tree has. `dmip_v4_parse_header()` still
tolerates a larger IHL on a *received* header (options are skipped, not
interpreted, via the `header_len` it reports).

Symmetrically, `dmip_v6_build_header()`/`dmip_v6_parse_header()` only
handle the 40-byte IPv6 fixed header. The one IPv6 extension header dmip
does understand is the Fragment header (RFC 8200 4.5) - entirely owned by
`dmip_v6_fragment()`/`dmip_v6_reassemble()`, since fragmentation is the
one case in this tree that actually needs it. Any other extension header
(Hop-by-Hop Options, Routing, ...) is out of scope until something
upstream of dmip actually needs one.

## Fragmentation is stateless, reassembly is not

`dmip_v4_fragment()`/`dmip_v6_fragment()` are pure functions: given a
payload and an MTU, they slice it and hand each resulting packet to a
caller-supplied callback, keeping no state of their own between calls.

`dmip_v4_reassemble()`/`dmip_v6_reassemble()` are the opposite - putting
fragments back together inherently requires remembering what's arrived so
far for a given (source, destination, protocol, identification) tuple
until either the last fragment shows up or too much time passes. Both
families share one system-wide reassembly table in `src/dmip.c` (a
`dmlist` of entries, guarded by one mutex) rather than each getting its
own - the bookkeeping (chunk list, coverage check, expiry) is completely
family-agnostic; only key construction and the final packet rebuild
differ. An entry that never completes is dropped after
`DMIP_REASSEMBLY_TIMEOUT_MS` (30s), checked opportunistically on every
`_reassemble()` call rather than by a background timer - dmod's minimal
module runtime gives every module a lifecycle, not a free-running clock
of its own.

Both `_reassemble()` functions are safe to call unconditionally on every
inbound packet: a packet that was never fragmented in the first place is
recognized immediately and handed back as-is, without ever touching the
reassembly table.

## Send / receive

dmip no longer touches routing, ARP, or raw frame I/O itself - all of that
moved to [dmnetbridge](../../dmnetbridge), the layer between dmip's
IP-address-level view of the world and the actual network interfaces.

`dmip_v4_send()` fills in `header->src` via `dmip_v4_get_source_address()`
if left unset, reads the egress interface's MTU (best-effort, via
`dmnetbridge_get_mtu()`), fragments the packet (the existing
`dmip_v4_fragment()`), and hands each fragment to `dmnetbridge_send()`
individually - which does the route lookup, ARP resolution, and Ethernet
framing/transmit that used to live here. A multi-fragment packet therefore
re-resolves the route/MAC once per fragment rather than once for the whole
packet - a deliberate tradeoff for dmip having zero knowledge of
interfaces/routing/ARP (see `send_v4_fragment()` in `src/dmip.c`).
`dmip_v4_get_source_address()` itself is now a thin call to
`dmnetbridge_get_source_address()`.

Receiving is push-based, not polled, and dispatched by protocol number -
there is no generic "give me the next packet" pull API anymore
(`dmip_v4_receive()`/`_v6_receive()`/`_receive()` were removed). dmip
implements dmnetbridge's `packet_received` DIF
(`dmip_dmnetbridge_packet_received()` in `src/dmip.c`, following the
naming dmod's DIF macros generate) - called by whichever thread is
pumping *any* interface (`dmnetbridge_handle_netif_rx()`, run by the
`networkd` service), for every frame it reads. The implementation checks
the Ethertype, strips the 14-byte L2 header, and feeds the rest through
`dmip_v4_reassemble()`/`_v6_reassemble()` exactly as before; a completed
packet's header is then parsed once more purely to read its protocol
number (IPv4's `protocol` field / IPv6's `next_header`), and
`dispatch_packet()` hands it to whichever loaded module's
`dmip_protocol_numbers()` DIF implementation claims that number, one that
claims `DMIP_PROTO_DEFAULT` if none matches, or drops it if neither
exists - see "Protocol dispatch" below.

There is no `dmip_v6_send()`: resolving a destination MAC for IPv6 uses
NDP (RFC 4861), not ARP, and there is no NDP module in this tree yet -
the same boundary `dmarp.h` documents for itself regarding IPv6. Once an
NDP module exists, `dmip_v6_send()` can be added following the exact shape
of `dmip_v4_send()` (via `dmnetbridge_send()`, same as IPv4).

## Family-agnostic `dmip_send()`

A caller already has to say which family it means once - by populating
either `dmip_v4_header_t` or `dmip_v6_header_t` (they don't share a field
layout, so there's no way around picking one) - so making it *also* pick
which function to call (`dmip_v4_send()` vs. a `dmip_v6_send()`) on top of
that is redundant. `dmip_send()` takes a `dmip_header_t` (the same two
header structs behind a `family` tag, needed because the structs don't
share a common initial sequence so the tag can't be inferred safely) and
dispatches to `dmip_v4_send()` itself, or `-ENOSYS` for `dmip_family_v6`
until `dmip_v6_send()` exists. There is no equivalent `dmip_receive()`
anymore - see "Protocol dispatch" below for why.

## Protocol dispatch

Before this, every completed packet (any protocol) was pushed onto one
shared queue, and `dmip_v4_receive()`/`_v6_receive()`/`_receive()` popped
from it filtering only by *family* - never by protocol. That was a real
bug waiting to happen: with two protocol consumers (say `dmudp` and a
future `dmtcp`), whichever one's receive call happened to be waiting when
a packet arrived could get a packet meant for the *other* protocol, which
it would then discard (`dmudp_receive()` used to do exactly this - parse
the packet, see `protocol != DMIP_PROTO_UDP`, free it and return
`-EPROTO`). That's silent, permanent data loss for whoever actually
wanted it, and it only gets worse as more protocols are added.

The first fix for that was a private dispatch table: `dmip_register_protocol(
protocol, handler)` made `handler` the sole recipient of packets naming that
protocol number, and dmip held a `Dmod_BeginUsage()` reference on the
registrant for as long as the registration stood - the same shape `dmvfs`
uses for a mounted filesystem module - since a registrant like `dmicmp` owns
no thread or process of its own and nothing else would keep it resident.
That table itself turned out to be its own problem: `dmicmp` couldn't be
restarted without first unregistering (which needs `dmip` itself to still be
working), `dmip` in turn couldn't be reloaded while it held a usage
reference on `dmicmp`, and if a registrant ever crashed instead of cleanly
calling `dmip_unregister_protocol()`, dmip would keep calling a now-stale
function pointer with nothing telling it otherwise.

The table is gone now. A module claims a protocol by implementing two DIFs
(see `include/dmip.h`'s "Protocol handler DIF" section) instead of calling
a registration function: `dmip_protocol_numbers(out_protocols,
max_protocols)` reports which `DMIP_PROTO_*` number(s) (and/or the
`DMIP_PROTO_DEFAULT` fallback marker) it currently wants, and
`dmip_protocol_receive(family, iface, packet, packet_len)` is called with a
matching packet. `dispatch_packet()` in `src/dmip.c` discovers implementors
fresh on every single packet via `Dmod_GetNextDifModule()`/
`_GetDifFunction()` (the same discovery dmip's own `packet_received` DIF
implementation is itself found through) rather than consulting anything it
owns - an exact protocol claim always wins over a `DMIP_PROTO_DEFAULT`
claim, and the packet is dropped if nobody claims either. Because nothing
is stored, nothing needs unregistering: a crashed, disabled, or unloaded
module simply stops being discovered, and `dmip` itself holds no reference
on anyone, so the two modules no longer block each other's reload. `dmicmp`
is the one module claiming a fallback today - it claims `DMIP_PROTO_ICMP`,
`DMIP_PROTO_ICMPV6`, and `DMIP_PROTO_DEFAULT` all from one implementation,
to answer any otherwise-unclaimed packet with an ICMP Destination
Unreachable. `dmudp`/`dmtcp` each claim exactly one protocol number. This
still mirrors how real IP stacks dispatch by protocol number (Linux's
`inet_add_protocol()` table, BSD's `protosw`) - dmip was already a
dispatcher one level up (by Ethertype, via the `packet_received` DIF); this
is the same idea one level further in, just discovered instead of
registered.

The accepted tradeoff: dispatching a packet now costs a scan over every
loaded module implementing `dmip_protocol_numbers()` instead of one table
lookup. Nothing in this tree implements more than a handful of protocol
handlers today, so this is not a concern in practice - if it ever becomes
one, the discovery results can be cached without changing this DIF-based
design.

An implementation receives a **borrowed** `packet` pointer, valid only for
the duration of the call (same contract `dmnetbridge.h`'s `packet_received`
DIF already documents for its own `frame` parameter) - `dispatch_packet()`
frees it right after the matched implementation (or none) returns, so an
implementation that wants to keep data past the call must copy it out
itself.

`dmip_for_each_protocol(callback, user_data)` enumerates the same DIF
implementors for introspection - one call to `callback` per (module,
protocol) pair currently claimed (never `DMIP_PROTO_DEFAULT`, which has no
protocol number of its own), with the claiming module's name. `tools/
lsproto` is a small DMOD application module built on top of it - see its
own README for how to run it.

## Byte buffers, not packed structs

Like `lib/dmarp/src/dmarp.c` and `tools/ip/src/ip.c`, headers are
built/parsed as raw `uint8_t` buffers indexed by hand, not packed C
structs - dmod's minimal module runtime gives no guarantee about struct
packing across targets, and a mismatched `dmip_v4_header_t` on the wire
would corrupt every packet silently.

## Dependencies

- `dmroute` - header-only: the address type (re-exported as `dmip_addr_t`).
  dmip.c itself never calls a `dmroute_*` function anymore - that moved to
  dmnetbridge
- `dmnetif` - header-only: `dmnetif_iface_t`, passed through to a
  registered protocol handler. dmip.c never calls a `dmnetif_*` function
  directly anymore either
- `dmnetbridge` - `dmnetbridge_send()`/`_get_source_address()`/`_get_mtu()`
  for `dmip_v4_send()`, and the `packet_received` DIF dmip implements for
  receiving
- `dmlist` - fragment reassembly bookkeeping
- `dmosi` - mutexes guarding reassembly/identification state, plus
  `dmosi_get_tick_count()` for reassembly timeouts
