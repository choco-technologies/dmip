# lsproto API Reference

Prints one line per protocol number currently registered with `dmip` via
`dmip_register_protocol()` (see
[../../../docs/api-reference.md](../../../docs/api-reference.md) for the
`dmip_for_each_protocol()` API it's built on) - `<number>  <name>` for a
well-known `DMIP_PROTO_*` value (ICMP, TCP, UDP, IPv6-Fragment, ICMPv6),
`<number>  unknown` otherwise. Prints `(none)` if nothing is registered.

## Arguments

_(none)_

## Exit codes

| Code | Meaning |
|------|---------|
| 0    | Always - listing an empty registration table is not an error |
