# Source attribution

| Component | Source and license | Changes |
|---|---|---|
| CUzeBox | [Jubatian/cuzebox](https://github.com/Jubatian/cuzebox) at `adcea412e18cca8a4bb9e94af94097a420c2f5cc`, GPL-3.0-or-later, `deps/cuzebox/LICENSE` | Selected AVR state, controller and SPI helper sources in `deps/cuzebox/`. `generated/core/` contains the actual AA interpreter sources, with guarded AVR loop, tile, poll and fill execution, and static hot code placement. |
| Shared platform | [fhoedemakers/pico_shared](https://github.com/fhoedemakers/pico_shared) at `df683bf00ff95e1ceda88f132bbf72f8cc7cadb0`, GPL-3.0-only root | Public SD/FatFs and HDMI support in `deps/pico_shared/`. Adapted HDMI implementation in `generated/hstx.c` and `clock/video_output.c` retains bounded audio batching and uses the Fruit Jam clock divider and pin definitions. The FatFs and SD adapter notices remain in their original files. |
| Runtime and adapters | BSD-3-Clause for files carrying those notices, `LICENSE.BSD-3-Clause` | `runtime/` parses external UZE cartridges. `firmware/` contains menu, streaming, audio resampling and frame scheduling. `platform/` provides USB HID, SD and PSRAM integration. |
| SDK and USB | Public revisions in `dependencies.lock.json` | External SDK, TinyUSB and Pico-PIO-USB, fetched by the build setup script. |

The root GPLv3 text applies to the combined distribution. The interpreter
and shared driver copies include local modifications. The source release
retains the AA core and changes board setup, clocking and input integration.
