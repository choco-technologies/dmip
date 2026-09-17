# lsproto

[![License](https://img.shields.io/badge/license-MIT-blue.svg)](../../LICENSE)

`lsproto` is a small DMOD application module: it lists every IP
protocol/next-header number currently registered with `dmip` via
`dmip_register_protocol()`, using `dmip`'s own
`dmip_for_each_protocol()` (see
[../../docs/api-reference.md](../../docs/api-reference.md)).

## Building

Built automatically as part of `dmip`'s own CMake configure (the repo-root
`CMakeLists.txt` calls `add_subdirectory(tools/lsproto)`), producing a
second, independently loadable/releasable module alongside `dmip` itself:

```bash
mkdir -p build
cd build
cmake ..
cmake --build . --target lsproto
```

Pass `-DDMOD_DIR=/path/to/local/dmod` to build against a local dmod checkout
instead of fetching `develop` from GitHub.

## Usage

Prints one line per registered protocol:

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
tools/lsproto/
├── docs/
│   ├── README.md
│   └── api-reference.md
├── include/            # No public header of its own - kept for the
│                        # auto-generated lsproto_defs.h
├── src/
│   └── lsproto.c
├── CMakeLists.txt
└── lsproto.dmr
```

## Author

Patryk Kubiak

## License

MIT
