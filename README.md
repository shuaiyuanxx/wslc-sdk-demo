# WSLC SDK Demo

Demonstrates the complete developer workflow for WSL Containers: **building** a container image at compile time and **running** it at runtime via the WSLC SDK.

## What it shows

### Build time (PR #14551 — Build UX)
The `.vcxproj` declares a `<WslcImage>` item:
```xml
<WslcImage Include="echo-server"
           Dockerfile="$(ProjectDir)container\Dockerfile"
           Context="$(ProjectDir)container"
           Tag="latest" />
```
MSBuild automatically runs `wslc image build` after compilation. Incremental rebuild is supported — if the container sources haven't changed, the build step is skipped.

### Run time (PR #40141 — WslcGetCliSession)
The app connects to the wslc CLI session where the image was built, creates a container, runs it, and captures the output:
```
WslcGetCliSession()          → connect to CLI session
WslcInitContainerSettings()  → configure container from "echo-server:latest"
WslcCreateContainer()        → create the container
WslcStartContainer()         → run it and capture stdout/stderr via callbacks
```

---

## Testing on another machine

**TL;DR** — Check out the `user/shawn/buildUX-demo` branch of the WSL repo, install a WSL MSI built from the same branch, then build `test/wslc-sdk-demo` with MSBuild or Visual Studio. No `nuget install` needed — the SDK payload is committed in the branch.

### Prerequisites

- Windows 11
- Visual Studio 2022 with the **Desktop development with C++** workload (or standalone Build Tools + MSBuild)

### Step 1 — Get the branch

```bash
git clone https://github.com/microsoft/WSL.git
cd WSL
git checkout user/shawn/buildUX-demo
```

The demo relies on the repo layout — it imports the SDK targets via a relative path (`test/wslc-sdk-demo/WslcSdkDemo.vcxproj` → `../../nuget/Microsoft.WSL.Containers/...`). Do not copy the demo folder out of the repo.

### Step 2 — Install WSL from this branch

The demo needs **both**:
- `wslc.exe` at `C:\Program Files\WSL\wslc.exe` — build time runs `wslc image build`
- The `WslcGetCliSession` runtime API in `wslservice.exe` — app connects to the CLI session

Neither ships in stock WSL yet, so you must install a WSL MSI built from `user/shawn/buildUX-demo`. Please follow the normal WSL build instructions from the repo root, then install `msipackage/…*.msi`.

e.g. In your WSL root folder, for example `D:\WSL\`, build WSL with these commands:
```bash
mkdir build
cd build
cmake ..
cmake --build .
```
Then install the `wsl.msi`, which should be found in the output folder `WSL\build\bin\x64\Debug`

Verify:
```
"C:\Program Files\WSL\wslc.exe" --version
wsl --version
```

If `wslc --version` fails, the demo build will stop with error `WSLC0001`.

### Step 3 — Build the demo

From `WSL\test\wslc-sdk-demo`:

```bash
msbuild WslcSdkDemo.vcxproj /p:Configuration=Debug /p:Platform=x64
```

Or open `WslcSdkDemo.sln` in Visual Studio 2022 and hit Build.

What happens during the build:
1. `main.cpp` compiles and links against `wslcsdk.lib`.
2. MSBuild runs `wslc image build` for the `<WslcImage Include="echo-server" …>` declared in the `.vcxproj`, producing image `echo-server:latest` inside the default wslc CLI session.
3. `wslcsdk.dll` is copied next to `WslcSdkDemo.exe`.

Incremental rebuilds skip the image step if `container/**` is unchanged.
