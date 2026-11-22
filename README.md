# Astra CPS1 for Old Nintendo 3DS

A native Capcom Play System 1 arcade emulator for the **original Nintendo 3DS**, distributed as a Homebrew Launcher application. The top screen shows the game. The touch screen handles the ROM library, controls, arcade DIP switches, frame pacing, sound and performance counters.

**[Download v0.6b — English / Français](https://github.com/SerialZ61/Astra-CPS1/releases/download/v0.6b/Astra-CPS1-v0.6b-public.zip)** · **[Download the v0.6a forum ZIP](https://github.com/SerialZ61/Astra-CPS1/releases/download/v0.6a/Astra-CPS1-v0.6a-public.zip)** · [All releases](https://github.com/SerialZ61/Astra-CPS1/releases) · [Français](LISEZ-MOI.md)

## Install and play

1. Extract the `3ds` folder from the public release ZIP onto the root of your SD card.
2. Put your own CPS1 ZIP files in `/3ds/AstraCPS1/roms/`. Leave them compressed. For a split clone, put its parent ZIP beside it.
3. Launch **Astra CPS1** from the Homebrew Launcher. One ZIP starts automatically. With several ZIPs, use the D-pad and A or the touch screen.
4. SELECT inserts a coin. START starts the game. Open **CONFIG / RÉGLER** to remap the buttons and set the cabinet's DIP switches. Settings are saved per set. SELECT + START returns to the library. L + R + START exits.

**Choosing a language in v0.6b.** In the library press **Y** or tap **Francais / English**. During play, open **CONFIG / RÉGLER** and select **Language / Langue**. The selection is saved on the SD and works even when a single ROM starts automatically. A French system starts in French. Other system languages start in English. Cabinet games retain the language of their ROM revision.

The original v0.6a interface is in French. **CADENCE** cycles through automatic pacing, a wall-clock target of 29.8 displayed frames/s, every other arcade frame, and every frame. **FORMAT** changes the aspect ratio. For vertical games it also rotates the controls and the picture together. Tap **SON CONTINU** to switch between adaptive and original audio. [English controls and troubleshooting](docs/GUIDE.en.md)

## Compatibility

The v0.6a test run booted 136 archives for 420 emulated frames each. The catalogue contains 141 regional, parent and clone entries. For *Final Fight* and *1941*, SerialZ also reported good results on a physical Old 3DS. *Three Wonders* remains an open hardware report. Some PIC-based bootlegs lack sound. [Per-archive results](out/v06/card-collection.json) · [Development history](HISTORY.md)

## Source and build

Install devkitPro's 3DS toolchain with devkitARM, libctru, citro3d, picasso, 3DS zlib, `smdhtool` and `3dsxtool`. Run `make -j4` in the devkitPro shell at the repository root. Windows users can run `.\build.ps1` in PowerShell. The executable and the unstripped ELF appear in `out/`.

| Directory | What you will find |
|---|---|
| `app/` | Touch interface, display, input profiles, frame pacing and DSP audio |
| `engine/` | Driver selection, ZIP loader and verified fast paths |
| `vendor/` | Sources of both emulation cores as used by this port |
| `tests/` | ARM emulation, input, audio and graphics regression tools |
| `notices/` | Third-party credits and original licence texts |

**SerialZ** directed the project and tested it on hardware. **Codex (OpenAI)** worked on the port, optimisation, packaging and validation. The arcade cores build on Final Burn, FB Alpha, MAME, FBA2X and PiFBA. The public packages include code and documentation. Game files are supplied by players. [Credits and redistribution](notices/README.md) · [Release history in French](HISTORIQUE-ASTRA-CPS1.md)
