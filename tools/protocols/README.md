# dmip_protocols

[![License](https://img.shields.io/badge/license-MIT-blue.svg)](../../LICENSE)

`dmip_protocols` is a small DMOD application module: it lists every IP
protocol/next-header number currently registered with `dmip` via
`dmip_register_protocol()`, using `dmip`'s own
`dmip_list_registered_protocols()` (see
[../../docs/api-reference.md](../../docs/api-reference.md)).

## Building

Built automatically as part of `dmip`'s own CMake configure (the repo-root
`CMakeLists.txt` calls `add_subdirectory(tools/protocols)`), producing a
second, independently loadable/releasable module alongside `dmip` itself:

```bash
mkdir -p build
cd build
cmake ..
cmake --build . --target dmip_protocols
```

Pass `-DDMOD_DIR=/path/to/local/dmod` to build against a local dmod checkout
instead of fetching `develop` from GitHub.

## Usage

Once loaded (e.g. by `dmod_loader`, or embedded in a `dmod-boot` package
alongside `dmip`), running it prints one line per registered protocol:

```bash
dmod_loader dmip_protocols.dmf
```

```
Registered IP protocols:
   17  UDP
    1  ICMP
```

With nothing registered:

```
Registered IP protocols:
  (none)
```

## Documentation

See [docs/api-reference.md](docs/api-reference.md) for command-line usage
and exit codes.

## Project Structure

```
tools/protocols/
├── docs/
│   ├── README.md
│   └── api-reference.md
├── include/            # No public header of its own - kept for the
│                        # auto-generated dmip_protocols_defs.h
├── src/
│   └── dmip_protocols.c
├── CMakeLists.txt
└── dmip_protocols.dmr
```

## Author

Patryk Kubiak

## License

MIT
