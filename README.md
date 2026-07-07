# AI Agent Parametric EQ

Parametric equalizer VST3 for FL Studio and other DAWs.

## Windows / FL Studio first

This project is intended to be built on Windows first. The plugin format is VST3,
which FL Studio can scan from:

```text
C:\Program Files\Common Files\VST3
```

Prerequisites on Windows:

- Visual Studio 2022 with "Desktop development with C++".
- CMake 3.22 or newer.
- Git.
- Python 3.

Build and install for FL Studio:

```powershell
scripts\build-windows.ps1 -Install
```

If PowerShell script execution is restricted, use:

```cmd
scripts\build-windows.cmd -Install
```

Then open FL Studio:

1. Open `Options > Manage plugins`.
2. Make sure `C:\Program Files\Common Files\VST3` is in the plugin search paths.
3. Click `Find installed plugins`.
4. Load `AI Agent Parametric EQ` as an effect plugin.

Build without installing:

```powershell
scripts\build-windows.ps1
```

The script builds DSP tests, runs them, builds the VST3 target, and prints the
generated `.vst3` path.

## macOS development

macOS build is still supported for local development and UI smoke checks:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --target AIAgentParametricEQ_VST3
cmake --build build --config Release --target AIAgentParametricEQ_Standalone
```

## Features

- Six interactive bands.
- Bell, low shelf, high shelf, low-cut, high-cut, notch and band-pass shapes.
- Drag points across the analyzer to change frequency and gain.
- Mouse wheel over a point adjusts Q.
- Real-time output spectrum with the EQ response curve overlaid.
- VST3 build through JUCE/CMake.
