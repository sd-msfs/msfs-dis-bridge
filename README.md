# MSFS DIS Bridge

&#x20;&#x20;

## Overview

This repository implements a **DIS Gateway bridge** that uses Microsoft Flight Simulator 2024 (MSFS 2024) as a terrain and vehicle renderer for DIS simulations. It:

- Parses incoming DIS‑6 PDUs (via the Open‑DIS C++ library)
- Maps DIS object types to SimConnect variables
- Writes data into MSFS using the SimConnect SDK
- Encodes and broadcasts PDUs back onto the network
- Exposes a REST API for runtime configuration and diagnostics

## Features

- **DIS‑6 Only**: Pruned to only support DIS Protocol Version 6
- **Multi‑threaded**: One thread per MSFS instance feed for low-latency processing
- **Configurable**: JSON‑based mapping profiles, editable via REST
- **Vendored SDKs**: MSFS 2024 and SimConnect SDKs under `extern/` for reproducible builds
- **Test Suites**: PDU replay and SimConnect mock tests for CI integration

## Repository Structure

```
msfs-dis-bridge/
├── extern/
│   ├── open-dis/            # Open-DIS DIS-6 code and examples
│   ├── SimConnect/          # Vendored SimConnect SDK (include + lib)
│   └── FlightSimSDK/        # Vendored MSFS 2024 SDK (include + lib)
├── scripts/                 # Helper scripts (add-submodule, build Docker)
├── src/
│   ├── bridge-core/         # Decode, encode, PDU mapping, threading
│   ├── simconnect-adapter/  # Wrappers around SimConnect API calls
│   └── rest-api/            # C++ REST service for config & diagnostics
├── tests/
│   ├── pdu-replay/          # Unit and integration tests for DIS handling
│   └── simconnect-mock/     # Mock SimConnect tests
├── .github/
│   └── workflows/ci.yml     # GitHub Actions CI pipeline
├── CMakeLists.txt           # Top-level build configuration
├── README.md                # You are here
└── LICENSE
```

## Prerequisites

- **CMake ≥ 3.15**
- **C++17** compatible compiler (e.g. MSVC 2019+, GCC 9+, Clang 10+)
- **Git** for cloning and submodules
- **MSFS 2024 SDK** and **SimConnect SDK** [Installation Guide](https://docs.flightsimulator.com/msfs2024/html/1_Introduction/SDK_Overview.htm?agt=index)


## Building

```bash
# Clone the repo
git clone https://github.com/John-Kilroy/MSFS-DIS-Gateway-.git
cd MSFS-DIS-Gateway-

# Initialize Open-DIS submodule if needed
git submodule update --init --recursive extern/open-dis
## Or, to initialize all submodules:
git submodule update --init --recursive

# Create a build directory
mkdir build && cd build

# Configure with CMake
cmake ..

# Build all targets
cmake --build . -- -j$(nproc)
```

By default, this will compile the **bridge core library**, the **SimConnect adapter**, the **REST service**, and run the test suites.

## Running

```bash
# From the build directory, run the REST API service:
./rest-api/rest-api-server --port 8080

# Launch one or more MSFS instances and start feeding PDUs over TCP:
#   …your DIS client setup…
```

Visit `http://localhost:8080` for diagnostics, mapping CRUD, and live stats.

## Mapping Profiles

Mapping profiles live under `mappings/` as JSON:

```json
{
  "EntityStatePdu": {
    "position": "SimConnect::DataDefineId::PlanePosition",
    "orientation": "SimConnect::DataDefineId::PlaneOrientation"
  }
}
```

Use the REST API to list, add, update, or delete profiles at runtime without restarting.

## Contributing

1. Fork the repo
2. Create a feature branch (`git checkout -b feature/YourFeature`)
3. Commit your changes with clear messages
4. Push to your fork and open a pull request

Please ensure tests pass and include new unit tests for any new functionality.

## License

This project is licensed under the **MIT License**. See [LICENSE](LICENSE) for details.

---

*Created by the MSFS DIS Bridge team for Senior Design*

