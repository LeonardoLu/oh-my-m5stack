#pragma once
#include <M5Unified.h>
#include <UxText.h>
#include <UxRender.h>
#include "WatchStrings.h"

// Migration adapter: retain deliberate alignment while using native AA glyphs.
inline void watchText(M5Canvas& cv, const char* text, int x, int y, bool localize=true) {
    if(localize) text=watchstrings::translate(text);
    const ux::Font* font = cv.getFont() == &fonts::FreeSansBold12pt7b
        || cv.getFont() == &fonts::FreeSansBold18pt7b ? &ux::Latin24 : &ux::Latin18;
    for (const unsigned char* p = (const unsigned char*)text; *p; ++p)
        if (*p >= 128) { font = &ux::Cjk18; break; }
    unsigned datum = (unsigned)cv.getTextDatum();
    int width = ux::textWidth(text, *font);
    if ((datum & 3) == 1) x -= width / 2;
    else if ((datum & 3) == 2) x -= width;
    if ((datum & 12) == 4) y -= ux::lineHeight(*font) / 2;
    else if ((datum & 12) == 8) y -= ux::lineHeight(*font);
    uint32_t rgb = cv.getTextStyle().fore_rgb888;
    uint16_t ink = ((rgb >> 8) & 0xF800) | ((rgb >> 5) & 0x07E0) | ((rgb >> 3) & 0x1F);
    ux::drawText(cv, text, x, y, ink, *font);
}
