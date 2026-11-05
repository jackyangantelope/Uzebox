# Release validation

| Check | Result |
|---|---|
| AA baseline physical acceptance | User confirmed FPS and original audio on November 4, 2026. Prior MusicDemo capture measured 59.999969 FPS over 2,100 frames with zero new audio underruns or zero padding. |
| Public Fruit Jam build | Independent clean clone built successfully. Flash 426,516 bytes and main RAM 505,072 bytes (96.33%) at link time. |
| Cartridge parsing | MusicDemo, Maze, GameOfLife and ControllerTester accepted as external UZE inputs. Bad magic, short headers and truncated payloads rejected for each input. |
| Source release audit | No blocking findings. License notices and pinned dependency revisions included. |
| Fruit Jam physical test | Pending. The 252 MHz configuration has different clocking and board integration from the tested 378 MHz baseline. Full speed, runtime memory headroom, gameplay and audio continuity require physical checks. |

The clean clone reproduced UF2 SHA-256
`cb2cffacfbb9a13f8a57c441f00d98d412a6025cbb6e9014410d4bd659655aea`.
Only documentation and host test tooling changed after that build.
