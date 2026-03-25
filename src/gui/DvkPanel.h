#pragma once

#include <QWidget>
#include <QPushButton>
#include <QLabel>
#include <QListWidget>

namespace AetherSDR {
class DvkModel;

class DvkPanel : public QWidget {
    Q_OBJECT
public:
    explicit DvkPanel(DvkModel* model, QWidget* parent = nullptr);

    int selectedSlot() const;

private slots:
    void onStatusChanged(int status, int id);
    void onRecordingChanged(int id);
    void rebuildList();

private:
    DvkModel* m_model;
    QListWidget* m_slotList;
    QPushButton* m_recBtn;
    QPushButton* m_playBtn;
    QPushButton* m_prevBtn;
    QLabel* m_statusLabel;

    void updateButtons();
    QString formatDuration(int ms);
};

} // namespace AetherSDR
