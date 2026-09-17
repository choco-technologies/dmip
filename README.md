# dmip

[![License](https://img.shields.io/badge/license-MIT-blue.svg)](../../LICENSE)

`dmip` is the DMOD IP layer: building/parsing IPv4 and IPv6 headers, the
IPv4 header checksum, TTL / Hop-Limit handling, identification generation,
fragmentation/reassembly for both families, protocol dispatch for inbound
packets, and sending outbound packets (route lookup, ARP resolution and
frame I/O are delegated to [dmnetbridge](../dmnetbridge)). `dmip_addr_t` -
the address type every module in this tree uses - is re-exported here from
[dmroute](../dmroute), which owns the real definition.

## Architecture

`dmip` sits between upper-layer protocol modules and `dmnetbridge`, which
is itself the layer over routing, ARP and the raw network interfaces:

```
        dmudp, (future) dmtcp, dmicmp, ...
                     |    ^
   dmip_v4_send()/   |    | dmip_register_protocol(proto, handler)
   dmip_send()        |    | dmip_register_default_protocol(handler)
                     v    |
              +-----------------+
              |      dmip       |   <- this module
              |  header build/  |
              |  parse, checksum|
              |  TTL/hop-limit  |
              |  fragmentation  |
              |  reassembly     |
              |  protocol       |
              |  dispatch       |
              +-----------------+
                     |    ^
  dmnetbridge_send()  |    | packet_received DIF
  _get_source_address()    | (dmip implements it)
  _get_mtu()           |    |
                     v    |
              +-----------------+
              |   dmnetbridge   |   routing (dmroute) + ARP (dmarp)
              +-----------------+   + frame I/O (dmnetif)
                     |    ^
                     v    |
              +-----------------+
              |     dmnetif     |   network interface drivers
              +-----------------+
```

Key architectural decisions:

- **No routing/ARP/frame I/O inside dmip.** That used to live here; it now
  lives in `dmnetbridge`, so `dmip` only ever deals with IP-address-level
  concepts (headers, checksums, fragments) and never touches a route table,
  an ARP cache or a driver directly. `dmip_v4_send()` is a thin
  orchestrator: fill in the source address if missing (via
  `dmnetbridge_get_source_address()`), read the egress MTU (via
  `dmnetbridge_get_mtu()`), fragment the payload, and hand each fragment to
  `dmnetbridge_send()`.
- **Receiving is push-based, dispatched by protocol number.** There is no
  polling "give me the next packet" API. `dmip` implements `dmnetbridge`'s
  `packet_received` DIF (called by whichever thread is pumping a network
  interface) and, once a packet is fully reassembled, dispatches it to
  whichever module registered for its protocol number
  (`dmip_register_protocol()`), a registered default handler, or drops it
  if nobody claimed it. This mirrors how real IP stacks dispatch inbound
  traffic (Linux's `inet_add_protocol()` table, BSD's `protosw`) and
  guarantees a packet can never be silently handed to the wrong consumer.
- **Address type is re-exported, not owned.** `dmip_addr_t`/`dmip_family_t`
  are `dmroute_addr_t`/`dmroute_family_t` re-typedef'd. `dmroute` is the
  base of this module graph (no dependencies of its own); if `dmip` owned
  the address type instead, `dmroute` (needed for route lookups) and `dmip`
  would depend on each other, a cycle. See [docs/dmip.md](docs/dmip.md) for
  the full history.
- **Fragmentation is stateless, reassembly is not.** `dmip_v4_fragment()` /
  `dmip_v6_fragment()` are pure functions over a payload + MTU, emitting
  each resulting packet via a callback with no state kept between calls.
  `dmip_v4_reassemble()` / `dmip_v6_reassemble()` maintain one system-wide
  reassembly table (keyed by source/destination/protocol/identification),
  guarded by a mutex, with incomplete entries expired after
  `DMIP_REASSEMBLY_TIMEOUT_MS` (30 s).
- **Wire format is hand-indexed byte buffers, not packed structs.** DMOD's
  minimal module runtime makes no guarantee about struct packing across
  targets, so headers are built/parsed byte-by-byte (same approach as
  `dmarp` and `tools/ip`) rather than risking silent corruption from a
  compiler-dependent struct layout.
- **No `dmip_v6_send()` yet.** Resolving a destination MAC for IPv6 needs
  NDP (RFC 4861), and there is no NDP module in this tree yet. Everything
  up to that point (header build, fragmentation) already works for IPv6;
  only the actual transmit path is missing.

See [docs/dmip.md](docs/dmip.md) for the full rationale behind each of
these decisions.

## Dependencies

| Module | Why |
|--------|-----|
| [dmroute](../dmroute) | Header-only: the address type (re-exported as `dmip_addr_t`/`dmip_family_t`) |
| [dmnetif](../dmnetif) | Header-only: `dmnetif_iface_t`, passed through to registered protocol handlers |
| [dmnetbridge](../dmnetbridge) | `dmnetbridge_send()` / `_get_source_address()` / `_get_mtu()` for sending; `dmip` implements its `packet_received` DIF for receiving |
| [dmlist](../dmlist) | Fragment reassembly bookkeeping and the protocol dispatch table |
| [dmosi](../dmosi) | Mutexes guarding reassembly/identification/dispatch state, plus tick counts for reassembly timeouts |

## Building

This module lives under `lib/dmip` inside the `dmnet` repository and is
built as part of the parent's CMake configure (the top-level
`CMakeLists.txt` calls `add_subdirectory(lib)`, whose own `CMakeLists.txt`
calls `add_subdirectory(dmip)` after `dmroute`/`dmnetif`/`dmarp`/
`dmnetbridge`, the modules it depends on) - it is not built standalone.

```bash
mkdir -p build
cd build
cmake ..
cmake --build . --target dmip
```

Pass `-DDMOD_DIR=/path/to/local/dmod` to build against a local dmod checkout
instead of fetching `develop` from GitHub.

### Running the tests

```bash
cmake --build . --target test_dmip
```

`tests/dmip_test.c` covers the address type, the RFC 1071 checksum, IPv4/
IPv6 header build/parse round trips, TTL/Hop-Limit decrement, identification
counters, fragmentation/reassembly for both families (including
out-of-order delivery and the "already whole" passthrough path), and send/
receive. Since no real driver backs the test fixtures, send tests exercise
everything up to (but not including) the final `dmnetif_send()`, and
receive tests drive `dmip`'s `packet_received` DIF implementation directly.

## Usage

### Building and sending an IPv4 packet

```c
#include "dmip.h"

dmip_v4_header_t header = {
    .ttl = DMIP_DEFAULT_TTL,
    .protocol = DMIP_PROTO_UDP,
    .identification = dmip_v4_next_identification(),
    .dst = peer_addr,          /* header.src left dmip_family_none: */
};                              /* dmip_v4_send() fills it in via routing */

int result = dmip_v4_send(&header, payload, payload_len, DMARP_DEFAULT_TIMEOUT_MS);
if (result != 0)
{
    /* -ENETUNREACH: no route, -EHOSTUNREACH: ARP failed, ... */
}
```

`dmip_v4_send()` fragments the payload internally if it doesn't fit the
egress interface's MTU - there's no separate fragmentation step to call for
a normal send.

### Building a raw header (no send)

```c
#include "dmip.h"

dmip_v4_header_t header = {
    .ttl = DMIP_DEFAULT_TTL,
    .protocol = DMIP_PROTO_UDP,
    .identification = dmip_v4_next_identification(),
    .src = my_addr,
    .dst = peer_addr,
};

uint8_t packet[DMIP_V4_HEADER_LEN + sizeof(payload)];
header.total_length = sizeof(packet);
dmip_v4_build_header(packet, sizeof(packet), &header);
memcpy(packet + DMIP_V4_HEADER_LEN, payload, sizeof(payload));
```

### Receiving: registering a protocol handler

There is no polling receive call - register a handler once (typically from
`dmod_init()`), and it is invoked for every completed packet matching that
protocol number:

```c
#include "dmip.h"

static void on_udp_packet(dmip_family_t family, dmnetif_iface_t iface,
                           const uint8_t* packet, size_t packet_len)
{
    /* `packet` is only valid for the duration of this call - copy out
     * anything you need to keep past it. */
    handle_udp_datagram(family, packet, packet_len);
}

int dmod_init(void)
{
    return dmip_register_protocol(DMIP_PROTO_UDP, on_udp_packet);
}

void dmod_deinit(void)
{
    dmip_unregister_protocol(DMIP_PROTO_UDP);
}
```

A single `dmip_register_default_protocol()` handler can be registered to
catch any protocol nobody else claimed.

### Sending on a specific interface, bypassing routing

Useful for traffic that must go out a specific interface before a route
exists yet (e.g. a DHCP client's initial broadcast):

```c
dmip_v4_header_t header = {
    .protocol = DMIP_PROTO_UDP,
    .src = (dmip_addr_t){ .family = dmip_family_v4 },   /* 0.0.0.0 */
    .dst = broadcast_addr,
};

dmip_v4_send_on_iface(iface, &header, payload, payload_len, DMARP_DEFAULT_TIMEOUT_MS);
```

### Family-agnostic send

```c
dmip_header_t header = {
    .family = dmip_family_v4,
    .header.v4 = { .protocol = DMIP_PROTO_UDP, .dst = peer_addr, /* ... */ },
};

dmip_send(&header, payload, payload_len, DMARP_DEFAULT_TIMEOUT_MS);
```

### Fragmenting a payload manually

```c
static void emit_fragment(const uint8_t* fragment, size_t fragment_len, void* user_data)
{
    /* `fragment` is only valid for the duration of this call */
    my_transmit(fragment, fragment_len);
}

dmip_v4_header_t header = {
    .ttl = DMIP_DEFAULT_TTL,
    .protocol = DMIP_PROTO_UDP,
    .identification = dmip_v4_next_identification(),
    .src = my_addr,
    .dst = peer_addr,
};

dmip_v4_fragment(&header, payload, payload_len, mtu, emit_fragment, NULL);
```

## API

Full parameter/return documentation lives in [include/dmip.h](include/dmip.h)
and [docs/api-reference.md](docs/api-reference.md); this is a quick summary.

### Checksum

| Function | Description |
|----------|--------------|
| `dmip_checksum(data, length)` | RFC 1071 Internet checksum |

### IPv4

| Function | Description |
|----------|--------------|
| `dmip_v4_build_header(buffer, buffer_len, header)` | Build a 20-byte IPv4 header, checksum included |
| `dmip_v4_parse_header(buffer, length, header, header_len)` | Parse an IPv4 header (tolerates options, reports real header length) |
| `dmip_v4_checksum_valid(buffer, length)` | Verify a received IPv4 header's checksum |
| `dmip_v4_decrement_ttl(buffer, length)` | Decrement TTL in place, fixing up the checksum |
| `dmip_v4_next_identification(void)` | Next value from the system-wide IPv4 identification counter |
| `dmip_v4_fragment(header, payload, payload_len, mtu, callback, user_data)` | Split a payload into MTU-sized IPv4 packets |
| `dmip_v4_reassemble(fragment, length, out_packet, out_length)` | Feed one received IPv4 packet through reassembly |
| `dmip_v4_get_source_address(dst, out_src)` | Source address `dmip_v4_send()` would use to reach `dst` |
| `dmip_v4_send(header, payload, payload_len, arp_timeout_ms)` | Build, fragment (if needed) and transmit a complete IPv4 packet via routing |
| `dmip_v4_send_on_iface(iface, header, payload, payload_len, arp_timeout_ms)` | Same, but on an explicit interface, bypassing routing |

### IPv6

| Function | Description |
|----------|--------------|
| `dmip_v6_build_header(buffer, buffer_len, header)` | Build a 40-byte IPv6 fixed header |
| `dmip_v6_parse_header(buffer, length, header)` | Parse an IPv6 fixed header |
| `dmip_v6_decrement_hop_limit(buffer, length)` | Decrement Hop Limit in place |
| `dmip_v6_next_identification(void)` | Next value from the system-wide IPv6 fragment identification counter |
| `dmip_v6_fragment(header, payload, payload_len, mtu, identification, callback, user_data)` | Split a payload into MTU-sized IPv6 packets, adding a Fragment header if needed |
| `dmip_v6_reassemble(fragment, length, out_packet, out_length)` | Feed one received IPv6 packet through reassembly |

No `dmip_v6_send()` yet - see [docs/dmip.md](docs/dmip.md#send--receive).

### Family-agnostic

| Function | Description |
|----------|--------------|
| `dmip_send(header, payload, payload_len, arp_timeout_ms)` | Dispatches to `dmip_v4_send()` for `dmip_family_v4`; `-ENOSYS` for `dmip_family_v6` |

### Protocol registration

| Function | Description |
|----------|--------------|
| `dmip_register_protocol(protocol, handler)` | Register the sole receiver for a given IP protocol/next-header number |
| `dmip_unregister_protocol(protocol)` | Undo the above (no-op if unregistered) |
| `dmip_register_default_protocol(handler)` | Register a fallback for any protocol with no specific registrant |
| `dmip_unregister_default_protocol(void)` | Undo the above (no-op if unregistered) |
| `dmip_list_registered_protocols(callback, user_data)` | Enumerate every protocol number currently claimed via `dmip_register_protocol()` |

### Well-known protocol numbers

`DMIP_PROTO_ICMP` (1), `DMIP_PROTO_TCP` (6), `DMIP_PROTO_UDP` (17),
`DMIP_PROTO_IPV6_FRAGMENT` (44), `DMIP_PROTO_ICMPV6` (58).

## Tools

### `dmip_protocols`

A small companion DMOD application module, in [tools/protocols](tools/protocols),
that lists every IP protocol number currently registered via
`dmip_register_protocol()` (built on `dmip_list_registered_protocols()`
above). Built alongside `dmip` by this repo's own `CMakeLists.txt` and
released as its own package - see [tools/protocols/README.md](tools/protocols/README.md).

## Documentation

See the `docs/` directory:

- **[dmip.md](docs/dmip.md)** - Overview and design rationale
- **[api-reference.md](docs/api-reference.md)** - Full API reference

View documentation using `dmf-man dmip`.

## Project Structure

```
dmip/
├── docs/              # Documentation (markdown format)
│   ├── README.md
│   ├── dmip.md
│   └── api-reference.md
├── include/           # Public headers
│   └── dmip.h
├── src/
│   └── dmip.c
├── tests/
│   ├── CMakeLists.txt
│   └── dmip_test.c
├── tools/
│   └── protocols/     # dmip_protocols application module - see its own README
├── CMakeLists.txt
├── Makefile
└── dmip.dmr
```

LICENSE is shared with the rest of the `dmnet` repository (`../../LICENSE`) -
see `dmip.dmr` for how it's picked up during packaging.

## Author

Patryk Kubiak

## License

MIT
