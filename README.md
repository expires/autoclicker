# Lunar AutoClicker

A DLL-based autoclicker for Lunar Client, designed for private server anticheat testing. Supports Lunar Client 1.21.4 (Mojang mappings) and 1.21.11 (Fabric intermediary mappings).

---

## Table of Contents

- [Architecture](#architecture)
- [Mapping System](#mapping-system)
- [Building](#building)
- [Usage](#usage)
- [Configuration](#configuration)

---

## Architecture

The project is split into two binaries: an injector executable and the payload DLL.

```
┌─────────────────────────────────────────────────────────┐
│  injector.exe                                           │
│                                                         │
│  1. Finds javaw.exe via CreateToolhelp32Snapshot        │
│  2. Allocates memory in the target process              │
│  3. Writes ac.dll path into that memory                 │
│  4. Spawns remote thread → LoadLibraryA(ac.dll)         │
└───────────────────────┬─────────────────────────────────┘
                        │ injects into
                        ▼
┌─────────────────────────────────────────────────────────┐
│  javaw.exe (Lunar Client)                               │
│                                                         │
│  ┌──────────────────────────────────────────────────┐   │
│  │  ac.dll                                          │   │
│  │                                                  │   │
│  │  DllMain                                         │   │
│  │    └─ spawns AutoclickerModule::init thread      │   │
│  │                                                  │   │
│  │  AutoclickerModule                               │   │
│  │    ├─ attaches to JVM via JNI_GetCreatedJavaVMs  │   │
│  │    ├─ enumerates loaded classes via JVMTI        │   │
│  │    ├─ reads ac_config.json for CPS               │   │
│  │    └─ runs click loop using Clicker              │   │
│  │                                                  │   │
│  │  SDK (JNI wrappers)                              │   │
│  │    ├─ Minecraft  ──▶ player, gui, screen,        │   │
│  │    │                 gameMode, hitResult          │   │
│  │    ├─ LivingEntity ▶ isUsingItem, getItemInHand  │   │
│  │    ├─ HitResult   ──▶ getType, getEntityHitResult│   │
│  │    └─ ...                                        │   │
│  │                                                  │   │
│  │  Clicker                                         │   │
│  │    ├─ randomDelay() — Gaussian timing per CPS    │   │
│  │    ├─ lclick() — down(30%) + up(70%) of cycle    │   │
│  │    └─ getClicksPerSecond() — sliding window      │   │
│  └──────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────┘
```

### Click loop logic

```
Every 50ms tick:
  Is Minecraft the active window AND left mouse held?
  ├─ END key pressed        → unload DLL
  ├─ Pause/ESC screen open  → skip
  ├─ HitResult type == 1 (block) AND not creative
  │     └─ hold mouse down, delay randomDelay(1.0) per iteration
  └─ HitResult type == 2 (entity) AND not using item
        └─ lclick()
```

### Timing model

Each click cycle targets `1000 / CPS` ms total, split into:

```
|── hold (30%) ──|──────── gap (70%) ────────|
     ~25ms @ 12 CPS          ~58ms @ 12 CPS
```

Both segments are sampled from a Gaussian distribution (stddev = 20% of mean) with a 1% chance of an extended pause to simulate natural human variance.

---

## Mapping System

Lunar Client obfuscates Minecraft class names. The mapping version is selected at compile time via `-DMAPPING_VERSION` and injected into `Mappings.h` from a JSON file.

```
mappings/
  mojang_1.21.4.json      ← Mojang mapped names  (net.minecraft.client.Minecraft, ...)
  fabric_1.21.11.json     ← Fabric intermediary  (net.minecraft.class_310, ...)

dll/SDK/Mappings.h.in     ← CMake template
      │
      │  configure_file() at build time
      ▼
build/dll/Mappings.h      ← generated, #define MC_Minecraft "net.minecraft.class_310"
```

To add a new version, create `mappings/<version>.json` matching the schema of an existing file, then build with `-DMAPPING_VERSION=<version>`.

---

## Building

### Requirements

- Windows, Visual Studio 2022+
- CMake 3.30+
- Java JDK 21 (`JAVA_HOME` must be set)

### Local build

```bat
cmake -S . -B build -DMAPPING_VERSION=fabric_1.21.11
cmake --build build --config Release
```

Outputs:
- `build/DLL/Release/DLL.dll`
- `build/INJECTOR/Release/INJECTOR.exe`

### GitHub Actions

Every push to `main` triggers a matrix build for all mapping versions. Artifacts are uploaded per version (`autoclicker-mojang_1.21.4`, `autoclicker-fabric_1.21.11`)

---

## Usage

1. Download the artifact for your Lunar Client version from the Actions tab or Releases.
2. Place `ac.dll`, `injector.exe`, and `ac_config.json` in the same folder.
3. Launch Lunar Client and join a world or server.
4. Run `injector.exe`. It will locate `javaw.exe` and inject the DLL automatically.
5. Hold left click in-game to activate. Press `END` to unload.

---

## Configuration

Edit `ac_config.json` before injecting:

```json
{
    "CPS": 10
}
```

| Key | Type | Range | Description |
|-----|------|-------|-------------|
| CPS | int  | 1–50  | Target clicks per second. Defaults to 12 if file is missing. |

---

## Notes

- Intended for use on private servers for anticheat development and testing only.
- Public servers with anticheat will likely detect or flag this.
