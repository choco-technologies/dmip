# lsproto API Reference

Prints one line per protocol number currently claimed by some loaded
module via `dmip`'s protocol handler DIF (see
[../../../docs/api-reference.md](../../../docs/api-reference.md) for the
`dmip_for_each_protocol()` API it's built on) - `<number>  <name>  <module>`
for a well-known `DMIP_PROTO_*` value (ICMP, TCP, UDP, IPv6-Fragment,
ICMPv6), `<number>  unknown  <module>` otherwise. Prints `(none)` if
nothing is claimed.

## Arguments

_(none)_

## Exit codes

| Code | Meaning |
|------|---------|
| 0    | Always - listing an empty registration table is not an error |
