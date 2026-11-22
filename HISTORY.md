# Astra CPS1 on the Old Nintendo 3DS — development history

**21–22 November 2026 · Reference release v0.6a**  
Project and hardware testing by **SerialZ**. Port, optimisation, test tools and releases developed with **Codex (OpenAI)**. [French journal and detailed references](HISTORIQUE-ASTRA-CPS1.md)

## Starting point

SerialZ wanted to play the original *Final Fight* arcade ZIP on a first-generation Nintendo 3DS using the Homebrew Launcher. On that particular console, he estimated that the RetroArch/FBA 2012 combination he had tried gave about seven displayed frames per second and broken sound. The first Astra build already gave him clean sound. Its counter showed just 14 displayed frames/s. The second build showed 29.8 displayed frames/s while the game itself executed 59.6 frames/s. That difference between **simulating the cabinet** and **drawing its latest image** shaped the rest of the project.

The 3DS already ran Luma3DS. devkitPro supplied the supported ARM11 user-space interfaces. The investigation focused on the arcade programs, their CPU and sound wait loops, the renderer and the work the Old 3DS actually had to do. PiFBA supplied tested CPS1 drivers and Cyclone, a 68000 interpreter written in ARM assembly. Astra replaced the Raspberry Pi presentation, audio and input paths with a native 3DS application. The main core builds for ARMv6K, tuned for MPCore, at `-O3`. The supplementary core builds at `-O2`. [Build rules](Makefile) · [Supplementary module](build/compat.mk)

## Build ledger

Times are local to Brussels and refer to the preserved release files. Sizes count the executable alone. The interim build labelled v0.5a preceded v0.5.

| Date and time | Version | Bytes | Main result |
|---|---|---:|---|
| 21 Nov 23:54 | v0.1 | 949,684 | *Final Fight* from its original ZIP, stereo output and performance counters |
| 22 Nov 00:22 | v0.2 | 984,004 | GPU presentation, 4:3 aspect ratio and cheaper rendering |
| 22 Nov 00:57 | v0.3 | 1,142,364 | Library, six-button support, per-set mappings and cabinet DIP switches |
| 22 Nov 01:24 | v0.4 | 1,154,908 | Targeted optimisations for *Street Fighter II: The World Warrior* |
| 22 Nov 10:04 | v0.5a, interim | 1,158,772 | Initial pass over the expanded collection |
| 22 Nov 10:43 | v0.5 | 1,920,488 | Supplementary drivers, vertical screens, reusable backgrounds and adaptive sound |
| 22 Nov 12:03 | v0.6, provisional | 1,933,320 | 30 fps presentation target, Auto audio improvements and additional fast paths |
| 22 Nov 12:19 | **v0.6a** | **1,929,152** | Original FM output restored on the supplementary drivers |

### v0.1 — one playable game

The first build identified the chips of *Final Fight* World and connected the cabinet CPUs, inputs, renderer and stereo mixer. An Azahar 2126.2 run using its Old 3DS profile inserted a coin, started Guy in Slum, moved, attacked, jumped and reached 7,000 points.

The 68000 checks a table of sixteen tasks even while some are asleep. In this program revision and Cyclone build, one complete pass over an idle entry consumes 98 emulated cycles. Astra recognises the exact wait-loop state, accounts for the cycles and updates `D0.W`, `D1.B` and `A0`. Runnable tasks, pending interrupts and short cycle budgets return to the ordinary interpreter. The test replayed 1,900 frames with the fast path enabled and disabled. Both runs produced identical rendered pixels, tracked registers, 64 KiB of work RAM and 702,582 stereo sample pairs. The shortcut handled 1,310,628 idle entries. SerialZ's console showed 93.7% arcade speed, 55.9 emulated frames/s, 14.0 displayed frames/s and no audio buffer events. [Wait-loop implementation](engine/ffight_idle.cpp) · [Initial test](out/validation/RESULTATS.md)

### v0.2 — spend less time drawing the same pixels

The PICA200 now rotated and scaled the 384 × 224 surface to a centred 320 × 240 picture on the 400 × 240 upper screen. The CPU still composed arcade layers and sprites, reusing coloured tiles and stable parts of the scenery. Z80 wait loops and silent YM2151 operators also became cheaper. Across eight sampled combat frames with drawing, counted ARM instruction slots fell from 29,505,578 to 10,643,551 (63.9%). Across eight without drawing, they fell by 43.0%. A 3,200-frame replay matched for pixels, sound, RAM and tracked registers. SerialZ measured 100% simulation speed and 29.8 displayed frames/s in Auto. He also saw occasional dips to about 22 in a crowded scene with every frame enabled. [Comparison](out/v02/validation/RESULTATS.md) · [GPU presentation](app/display.c)

### v0.3 — controls, library and timing

The library scans filenames at startup. At launch the loader identifies chips by size and CRC and checks nearby parent ZIPs for split clones. Touch settings allow sixteen physical inputs to be reassigned. Cabinet DIP choices come from the current driver and apply after an emulated reset. Button and DIP profiles persist separately for each set. Tests exercised 582 groups and 2,314 choices across 56 sets. Only *Final Fight* and *World Warrior* World 910522 were available for gameplay testing at this stage. The SF2 screen-flip test matched 106 rotated images pixel for pixel. [DIP verification](out/v03/validation/controls/verification.md)

Three presentation buffers let the CPU draw while the LCD was waiting. The queue retains the newest ready frame. At five different phase offsets in a controlled Azahar test, v0.2 submitted 45 to 75 out of 90 frames. The new path submitted all 90 at every offset. On the physical Old 3DS, SF2 could still drop to about 10–14 displayed frames/s despite near-full simulation speed. [v0.3 report](out/v03/validation/RESULTATS.md)

### v0.4 — why the stage mattered

In World 910522, Astra recognises a verified 68000 task loop and a Z80 sound-program wait loop. It reuses 16 × 16 background tiles in strips measured in map coordinates and applies per-scanline scrolling when copying them. Changed tiles and palettes cause regeneration. Selected FM instruments can be rendered in blocks while producing the original samples.

Three 3,200-frame scenarios matched v0.3 for the checked images, PCM, RAM and CPU state. On five drawn Ryu–Dhalsim samples the reduction in ARM work was 45.83%. On four Dhalsim–Chun-Li samples it was 11.71%. The *Final Fight* control changed by +0.10% across eight drawn samples. SerialZ's video showed that the Chinese market and Guile's airfield still needed work. He could also lose sight of a brief projectile between frames in Auto. [v0.4 results](out/v04/RESULTATS.md) · [Background renderer](engine/sf2_scroll.cpp)

### v0.5a and v0.5 — a second engine and a vertical screen

Early on, `1941` and `wonder3` could appear only as filenames because their revisions lacked a matching driver. Once it loaded, *1941* offered 3:4 portrait and two sideways positions, rotating both D-pad and Circle Pad input. SerialZ subsequently reported excellent results with every frame enabled. The programme used the upper screen for play and kept the lower screen available for controls.

The catalogue expanded to 115 PiFBA entries and 26 supplementary FB Alpha 2012 entries. Several are regional revisions or clones. The latter module carries a private Musashi 68000 and private globals. Its link step exposes one `astra_extra_api` entry point. One selected core emulates the active game. Tile-strip caching expanded to 8-, 16- and 32-pixel layers, observing palette changes, transparency and sprite priority. [Main drivers](engine/drivers.inc) · [Supplementary drivers](engine/compat/sets.inc)

On the interim build SerialZ saw regular animation but counted five empty-audio events in *Dai Makai-Mura*, 24 in *Knights of the Round* and 32 in *Magic Sword*. In v0.5, Continuous Sound made its first appearance in the every-frame mode. It adjusted the delivery of samples to the real-time DSP when emulation slowed. [Collection test](out/v05/verified-collection.json) · [Audio queue](out/v05/real-sound-queue.json)

### v0.6 and v0.6a — Auto audio and a fidelity regression

SerialZ found that moving back to Auto brought back the audio gaps. Adaptive sound was extended to all four pacing options. Nine buffers are available, four are primed initially and eight successive production intervals are averaged. When needed, two similar stereo fragments are overlapped to bridge a gap while limiting pitch change. Arcade Sound passes the original PCM through. Counters reset on mode changes, so a fresh comparison begins at zero. The new 30 fps option targets one presentation roughly every 33.5 ms of real time. [DSP queue](app/sound.c) · [Stereo overlap](app/stretch.c) · [Frame pacing](app/pacing.c)

Additional fast paths covered selected revisions of *Champion Edition*, *Street Fighter Zero* for CPS Changer and *Dynasty Wars*. The main-core task optimisation verifies the instruction pattern before activating it. The secondary core gained tile and palette reuse. Six drawn samples on `dynwaru.zip` fell from 25,998,328 to 18,250,717 counted ARM slots (29.8%), and six undrawn samples fell by 26.3%. The full 3,200-frame scenario matched for accumulated pixels, PCM and RAM. [Before](out/v06/before-dynwaru.json) · [Release](out/v06/card-dynwaru.json)

A longer comparison found altered audio from an FM optimisation in the secondary core. On `3wonderh` the first difference occurred at frame 620, beyond the 420-frame boot check. v0.6a restored the original FM routine in that core. All 26 supplementary drivers then matched the v0.5 baseline for PCM, work RAM and the checked 68000 state over 1,460 frames each, with images compared whenever drawn. [Final run](out/v06/all-final.log)

## What the results establish

The reference build booted **136 of 136 archives**, running 420 frames per archive. Four images were sampled for each set, and audio activity was recorded. This establishes that those files can boot and produce visible output. The simulated DSP also passed three 1,800-frame runs using audio from `dynwaru`, `wonder3` and `sfzch`. On actual hardware SerialZ reported good results on the games he tried, with **Three Wonders** still unresolved. The exact revision, mini-game and symptom remain unknown. Some bootlegs use unsupported PIC audio, and `wofh` can boot with missing samples. [Per-archive results](out/v06/card-collection.json) · [Audio tests](out/v06/card-audio.log) · [User guide](docs/GUIDE.en.md)

At full speed the target is 59.63 emulated frames/s, or roughly 16.77 ms per arcade tick. Stereo at 22,050 Hz yields 369 or 370 sample pairs per tick. Auto measures drawing and simulation-only costs. The 30 fps option follows real time. Every Other follows simulated ticks. Every Frame requests every draw even when the cabinet itself is running slowly. Reporting both emulation and display rates makes field results meaningful.

## Reproducing an investigation

Build the ELF using the instructions in the [README](README.md). Use the same ROM revision, character, stage, inputs and draw schedule on both builds. Compare rendered RGB565, raw stereo PCM, RAM and tracked registers. Count ARM instruction slots to locate savings on the host, then repeat the scene on an actual Old 3DS to measure speed, presentation and audible gaps. Use `--original-cpu` with the *Final Fight* ARM test to exercise ordinary Cyclone.

```sh
python -m pip install unicorn pyelftools pillow
python tests/arm_smoke.py out/AstraCPS1.elf Roms/ffight.zip out/test-ffight --frames 1900
python tests/collection_smoke.py out/AstraCPS1.elf Roms out/collection.json
python tests/sound_timing.py out/AstraCPS1.elf Roms/dynwaru.zip Roms/wonder3.zip Roms/sfzch.zip
```

**v0.6a fingerprint** · SHA-256 `eea2c3f7c971c60e6fea763245d6ff17f85fd7f0c32e5c52fdf19b62f55ea31b`.

**Credits.** Dave created Final Burn. Team FB Alpha and MAME contributors supplied arcade drivers and chip emulations. HeadOverHeels developed FBA2X 7.3, and Squid (Pudding) ported and tuned it for Raspberry Pi as PiFBA. Astra uses Cyclone by Dave and notaz, Musashi by Karl Stenerud, Z80 code by Juergen Buchmueller, CZ80 by Stéphane Dallongeville and YM2151 code by Jarek Burczynski. MiniZip is by Gilles Vollant and zlib by Jean-loup Gailly and Mark Adler. devkitPro and the 3DS homebrew community supplied the toolchain and platform libraries. [Upstream credits and original notices](notices/README.md) · [PiFBA revision](https://github.com/squidrpi/pifba/tree/9d3dd8476750adf29253c8922841b87ff36b930b) · [Supplementary core revision](https://github.com/libretro/fbalpha2012_cps1/tree/5542c1848ef81e92db311193b01dc349bc29d7cc)
