# Uzebox on RP2350

Uzebox cartridge runtime based on the tested 0.6.26 AA core. The public
configuration targets Adafruit Fruit Jam with 8 MiB PSRAM, SD, HDMI
video/audio and USB HID input. The build runs the RP2350 at 252 MHz and
HSTX at 126 MHz. Supply your own `.uze` files in `/roms/UZEBOX` on the SD
card. The native NES/SNES adapter is disabled in this configuration.

The 0.6.26 AA baseline passed scoped FPS and original-game audio acceptance
on November 4, 2026. A prior MusicDemo capture recorded 2,100 frames in
35.000018 seconds (59.999969 FPS), with zero new audio underruns or zero
padding. That capture used an RP2350B at 378 MHz. Physical Fruit Jam
gameplay, audio continuity, performance and long runs remain unverified.

## Build

Install Git, CMake 3.20+, Ninja, Python 3 and Arm GNU Toolchain 14.2.Rel1.
`dependencies.lock.json` pins Pico SDK 2.3.0, TinyUSB and Pico-PIO-USB.

```powershell
./scripts/fetch-dependencies.ps1 -Destination <outside-source-directory>
./scripts/build.ps1 -PicoSdkPath <pico-sdk> -PicoPioUsbPath <Pico-PIO-USB> `
  -ToolchainPath <arm-toolchain> -NinjaPath <ninja.exe>
```

For offline builds, pass `-PioasmDir`, `-PicotoolDir` and optionally
`-CmakeExe` and `-PythonExe`. The output is
`build/fruit-jam/picoUzeboxCartridgeRuntime.uf2`, linked for a compatible
16 MiB flash loader with an application base of `0x10080000`.

The AA hot loops use ARM Thumb-2. See [VALIDATION.md](VALIDATION.md)
for the scope of the public build and cartridge parser checks.

## Source

The core derives from [CUzeBox](https://github.com/Jubatian/cuzebox)
at `adcea412e18cca8a4bb9e94af94097a420c2f5cc` under GPL-3.0-or-later.
The combined source distribution uses GPLv3, with individual notices
retained. See [THIRD_PARTY.md](THIRD_PARTY.md) for source layout, licenses and
local changes. Game files and firmware binaries are excluded.
