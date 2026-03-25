#pragma once

#include <QWidget>

class QLabel;

namespace AetherSDR {

class AmpApplet : public QWidget {
    Q_OBJECT
public:
    explicit AmpApplet(QWidget* parent = nullptr);

    void setFwdPower(float watts);
    void setSwr(float swr);
    void setTemp(float degC);
    void setModel(const QString& model);

protected:
    void paintEvent(QPaintEvent*) override;

private:
    void drawGauge(QPainter& p, int y, int h, const QString& label,
                   float value, float minVal, float maxVal,
                   const QVector<float>& ticks, const QString& unit,
                   float yellowThresh, float redThresh);

    QString m_model{"POWER GENIUS XL"};
    float   m_fwdPwr{0};
    float   m_swr{1.0f};
    float   m_temp{0};
};

} // namespace AetherSDR
