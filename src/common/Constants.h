#pragma once
#include <QColor>
#include <QString>

namespace Constants {
    inline constexpr const char* AppName     = "IMMERSIVE PLAYER";
    inline constexpr const char* AppVersion  = "1.0.0";

    // Color palette v2 — brighter premium dark
    inline const QColor ColorBg(0x10, 0x11, 0x14);        // #101114
    inline const QColor ColorPanel(0x18, 0x19, 0x1D);      // #18191d
    inline const QColor ColorText(0xD4, 0xDC, 0xEC);       // #d4dcec
    inline const QColor ColorTextDim(0xFF, 0xFF, 0xFF, 33);// rgba(255,255,255,0.13)

    // Scene theme colors
    inline const QColor ColorConcert(0xFF, 0x00, 0x64);
    inline const QColor ColorTechPlaza(0x00, 0xD4, 0xFF);
    inline const QColor ColorCinema(0xFF, 0xB4, 0x32);
    inline const QColor ColorNormal(0x66, 0x66, 0xFF);

    // UI sizes
    inline constexpr int ControlBarHeight   = 48;
    inline constexpr int ProgressBarHeight  = 4;
    inline constexpr int PanelBorderRadius  = 16;
    inline constexpr int CardBorderRadius   = 12;
    inline constexpr int AutoHideDelayMs    = 3000;
    inline constexpr double ProximityRange  = 2.0;
}
