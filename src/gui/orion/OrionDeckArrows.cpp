// SPDX-License-Identifier: GPL-2.0-or-later
#include "gui/orion/OrionDeckArrows.h"

#include <QAbstractSpinBox>
#include <QApplication>
#include <QComboBox>
#include <QEvent>
#include <QKeyEvent>
#include <QLineEdit>
#include <QTextEdit>
#include <QPlainTextEdit>
#include <QWidget>

namespace AetherSDR::orion {

DeckArrowFilter::DeckArrowFilter(QWidget* window, std::function<void(int)> walk, QObject* parent)
    : QObject(parent)
    , m_window(window)
    , m_walk(std::move(walk))
{
}

DeckArrowFilter::~DeckArrowFilter()
{
    setActive(false);
}

void DeckArrowFilter::setActive(bool on)
{
    if (on == m_active) return;
    m_active = on;
    if (on) qApp->installEventFilter(this);
    else qApp->removeEventFilter(this);
}

bool DeckArrowFilter::eventFilter(QObject* obj, QEvent* ev)
{
    if (ev->type() != QEvent::KeyPress && ev->type() != QEvent::ShortcutOverride) {
        return false;
    }
    auto* ke = static_cast<QKeyEvent*>(ev);
    if ((ke->key() != Qt::Key_Left && ke->key() != Qt::Key_Right)
        || ke->modifiers() != Qt::NoModifier) {
        return false;
    }
    QWidget* w = qobject_cast<QWidget*>(obj);
    if (!w || w->window() != m_window) return false;
    // A text cursor owns its own arrows; the deck's boxes handle theirs.
    QWidget* f = QApplication::focusWidget();
    if (f && (qobject_cast<QLineEdit*>(f) || qobject_cast<QAbstractSpinBox*>(f)
              || qobject_cast<QComboBox*>(f) || qobject_cast<QTextEdit*>(f)
              || qobject_cast<QPlainTextEdit*>(f))) {
        return false;
    }
    if (ev->type() == QEvent::ShortcutOverride) {
        ev->accept();
        return true;
    }
    if (m_walk) m_walk(ke->key() == Qt::Key_Right ? +1 : -1);
    return true;
}

}  // namespace AetherSDR::orion
