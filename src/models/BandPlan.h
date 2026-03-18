#pragma once

#include <QColor>

namespace AetherSDR {

// ARRL band plan sub-segments (US allocation) with license class
// Color key matches ARRL official band plan chart:
//   Red     = RTTY and data
//   Orange  = Phone and image
//   Blue    = CW only
//   Green   = SSB phone
//   Yellow  = USB phone, CW, RTTY, and data
//   Purple  = Fixed digital message forwarding
struct BandSegment {
    double lowMhz;
    double highMhz;
    const char* label;   // "CW", "DIGI", "SSB", "USB/CW/DIGI", etc.
    const char* license; // "E"=Extra, "A"=Advanced, "G"=General, "T"=Tech, "N"=Novice, ""=all
    int r, g, b;         // segment color per ARRL key
};

// ARRL colors:
//   CW only:              blue    (0x30, 0x60, 0xff)
//   RTTY/data:            red     (0xd0, 0x30, 0x30)
//   Phone/image:          orange  (0xff, 0x80, 0x00)
//   SSB phone:            green   (0x30, 0xb0, 0x30)
//   USB phone/CW/RTTY:    yellow  (0xff, 0xd0, 0x00)
//   Fixed digital fwd:    purple  (0x90, 0x30, 0xc0)
//   Beacon:               cyan    (0x00, 0xd0, 0xd0)

inline constexpr BandSegment kBandPlan[] = {
    // 160m (1.800 - 2.000)
    {1.800,  1.810,  "CW",     "E",  0x30, 0x60, 0xff},
    {1.810,  1.840,  "CW",     "",   0x30, 0x60, 0xff},
    {1.840,  1.843,  "PHONE",  "E",  0xff, 0x80, 0x00},
    {1.843,  2.000,  "PHONE",  "G",  0xff, 0x80, 0x00},

    // 80m (3.500 - 4.000)
    {3.500,  3.525,  "CW",     "E",  0x30, 0x60, 0xff},
    {3.525,  3.600,  "CW",     "",   0x30, 0x60, 0xff},
    {3.600,  3.700,  "DATA",   "",   0xd0, 0x30, 0x30},
    {3.700,  3.800,  "PHONE",  "E",  0xff, 0x80, 0x00},
    {3.800,  4.000,  "PHONE",  "G",  0xff, 0x80, 0x00},

    // 60m (5.330 - 5.405) — channelized, USB phone/CW/RTTY/data
    {5.330,  5.405,  "ALL",    "G",  0xff, 0xd0, 0x00},

    // 40m (7.000 - 7.300)
    {7.000,  7.025,  "CW",     "E",  0x30, 0x60, 0xff},
    {7.025,  7.125,  "CW",     "",   0x30, 0x60, 0xff},
    {7.125,  7.175,  "PHONE",  "G",  0xff, 0x80, 0x00},
    {7.175,  7.300,  "PHONE",  "",   0xff, 0x80, 0x00},

    // 30m (10.100 - 10.150) — CW/data only, General+
    {10.100, 10.130, "CW",     "G",  0x30, 0x60, 0xff},
    {10.130, 10.150, "DATA",   "G",  0xd0, 0x30, 0x30},

    // 20m (14.000 - 14.350)
    {14.000, 14.025, "CW",     "E",  0x30, 0x60, 0xff},
    {14.025, 14.070, "CW",     "",   0x30, 0x60, 0xff},
    {14.070, 14.095, "DATA",   "",   0xd0, 0x30, 0x30},
    {14.095, 14.099, "DATA",   "",   0xd0, 0x30, 0x30},
    {14.099, 14.101, "BCN",    "",   0x00, 0xd0, 0xd0},
    {14.101, 14.150, "PHONE",  "",   0xff, 0x80, 0x00},
    {14.150, 14.175, "PHONE",  "E",  0xff, 0x80, 0x00},
    {14.175, 14.225, "PHONE",  "",   0xff, 0x80, 0x00},
    {14.225, 14.350, "SSB",    "",   0x30, 0xb0, 0x30},

    // 17m (18.068 - 18.168) — General+
    {18.068, 18.100, "CW",     "G",  0x30, 0x60, 0xff},
    {18.100, 18.105, "DATA",   "G",  0xd0, 0x30, 0x30},
    {18.105, 18.110, "BCN",    "",   0x00, 0xd0, 0xd0},
    {18.110, 18.168, "PHONE",  "G",  0xff, 0x80, 0x00},

    // 15m (21.000 - 21.450)
    {21.000, 21.025, "CW",     "E",  0x30, 0x60, 0xff},
    {21.025, 21.070, "CW",     "",   0x30, 0x60, 0xff},
    {21.070, 21.110, "DATA",   "",   0xd0, 0x30, 0x30},
    {21.110, 21.150, "DATA",   "",   0xd0, 0x30, 0x30},
    {21.150, 21.151, "BCN",    "",   0x00, 0xd0, 0xd0},
    {21.151, 21.200, "PHONE",  "E",  0xff, 0x80, 0x00},
    {21.200, 21.275, "SSB",    "",   0x30, 0xb0, 0x30},
    {21.275, 21.450, "PHONE",  "",   0xff, 0x80, 0x00},

    // 12m (24.890 - 24.990) — General+
    {24.890, 24.920, "CW",     "G",  0x30, 0x60, 0xff},
    {24.920, 24.925, "DATA",   "G",  0xd0, 0x30, 0x30},
    {24.925, 24.930, "BCN",    "",   0x00, 0xd0, 0xd0},
    {24.930, 24.990, "PHONE",  "G",  0xff, 0x80, 0x00},

    // 10m (28.000 - 29.700)
    {28.000, 28.070, "CW",     "",   0x30, 0x60, 0xff},
    {28.070, 28.150, "DATA",   "",   0xd0, 0x30, 0x30},
    {28.150, 28.190, "CW",     "",   0x30, 0x60, 0xff},
    {28.190, 28.200, "BCN",    "",   0x00, 0xd0, 0xd0},
    {28.200, 28.300, "BCN",    "",   0x00, 0xd0, 0xd0},
    {28.300, 29.000, "PHONE",  "",   0xff, 0x80, 0x00},
    {29.000, 29.200, "DATA",   "",   0xd0, 0x30, 0x30},
    {29.200, 29.300, "PHONE",  "",   0xff, 0x80, 0x00},
    {29.300, 29.510, "SAT",    "",   0x90, 0x30, 0xc0},
    {29.510, 29.700, "FM",     "T",  0xff, 0x80, 0x00},

    // 6m (50.000 - 54.000)
    {50.000, 50.100, "CW",     "",   0x30, 0x60, 0xff},
    {50.100, 50.300, "SSB",    "",   0x30, 0xb0, 0x30},
    {50.300, 50.600, "DATA",   "",   0xd0, 0x30, 0x30},
    {50.600, 51.000, "PHONE",  "",   0xff, 0x80, 0x00},
    {51.000, 52.000, "FM",     "T",  0xff, 0x80, 0x00},
    {52.000, 54.000, "ALL",    "T",  0xff, 0xd0, 0x00},
};

inline constexpr int kBandPlanCount = static_cast<int>(std::size(kBandPlan));

} // namespace AetherSDR
