# QA Test Script: Multi-Pan Step Size & Applet Sync (#297)

**Issue:** #297
**Prereqs:** Radio connected, two slices on separate pans (Slice A top, Slice B bottom)

---

## 1. Step Size Follows Active Pan

| # | Step | Expected |
|---|------|----------|
| 1.1 | Click Slice A's VFO flag (top pan) | Top pan active, RX applet shows Slice A's step size |
| 1.2 | Set step to 100 Hz via RX applet stepper | Step shows 100 Hz, click-to-tune moves by 100 Hz |
| 1.3 | Click Slice B's VFO flag (bottom pan) | Bottom pan active, RX applet shows Slice B's step size (may differ from 100) |
| 1.4 | Set step to 1000 Hz on Slice B | Step shows 1000 Hz, click-to-tune moves by 1000 Hz |
| 1.5 | Click back on Slice A (top pan) | Step reverts to 100 Hz (Slice A's setting) |
| 1.6 | Click back on Slice B (bottom pan) | Step reverts to 1000 Hz (Slice B's setting) |
| 1.7 | Scroll wheel on top pan spectrum | Tunes by 100 Hz (Slice A's step) |
| 1.8 | Scroll wheel on bottom pan spectrum | Tunes by 1000 Hz (Slice B's step) |

## 2. RX Applet Controls Follow Active Pan

| # | Step | Expected |
|---|------|----------|
| 2.1 | Click Slice A, set AGC to Slow in RX applet | AGC shows Slow |
| 2.2 | Click Slice B | AGC switches to Slice B's setting (e.g., Med) |
| 2.3 | Click Slice A | AGC reverts to Slow |
| 2.4 | Change filter preset on Slice B (e.g., 3.3K) | Filter shows 3.3K |
| 2.5 | Click Slice A | Filter reverts to Slice A's setting (e.g., 2.7K) |
| 2.6 | Toggle NB on Slice A | NB indicator lit |
| 2.7 | Click Slice B | NB state shows Slice B's setting (NB off if not set) |

## 3. Overlay Menu Follows Active Pan

| # | Step | Expected |
|---|------|----------|
| 3.1 | Open ANT panel on top pan overlay | Shows Slice A's RX/TX antennas |
| 3.2 | Click bottom pan to make it active | ANT panel (if still open) updates to Slice B's antennas |
| 3.3 | Open DSP panel on bottom pan overlay | Shows Slice B's DSP toggles (NR, NB, ANF states) |
| 3.4 | Click top pan | DSP panel updates to Slice A's states |

## 4. Different Modes on Each Pan

| # | Step | Expected |
|---|------|----------|
| 4.1 | Set Slice A to USB, Slice B to CW | Each pan shows correct mode in VFO |
| 4.2 | Click Slice B (CW) | CW decode panel appears, step size matches CW step |
| 4.3 | Click Slice A (USB) | CW decode panel hides, step size matches USB step |
| 4.4 | Set Slice A to LSB, Slice B to AM | Step sizes independent per slice |
| 4.5 | Click between pans rapidly | Each shows correct mode, step, filter, AGC — no stale values |

## 5. Single Pan (Regression Check)

| # | Step | Expected |
|---|------|----------|
| 5.1 | Close second pan, single slice | Step size changes work as before |
| 5.2 | Add second slice on same pan (+RX) | Step follows whichever slice is clicked |
| 5.3 | Change step on Slice A, click Slice B flag, change step | Each slice keeps its own step |

---

## Results

| Section | Pass | Fail | Notes |
|---------|------|------|-------|
| 1. Step Size | | | |
| 2. RX Applet | | | |
| 3. Overlay Menu | | | |
| 4. Different Modes | | | |
| 5. Single Pan | | | |

**Tested by:** ________________  **Date:** ________________
