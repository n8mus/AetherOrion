// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

// N8EM fork: ←/→ walk the spots from ANYWHERE in the main window while a
// deck is up — the panadapter included, as the console had it — not only
// from the deck's own call box. Installed on the application while the
// deck is shown; a key that lands in a text box, a spin box or a combo is
// left to that widget.

#include <QObject>
#include <functional>

class QWidget;

namespace AetherSDR::orion {

class DeckArrowFilter : public QObject {
    Q_OBJECT
public:
    // `window` is the main window; `walk` gets +1 / -1.
    DeckArrowFilter(QWidget* window, std::function<void(int)> walk, QObject* parent = nullptr);
    ~DeckArrowFilter() override;
    void setActive(bool on);

protected:
    bool eventFilter(QObject* obj, QEvent* ev) override;

private:
    QWidget* m_window;
    std::function<void(int)> m_walk;
    bool m_active = false;
};

}  // namespace AetherSDR::orion
