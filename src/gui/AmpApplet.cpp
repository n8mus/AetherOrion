#include "AmpApplet.h"
#include <QPainter>
#include <QPaintEvent>

namespace AetherSDR {

AmpApplet::AmpApplet(QWidget* parent)
    : QWidget(parent)
{
    setMinimumHeight(120);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

void AmpApplet::setFwdPower(float watts)
{
    if (qFuzzyCompare(m_fwdPwr, watts)) return;
    m_fwdPwr = watts;
    update();
}

void AmpApplet::setSwr(float swr)
{
    if (qFuzzyCompare(m_swr, swr)) return;
    m_swr = swr;
    update();
}

void AmpApplet::setTemp(float degC)
{
    if (qFuzzyCompare(m_temp, degC)) return;
    m_temp = degC;
    update();
}

void AmpApplet::setModel(const QString& model)
{
    m_model = model;
    update();
}

void AmpApplet::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.fillRect(rect(), QColor(0x0f, 0x0f, 0x1a));

    // Title
    QFont titleFont("sans-serif", 10, QFont::Bold);
    p.setFont(titleFont);
    p.setPen(QColor(0xc8, 0xd8, 0xe8));
    p.drawText(QRect(0, 2, width(), 18), Qt::AlignCenter, m_model);

    const int gaugeX = 8;
    const int gaugeW = width() - 16;
    const int gaugeH = 16;
    const int startY = 24;
    const int spacing = gaugeH + 16;

    // Fwd Power: 0-2000W
    drawGauge(p, startY, gaugeH, "Fwd Pwr",
              m_fwdPwr, 0, 2000,
              {0, 500, 1000, 1500, 2000}, "W",
              1500, 2000);

    // SWR: 1-3
    drawGauge(p, startY + spacing, gaugeH, "SWR",
              m_swr, 1.0f, 3.0f,
              {1.0f, 1.5f, 2.0f, 2.5f, 3.0f}, "",
              2.0f, 2.5f);

    // Temp: 30-100°C
    drawGauge(p, startY + spacing * 2, gaugeH, "Temp",
              m_temp, 30, 100,
              {30, 50, 80, 100}, "°C",
              80, 100);
}

void AmpApplet::drawGauge(QPainter& p, int y, int h, const QString& label,
                          float value, float minVal, float maxVal,
                          const QVector<float>& ticks, const QString& unit,
                          float yellowThresh, float redThresh)
{
    const int gaugeX = 8;
    const int gaugeW = width() - 16;
    const int labelW = 55;
    const int barX = gaugeX + labelW;
    const int barW = gaugeW - labelW;

    // Label
    QFont labelFont("sans-serif", 9);
    p.setFont(labelFont);
    p.setPen(QColor(0xc8, 0xd8, 0xe8));
    p.drawText(QRect(gaugeX, y, labelW, h), Qt::AlignLeft | Qt::AlignVCenter, label);

    // Background bar
    QRect barRect(barX, y, barW, h);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0x1a, 0x2a, 0x3a));
    p.drawRoundedRect(barRect, 3, 3);

    // Fill bar
    float frac = qBound(0.0f, (value - minVal) / (maxVal - minVal), 1.0f);
    int fillW = static_cast<int>(frac * barW);
    if (fillW > 0) {
        QColor fillColor(0x00, 0xb4, 0xd8);  // cyan
        if (value >= redThresh)
            fillColor = QColor(0xff, 0x30, 0x30);  // red
        else if (value >= yellowThresh)
            fillColor = QColor(0xff, 0xc0, 0x00);  // yellow

        QRect fillRect(barX, y, fillW, h);
        p.setBrush(fillColor);
        p.drawRoundedRect(fillRect, 3, 3);
    }

    // Tick marks and labels
    QFont tickFont("sans-serif", 7);
    p.setFont(tickFont);
    QFontMetrics tfm(tickFont);

    for (float t : ticks) {
        float tf = (t - minVal) / (maxVal - minVal);
        int tx = barX + static_cast<int>(tf * barW);

        // Tick line
        p.setPen(QPen(QColor(0x40, 0x50, 0x60), 1));
        p.drawLine(tx, y, tx, y + h);

        // Tick label
        QString tickLabel;
        if (t == static_cast<int>(t))
            tickLabel = QString::number(static_cast<int>(t));
        else
            tickLabel = QString::number(t, 'f', 1);

        p.setPen(QColor(0x80, 0x90, 0xa0));
        int tw = tfm.horizontalAdvance(tickLabel);
        p.drawText(tx - tw / 2, y + h + tfm.ascent() + 1, tickLabel);
    }

    // Red zone markers for threshold ticks
    for (float t : {yellowThresh, redThresh}) {
        float tf = (t - minVal) / (maxVal - minVal);
        int tx = barX + static_cast<int>(tf * barW);
        QColor c = (t >= redThresh) ? QColor(0xff, 0x30, 0x30) : QColor(0xff, 0xc0, 0x00);
        QString tickLabel;
        if (t == static_cast<int>(t))
            tickLabel = QString::number(static_cast<int>(t));
        else
            tickLabel = QString::number(t, 'f', 1);
        p.setPen(c);
        int tw = tfm.horizontalAdvance(tickLabel);
        p.drawText(tx - tw / 2, y + h + tfm.ascent() + 1, tickLabel);
    }

    // Value readout on right
    QFont valFont("sans-serif", 9, QFont::Bold);
    p.setFont(valFont);
    p.setPen(QColor(0xc8, 0xd8, 0xe8));
    QString valText;
    if (maxVal > 100)
        valText = QString::number(static_cast<int>(value));
    else
        valText = QString::number(value, 'f', 1);
    p.drawText(QRect(barX, y - 14, barW, 14), Qt::AlignRight | Qt::AlignBottom,
               valText + " " + unit);
}

} // namespace AetherSDR
