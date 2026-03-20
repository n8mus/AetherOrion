#pragma once

#include <QDialog>
#include <QMap>

class QCheckBox;
class QPlainTextEdit;
class QLabel;

namespace AetherSDR {

class SupportDialog : public QDialog {
    Q_OBJECT

public:
    explicit SupportDialog(QWidget* parent = nullptr);

private slots:
    void refreshLog();
    void clearLog();
    void openLogFolder();
    void enableAll();
    void disableAll();

private:
    void buildUI();
    void syncCheckboxes();
    QString formatFileSize(qint64 bytes) const;

    QMap<QString, QCheckBox*> m_checkboxes;  // category id → checkbox
    QPlainTextEdit* m_logViewer{nullptr};
    QLabel*         m_sizeLabel{nullptr};
};

} // namespace AetherSDR
