// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

// N8EM fork: the look the decks under the pan share, ported from the Orion
// console (src/ui/DeckStyle.h): the console's contest deck and its CW deck
// wear the same dark navy, typing white in dark-blue fields, labels light
// blue, buttons light-blue letters that go white on blue when on. Fork rule
// 2 allows the console's appearance here (the CW window's exception, which
// the deck extends); the sheet is set on the deck widgets only, so nothing
// of it leaks into AetherSDR's own screens. Widget-level sheets (the worked
// colours, the LOG IT button) still win over these rules.

namespace AetherSDR::orion {

inline constexpr const char* kDeckStyle =
    "QWidget#orionCwDeck, QWidget#orionRttyDeck, QDialog#orionDeckPane { background:#121921; }"
    "QWidget { color:#e8eef5; }"
    "QLabel { color:#c8d4e0; background:transparent; }"
    "QLineEdit, QSpinBox, QComboBox { background:#1c2430; color:#ffffff;"
    " border:1px solid #2a3644; border-radius:3px; padding:2px 4px;"
    " selection-background-color:#2b5c8a; }"
    "QComboBox QAbstractItemView { background:#1c2430; color:#ffffff;"
    " selection-background-color:#2b5c8a; }"
    "QLineEdit:focus, QSpinBox:focus { border:1px solid #5db2f0; }"
    "QPushButton { background:#1c2430; color:#8fa3b8; border:1px solid #2a3644;"
    " border-radius:3px; padding:2px 2px; }"
    "QPushButton:hover { border-color:#4a5a6e; }"
    "QPushButton:checked { background:#2b5c8a; color:#ffffff; border-color:#5db2f0; }"
    "QPushButton:disabled { color:#4a5868; border-color:#222c38; }"
    "QPushButton:checked:disabled { background:#1f3a55; color:#8fa3b8; }"
    "QPlainTextEdit { background:#0e141b; color:#e8eef5;"
    " border:1px solid #2a3644; selection-background-color:#2b5c8a; }"
    "QSlider::groove:horizontal { background:#1c2430; height:4px; border-radius:2px; }"
    "QSlider::handle:horizontal { background:#5db2f0; width:10px; margin:-4px 0;"
    " border-radius:5px; }"
    "QScrollBar:vertical { background:#121921; width:12px; }"
    "QScrollBar::handle:vertical { background:#2a3644; border-radius:4px; min-height:20px; }"
    "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height:0; }";

}  // namespace AetherSDR::orion
