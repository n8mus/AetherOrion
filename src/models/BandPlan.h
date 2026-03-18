#pragma once

#include <QColor>

namespace AetherSDR {

// ARRL US Amateur Radio Band Plan (rev. 1/16/2026)
// Source: docs/Hambands4_Color_17x11.pdf
//
// Color key (matches ARRL chart):
//   Red     = RTTY and data
//   Orange  = Phone and image
//   Blue    = CW only
//   Green   = SSB phone
//   Yellow  = USB phone, CW, RTTY, and data
//   Purple  = Fixed digital message forwarding systems only
//
// License: E=Extra, A=Advanced, G=General, T=Technician, N=Novice

struct BandSegment {
    double lowMhz;
    double highMhz;
    const char* label;
    const char* license; // "" = all, "E" = Extra only, "E,A,G" = Extra/Adv/Gen, etc.
    int r, g, b;
};

// ARRL colors
//   CW only:              blue    (0x30, 0x60, 0xff)
//   RTTY/data:            red     (0xc0, 0x30, 0x30)
//   Phone/image:          orange  (0xff, 0x80, 0x00)
//   SSB phone:            green   (0x30, 0xb0, 0x30)
//   USB/CW/RTTY/data:     yellow  (0xff, 0xd0, 0x00)
//   Fixed digital fwd:    purple  (0x90, 0x30, 0xc0)
//   Beacon:               cyan    (0x00, 0xd0, 0xd0)

inline constexpr BandSegment kBandPlan[] = {
    // 2200m (135 kHz)
    {0.1357, 0.1378, "CW",    "E,A,G",  0x30, 0x60, 0xff},

    // 630m (472 kHz)
    {0.472,  0.479,  "CW",    "E,A,G",  0x30, 0x60, 0xff},

    // 160m (1.800 - 2.000)
    // CW: 1.800-2.000 all (E,A,G)
    // Phone: 1.800-2.000 (E,A,G) — shown as phone/image from 1.800
    {1.800,  1.900,  "CW",    "E,A,G",  0x30, 0x60, 0xff},
    {1.900,  2.000,  "PHONE", "E,A,G",  0xff, 0x80, 0x00},

    // 80m (3.500 - 4.000)
    // E: 3.500-3.600 CW/data, 3.600-4.000 all
    // A: 3.525-3.600 CW/data, 3.700-4.000 phone
    // G: 3.525-3.600 CW/data, 3.800-4.000 phone
    // N,T: 3.525-3.600 CW (outside region 2)
    {3.500,  3.525,  "CW",    "E",      0x30, 0x60, 0xff},
    {3.525,  3.600,  "CW",    "E,A,G",  0x30, 0x60, 0xff},
    {3.600,  3.700,  "DATA",  "E",      0xc0, 0x30, 0x30},
    {3.700,  3.800,  "PHONE", "E,A",    0xff, 0x80, 0x00},
    {3.800,  4.000,  "PHONE", "E,A,G",  0xff, 0x80, 0x00},

    // 60m (5.330 - 5.405) — channelized, USB phone/CW/data
    // Individual channels: 5330.5, 5346.5, 5351.5, 5357.0, 5371.5, 5403.5 kHz
    {5.3305, 5.3333, "USB",   "E,A,G",  0xff, 0xd0, 0x00},
    {5.3465, 5.3493, "USB",   "E,A,G",  0xff, 0xd0, 0x00},
    {5.3515, 5.3543, "USB",   "E,A,G",  0xff, 0xd0, 0x00},
    {5.3570, 5.3598, "USB",   "E,A,G",  0xff, 0xd0, 0x00},
    {5.3715, 5.3743, "USB",   "E,A,G",  0xff, 0xd0, 0x00},
    {5.4035, 5.4063, "USB",   "E,A,G",  0xff, 0xd0, 0x00},

    // 40m (7.000 - 7.300)
    {7.000,  7.025,  "CW",    "E",      0x30, 0x60, 0xff},
    {7.025,  7.075,  "CW",    "E,A,G",  0x30, 0x60, 0xff},
    {7.075,  7.100,  "CW",    "E,A,G",  0x30, 0x60, 0xff},
    {7.100,  7.125,  "CW",    "E,A,G",  0x30, 0x60, 0xff},
    {7.125,  7.175,  "PHONE", "E,A,G",  0xff, 0x80, 0x00},
    {7.175,  7.300,  "PHONE", "N,T",    0xff, 0x80, 0x00},

    // 30m (10.100 - 10.150) — CW/data only
    {10.100, 10.150, "CW",    "E,A,G",  0x30, 0x60, 0xff},

    // 20m (14.000 - 14.350)
    {14.000, 14.025, "CW",    "E",      0x30, 0x60, 0xff},
    {14.025, 14.150, "CW",    "E,A,G",  0x30, 0x60, 0xff},
    {14.150, 14.175, "PHONE", "E",      0xff, 0x80, 0x00},
    {14.175, 14.225, "PHONE", "E,A",    0xff, 0x80, 0x00},
    {14.225, 14.350, "PHONE", "E,A,G",  0xff, 0x80, 0x00},

    // 17m (18.068 - 18.168)
    {18.068, 18.110, "CW",    "E,A,G",  0x30, 0x60, 0xff},
    {18.110, 18.168, "PHONE", "E,A,G",  0xff, 0x80, 0x00},

    // 15m (21.000 - 21.450)
    {21.000, 21.025, "CW",    "E",      0x30, 0x60, 0xff},
    {21.025, 21.200, "CW",    "E,A,G",  0x30, 0x60, 0xff},
    {21.200, 21.225, "PHONE", "E",      0xff, 0x80, 0x00},
    {21.225, 21.275, "PHONE", "E,A",    0xff, 0x80, 0x00},
    {21.275, 21.450, "PHONE", "E,A,G",  0xff, 0x80, 0x00},

    // 12m (24.890 - 24.990)
    {24.890, 24.930, "CW",    "E,A,G",  0x30, 0x60, 0xff},
    {24.930, 24.990, "PHONE", "E,A,G",  0xff, 0x80, 0x00},

    // 10m (28.000 - 29.700)
    {28.000, 28.300, "CW",    "E,A,G",  0x30, 0x60, 0xff},
    {28.300, 28.500, "PHONE", "E,A,G",  0xff, 0x80, 0x00},
    {28.500, 29.700, "PHONE", "N,T",    0xff, 0x80, 0x00},

    // 6m (50.000 - 54.000)
    {50.000, 50.100, "CW",    "E,A,G",  0x30, 0x60, 0xff},
    {50.100, 54.000, "PHONE", "E,A,G,T",0xff, 0x80, 0x00},
};

inline constexpr int kBandPlanCount = static_cast<int>(std::size(kBandPlan));

} // namespace AetherSDR
