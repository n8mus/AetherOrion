// SPDX-License-Identifier: GPL-2.0-or-later
// The Connect to Radio window for a serial radio: with "Ten-Tec Orion
// (serial)" remembered as the Connect-by-IP family, the device box comes up
// holding the last device used (the station's udev name the first time),
// never blank. Live, 2026-11-05: the recent-address loader cleared it,
// because a serial radio never adds to that list, and the operator typed
// /dev/orion at every start.
//
//   cmake --build build --target orion_connection_panel_test && ./build/orion_connection_panel_test

#include "TestSettingsProfile.h"
#include "core/AppSettings.h"
#include "gui/ConnectionPanel.h"

#include <QApplication>
#include <QComboBox>
#include <QLineEdit>

#include <cstdio>

using namespace AetherSDR;

namespace {
int g_failures = 0;
void check(bool ok, const char* what)
{
    std::printf("  %s: %s\n", ok ? "ok" : "FAIL", what);
    if (!ok) ++g_failures;
}
}  // namespace

int main(int argc, char** argv)
{
    TestSettingsProfile settingsProfile(QStringLiteral("aether-orion-connection-panel-test"));
    if (!settingsProfile.isValid()) {
        return 1;
    }
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);
    auto& settings = AppSettings::instance();

    std::printf("-- the Orion remembered as the Connect-by-IP family, no device saved\n");
    settings.setValue(QStringLiteral("ConnectByIpRadioFamily"), QStringLiteral("orion"));
    settings.remove(QStringLiteral("SerialDevice/orion"));
    {
        ConnectionPanel panel;
        auto* type = panel.findChild<QComboBox*>(QStringLiteral("connectionManualRadioType"));
        auto* ip = panel.findChild<QLineEdit*>(QStringLiteral("connectionManualIp"));
        check(type && type->currentText().contains(QStringLiteral("Orion")),
              "the radio type comes up as the Orion");
        check(ip && ip->text() == QStringLiteral("/dev/orion"),
              "the device box holds the station's udev name");
    }

    std::printf("-- a device was used before\n");
    settings.setValue(QStringLiteral("SerialDevice/orion"), QStringLiteral("/dev/omni7"));
    {
        ConnectionPanel panel;
        auto* ip = panel.findChild<QLineEdit*>(QStringLiteral("connectionManualIp"));
        check(ip && ip->text() == QStringLiteral("/dev/omni7"), "the device box holds it");
    }

    std::printf("-- a network family still starts blank with no recent address\n");
    settings.setValue(QStringLiteral("ConnectByIpRadioFamily"), QStringLiteral("flex"));
    settings.remove(QStringLiteral("RecentConnectByIpAddresses"));
    {
        ConnectionPanel panel;
        auto* ip = panel.findChild<QLineEdit*>(QStringLiteral("connectionManualIp"));
        check(ip && ip->text().isEmpty(), "Flex: blank, as upstream has it");
    }

    std::printf("%s\n", g_failures ? "FAILED" : "all ok");
    return g_failures ? 1 : 0;
}
