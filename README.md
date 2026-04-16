# WSLC SDK Demo

Demonstrates the complete developer workflow for WSL Containers: **building** a container image at compile time and **running** it at runtime via the WSLC SDK.

## What it shows

### Build time (PR #14551 — Build UX)
The `.vcxproj` declares a `<WslcImage>` item:
```xml
<WslcImage Include="demo-app"
           Dockerfile="$(ProjectDir)container\Dockerfile"
           Context="$(ProjectDir)container"
           Tag="latest" />
```
MSBuild automatically runs `wslc image build` after compilation. Incremental rebuild is supported — if the container sources haven't changed, the build step is skipped.

### Run time (PR #40141 — WslcGetCliSession)
The app connects to the wslc CLI session where the image was built, creates a container, runs it, and captures the output:
```
WslcGetCliSession()          → connect to CLI session
WslcInitContainerSettings()  → configure container from "demo-app:latest"
WslcCreateContainer()        → create the container
WslcStartContainer()         → run it and capture stdout/stderr via callbacks
```

## How to build & run

### Prerequisites
- Windows 11 with WSL installed (`wsl --install --no-distribution`)
- Visual Studio 2022 with C++ workload

### Build
```
msbuild WslcSdkDemo.vcxproj /p:Configuration=Debug /p:Platform=x64
```

Or open `WslcSdkDemo.sln` in Visual Studio and build.

The build will:
1. Compile `main.cpp`
2. Automatically build the `demo-app:latest` container image

### Run
```
x64\Debug\WslcSdkDemo.exe
```

Expected output:
```
========================================
  WSLC SDK Demo
  Build + Run container images
========================================

[1] Connecting to wslc CLI session...
    Connected!

[2] Creating container from 'demo-app:latest'...
    Container created.

[3] Starting container...

--- Container stdout ---
=== WSLC Container Demo ===
Hostname: <container-id>
OS:       "Alpine Linux v3.21"
Uptime:   N/A
==========================

--- Exit code: 0 ---

[4] Cleaning up...
    Done.
```
