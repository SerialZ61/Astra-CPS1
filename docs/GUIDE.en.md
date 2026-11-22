# Playing on an Old 3DS

Use the `3ds` directory in the forum ZIP. Your emulator launches from `/3ds/AstraCPS1/AstraCPS1.3dsx`. The main ROM folder is `/3ds/AstraCPS1/roms/`. The library opens the selected ZIP on demand and checks chip sizes and CRCs. An unfamiliar ZIP filename can appear before the loader has identified a supported set. If the loader asks for a parent, keep the parent archive next to the clone. X rescans the card. Left and right change pages.

| In-game touch control in v0.6a | Purpose |
|---|---|
| PAUSE / REPRISE | Pause or resume the game and inspect the most recent counters. |
| CADENCE | Auto, approximately 30 presentations/s, every other emulated frame, or every frame. The arcade simulation targets 59.63 frames/s. |
| FORMAT | Arcade 4:3 or native size. Vertical games also offer portrait and two sideways positions with matching D-pad and Circle Pad directions. |
| RÉGLER | Remap a physical control or change the game's cabinet switches. Save button profiles to apply immediately. Saving changed DIP switches asks before restarting the cabinet. |
| JEUX | End the current session and return to the library. |
| SON CONTINU / SON ARCADE | Choose adaptive sound or direct arcade PCM. The counters restart when changing pacing or sound treatment. |

In *Final Fight*, A attacks, B jumps and X presses both. In six-button *Street Fighter II*, Y/X/L are light/medium/heavy punch and B/A/R are light/medium/heavy kick. SELECT is coin and START starts play. Profiles live in `/3ds/AstraCPS1/config/`. Replacing the application file preserves them.

**Reading the figures.** Speed compares simulation with the arcade clock. Emulation counts game frames, Drawn counts rendered images and Display counts submissions. Audio empty counts occasions when the DSP ran out of sound. A demanding stage can slow the simulation even when the pictures look evenly spaced.

**Known cases.** The drivers for `dinopic`, `dinopic2`, `punipic`, `punipic2` and `punipic3` do not reproduce their PIC audio. For `wofh` the loader can offer to proceed without sampled effects if `sgyxz_snd1.bin` (CRC `c15ac0f2`) is missing. It can use that chip from the player's `sgyxz.zip` beside `wofh.zip`. On the *Three Wonders* cabinet menu, insert a credit, select a game with left/right, then press START. A physical-console issue remains open. An actionable report identifies the ZIP, mini-game, pacing, sound mode and exact symptom.
