# QA Test Script: DVK Phase 2 — Context Menu & Rename

**Issue:** #19
**Commit:** (post Phase 2)
**Prereqs:** Radio connected, DVK panel visible (click DVK indicator in title bar)

---

## 1. Context Menu — Basic Appearance

| # | Step | Expected |
|---|------|----------|
| 1.1 | Right-click on slot 1 row | Context menu appears with: Rename..., separator, Clear, Delete, separator, Export WAV... |
| 1.2 | Verify Export WAV... item | Greyed out / disabled |
| 1.3 | Click away to dismiss menu | Menu closes, no action taken |
| 1.4 | Right-click on slot 5 row | Slot 5 becomes selected (blue border), context menu appears |
| 1.5 | Right-click on slot 12 row | Slot 12 selected, menu appears at correct position (not clipped off-screen) |

## 2. Context Menu — Clear

| # | Step | Expected |
|---|------|----------|
| 2.1 | Record a short message into slot 1 (select slot 1, click REC, wait 2s, click REC again) | Slot 1 shows name and duration (e.g. "2.0s") |
| 2.2 | Right-click slot 1 -> Clear | Duration resets to "Empty", name label dims to grey (#505060) |
| 2.3 | Right-click an already-empty slot -> Clear | No crash, no visible change |

## 3. Context Menu — Delete

| # | Step | Expected |
|---|------|----------|
| 3.1 | Record a short message into slot 2 | Slot 2 shows name and duration |
| 3.2 | Right-click slot 2 -> Delete | Slot resets — duration shows "Empty", name label dims |
| 3.3 | Right-click an empty slot -> Delete | No crash, no visible change |

## 4. Rename via Context Menu

| # | Step | Expected |
|---|------|----------|
| 4.1 | Right-click slot 1 -> Rename... | Inline QLineEdit appears over the name label, text pre-selected, label hidden |
| 4.2 | Type "CQ Contest" and press Enter | Edit closes, label shows "CQ Contest", `dvk set_name name="CQ Contest" id=1` sent to radio |
| 4.3 | Right-click slot 3 -> Rename..., type "My Call" and click elsewhere (lose focus) | Edit commits on focus loss, label shows "My Call" |
| 4.4 | Right-click slot 1 -> Rename..., press Escape | Edit cancels, original name restored (no command sent) |
| 4.5 | Right-click slot 2 -> Rename..., type text with quotes: `He said "hi"` | Quotes stripped, name stored as `He said hi` |
| 4.6 | Right-click slot 2 -> Rename..., type text with apostrophe: `It's me` | Apostrophe stripped, name stored as `Its me` |
| 4.7 | Right-click slot 4 -> Rename..., clear all text and press Enter | Empty name rejected — no command sent, original name preserved |
| 4.8 | Right-click slot 5 -> Rename..., type 45 characters | Input truncated to 40 characters (maxLength enforced) |

## 5. Rename via Double-Click

| # | Step | Expected |
|---|------|----------|
| 5.1 | Double-click on slot 1's name label | Inline edit appears, same as context menu rename |
| 5.2 | Type "New Name" and press Enter | Edit commits, label updates |
| 5.3 | Double-click on slot 6's name label, press Escape | Edit cancels, original name preserved |
| 5.4 | Double-click on slot 3 name, then double-click on slot 7 name (without committing) | First edit commits (or cancels cleanly), second edit opens on slot 7. No crash, no orphaned editors |
| 5.5 | Double-click on the duration label (right side) | Nothing happens — only name labels trigger rename |
| 5.6 | Double-click on the F-key button | Normal button behavior (not rename). No crash |

## 6. Rename Editor Appearance

| # | Step | Expected |
|---|------|----------|
| 6.1 | Trigger rename on any slot | Edit has dark background (#1a2a3a), light text (#c8d8e8), cyan border (#00b4d8), matches theme |
| 6.2 | Verify edit position | Overlays exactly where the name label was, same height and width |
| 6.3 | Verify text is pre-selected | All text highlighted on open, typing replaces it |

## 7. Escape Key Priority

| # | Step | Expected |
|---|------|----------|
| 7.1 | Start rename on slot 1, then press Escape | Rename cancels (edit closes, label restored) — DVK operation NOT stopped |
| 7.2 | Start playback on slot 1 (F1), then press Escape (no rename active) | Playback stops as before |
| 7.3 | Start recording, then right-click to rename, then press Escape | Rename cancels. Recording continues (another Escape stops it) |

## 8. Interaction with DVK Operations

| # | Step | Expected |
|---|------|----------|
| 8.1 | Start recording on slot 1, right-click slot 1 -> Rename... | Rename editor opens (recording continues in background) |
| 8.2 | During playback, right-click the playing slot -> Clear | Clear command sent, playback behavior depends on radio response |
| 8.3 | Right-click context menu on slot while DVK is disabled (no SmartSDR+ license) | Menu appears but Clear/Delete may fail gracefully (radio rejects) |

## 9. Edge Cases

| # | Step | Expected |
|---|------|----------|
| 9.1 | Rapidly right-click different slots | Selection updates each time, menu appears correctly, no crash |
| 9.2 | Open context menu, then resize the DVK panel (drag splitter) | Menu stays open or closes gracefully |
| 9.3 | Open rename editor, then toggle DVK panel off (click DVK indicator) | Panel hides, editor destroyed cleanly. Reopen panel — no orphaned widgets |
| 9.4 | Open rename editor, then disconnect from radio | No crash, editor closes or becomes inert |
| 9.5 | Right-click in empty area below slot 12 | No context menu (only row frames have it) |

---

## Results

| Section | Pass | Fail | Notes |
|---------|------|------|-------|
| 1. Context Menu Appearance | | | |
| 2. Clear | | | |
| 3. Delete | | | |
| 4. Rename (Context Menu) | | | |
| 5. Rename (Double-Click) | | | |
| 6. Editor Appearance | | | |
| 7. Escape Priority | | | |
| 8. DVK Operations | | | |
| 9. Edge Cases | | | |

**Tested by:** ________________  **Date:** ________________
