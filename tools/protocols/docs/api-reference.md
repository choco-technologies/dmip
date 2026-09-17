# dmip_protocols API Reference

`dmip_protocols` is loaded and executed through the DMOD loader - it does
not expose a callable module API of its own (see
[../../../docs/api-reference.md](../../../docs/api-reference.md) for the
`dmip_list_registered_protocols()` API it's built on).

## Usage

```bash
dmod_loader dmip_protocols.dmf
```

Prints one line per protocol number currently registered with `dmip` via
`dmip_register_protocol()` - `<number>  <name>` for a well-known
`DMIP_PROTO_*` value (ICMP, TCP, UDP, IPv6-Fragment, ICMPv6), `<number>
unknown` otherwise. Prints `(none)` if nothing is registered.

## Arguments

_(none)_

## Exit codes

| Code | Meaning |
|------|---------|
| 0    | Always - listing an empty registration table is not an error |
