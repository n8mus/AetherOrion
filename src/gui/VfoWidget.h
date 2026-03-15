#pragma once

#include <QWidget>
#include <QVector>
#include <QStringList>

class QPushButton;
class QLabel;
class QStackedWidget;
class QSlider;
class QComboBox;

namespace AetherSDR {

class SliceModel;
class TransmitModel;

// Floating VFO info panel attached to the VFO marker on the spectrum display.
// Shows slice info (antennas, frequency, signal level, filter width, TX/SPLIT)
// and tabbed sub-menus (Audio, DSP, Mode, X/RIT, DAX).
// Anchored to the left of the VFO marker; flips right when clipped.
class VfoWidget : public QWidget {
    Q_OBJECT

public:
    explicit VfoWidget(QWidget* parent = nullptr);

    void setSlice(SliceModel* slice);
    void setAntennaList(const QStringList& ants);
    void setTransmitModel(TransmitModel* txModel);
    void setSignalLevel(float dbm);

    // Reposition relative to VFO marker x coordinate.
    void updatePosition(int vfoX, int specTop);

protected:
    void paintEvent(QPaintEvent* event) override;
    bool eventFilter(QObject* obj, QEvent* event) override;

private:
    void buildUI();
    void buildTabContent();
    void syncFromSlice();
    void showTab(int index);
    void updateFreqLabel();
    void updateFilterLabel();
    void updateModeTab();

    SliceModel*    m_slice{nullptr};
    TransmitModel* m_txModel{nullptr};
    QStringList    m_antList;
    bool           m_updatingFromModel{false};
    float          m_signalDbm{-130.0f};

    // Header row
    QPushButton* m_rxAntBtn{nullptr};
    QPushButton* m_txAntBtn{nullptr};
    QLabel*      m_filterWidthLbl{nullptr};
    QLabel*      m_splitBadge{nullptr};
    QPushButton* m_txBadge{nullptr};
    QLabel*      m_sliceBadge{nullptr};

    // Frequency / meter
    QLabel* m_freqLabel{nullptr};
    QLabel* m_dbmLabel{nullptr};

    // Sub-menu tabs (QLabels with click via event filter)
    QVector<QLabel*> m_tabBtns;
    QStackedWidget* m_tabStack{nullptr};
    QWidget*        m_tabBar{nullptr};
    int m_activeTab{-1};

    // Tab content widgets
    // Audio tab
    QSlider* m_afGainSlider{nullptr};
    QSlider* m_panSlider{nullptr};
    QPushButton* m_muteBtn{nullptr};
    // DSP tab
    QPushButton* m_nbBtn{nullptr};
    QPushButton* m_nrBtn{nullptr};
    QPushButton* m_anfBtn{nullptr};
    QPushButton* m_nrlBtn{nullptr};
    QPushButton* m_nrsBtn{nullptr};
    QPushButton* m_rnnBtn{nullptr};
    // Mode tab
    QVector<QPushButton*> m_modeBtns;
    // RIT/XIT tab
    QPushButton* m_ritBtn{nullptr};
    QPushButton* m_xitBtn{nullptr};
    QLabel* m_ritLabel{nullptr};
    QLabel* m_xitLabel{nullptr};
    // DAX tab
    QComboBox* m_daxCmb{nullptr};

    static constexpr int WIDGET_W = 252;
};

} // namespace AetherSDR
