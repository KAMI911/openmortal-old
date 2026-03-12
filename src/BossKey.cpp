/***************************************************************************
                         BossKey.cpp  -  description
                            -------------------
   begin                : 2025
   copyright            : (C) 2025 OpenMortal contributors

   Pressing Shift+Esc overlays a fake Microsoft Excel spreadsheet on the
   screen and silences music until any key is pressed, then restores the
   previous screen.  Compiled in by default; disable with
     ./configure --disable-boss-key
***************************************************************************/

#include "config.h"

#ifdef MSZ_BOSS_KEY

#include <stdio.h>
#include <string.h>
#include <string>

#include "SDL.h"
#include "sge_primitives.h"
#include "sge_tt_text.h"
#include "common.h"
#include "Audio.h"
#include "State.h"

// ---------------------------------------------------------------------------
// Fake spreadsheet content
// ---------------------------------------------------------------------------

static const char* s_apcMenuItems[] = {
    "File", "Edit", "View", "Insert", "Format", "Tools", "Data", "Window", "Help", NULL
};

static const char* s_apcColHeaders[] = {
    "", "A", "B", "C", "D", "E", "F", "G"
};

struct SBossRow
{
    const char* m_pcName;
    const char* m_apcCells[6];
    bool        m_bBold;
};

static const SBossRow s_aoRows[] = {
    { "Product",      { "Q1 2024", "Q2 2024", "Q3 2024", "Q4 2024", "YTD Total", "Change" }, true  },
    { "Widget Alpha", { "12,450",  "13,200",  "14,890",  "16,234",   "56,774",  "+14.2%" }, false },
    { "Widget Beta",  {  "8,234",   "7,981",   "9,103",  "10,456",   "35,774",  "+15.0%" }, false },
    { "Gadget Pro",   { "34,100",  "32,500",  "36,800",  "41,200",  "144,600",  "+12.1%" }, false },
    { "Gadget Mini",  {  "5,670",   "6,120",   "5,890",   "7,340",   "25,020",  "+25.0%" }, false },
    { "Deluxe Unit",  { "22,300",  "24,100",  "26,400",  "29,100",  "101,900",  "+10.2%" }, false },
    { "Budget Line",  {  "3,120",   "4,205",   "3,890",   "5,670",   "16,885",  "+81.7%" }, false },
    { "",             {      "",       "",        "",        "",          "",        ""    }, false },
    { "TOTAL",        { "85,874",  "88,106",  "96,973", "110,000",  "380,953",  "+13.7%" }, true  },
    { "",             {      "",       "",        "",        "",          "",        ""    }, false },
    { "",             {      "",       "",        "",        "",          "",        ""    }, false },
    { "",             {      "",       "",        "",        "",          "",        ""    }, false },
};
static const int NUM_ROWS = (int)(sizeof(s_aoRows) / sizeof(s_aoRows[0]));

// ---------------------------------------------------------------------------
// Drawing helpers
// ---------------------------------------------------------------------------

static void XlBox(Sint16 x1, Sint16 y1, Sint16 x2, Sint16 y2,
                  Uint8 r, Uint8 g, Uint8 b)
{
    sge_FilledRect(gamescreen, x1, y1, x2, y2, r, g, b);
}

static void XlHLine(Sint16 x1, Sint16 x2, Sint16 y, Uint8 r, Uint8 g, Uint8 b)
{
    sge_HLine(gamescreen, x1, x2, y, r, g, b);
}

static void XlVLine(Sint16 x, Sint16 y1, Sint16 y2, Uint8 r, Uint8 g, Uint8 b)
{
    sge_VLine(gamescreen, x, y1, y2, r, g, b);
}

// Win98-style raised 3-D box
static void Xl3DBox(Sint16 x1, Sint16 y1, Sint16 x2, Sint16 y2)
{
    sge_FilledRect(gamescreen, x1, y1, x2, y2, 212, 208, 200);
    sge_HLine(gamescreen, x1,   x2,   y1,   255, 255, 255);
    sge_VLine(gamescreen, x1,   y1,   y2,   255, 255, 255);
    sge_HLine(gamescreen, x1,   x2,   y2,   128, 128, 128);
    sge_VLine(gamescreen, x2,   y1,   y2,   128, 128, 128);
    sge_HLine(gamescreen, x1+1, x2-1, y2-1, 64,  64,  64 );
    sge_VLine(gamescreen, x2-1, y1+1, y2-1, 64,  64,  64 );
}

// Render text with transparent background (direct onto whatever is behind)
static void XlText(sge_TTFont* f, const char* s, Sint16 x, Sint16 y,
                   Uint8 r, Uint8 g, Uint8 b)
{
    if (!f || !s || !*s) return;
    sge_tt_textout(gamescreen, f, s, x, y, r, g, b, 0, 0, 0, -1);
}

// Render text on a solid colour background
static void XlTextBg(sge_TTFont* f, const char* s, Sint16 x, Sint16 y,
                     Uint8 r, Uint8 g, Uint8 b,
                     Uint8 br, Uint8 bg, Uint8 bb)
{
    if (!f || !s || !*s) return;
    sge_tt_textout(gamescreen, f, s, x, y, r, g, b, br, bg, bb, 255);
}

// Return pixel width of a string (0 if font is NULL)
static int XlTextW(sge_TTFont* f, const char* s)
{
    if (!f || !s || !*s) return 0;
    SDL_Rect r = sge_TTF_TextSize(f, s);
    return r.w;
}

// ---------------------------------------------------------------------------
// ShowBossKey
// ---------------------------------------------------------------------------

void ShowBossKey()
{
    const int W = gamescreen->w;
    const int H = gamescreen->h;

    // ── Save current screen ──────────────────────────────────────────────
    SDL_Surface* poBak = SDL_CreateRGBSurface(
        SDL_SWSURFACE, W, H,
        gamescreen->format->BitsPerPixel,
        gamescreen->format->Rmask,
        gamescreen->format->Gmask,
        gamescreen->format->Bmask,
        gamescreen->format->Amask);
    if (poBak)
        SDL_BlitSurface(gamescreen, NULL, poBak, NULL);

    // ── Silence music ────────────────────────────────────────────────────
    int iOldVol = g_oState.m_iMusicVolume;
    Audio->SetMusicVolume(0);

    // ── Change window caption ────────────────────────────────────────────
    SDL_WM_SetCaption("Microsoft Excel", "Microsoft Excel");

    // ── Load small font for spreadsheet text ─────────────────────────────
    std::string sFontPath = std::string(MSZ_DATADIR) + "/fonts/thin.ttf";
    sge_TTFont* poFont     = sge_TTF_OpenFont(sFontPath.c_str(), 11);
    sge_TTFont* poBold     = sge_TTF_OpenFont(sFontPath.c_str(), 11);
    if (poBold) sge_TTF_SetFontStyle(poBold, SGE_TTF_BOLD);

    // ── Layout constants ─────────────────────────────────────────────────
    // Scale column x-positions linearly if screen is wider than 640.
    // Base positions (for 640 px):
    //   col 0 = row-number col  (0..40)
    //   col 1 = A               (40..175)
    //   cols 2-7 = B..G         (70 px each)
    static const int kBaseColX[] = { 0, 40, 175, 245, 315, 385, 455, 525, 640 };
    int aColX[9];
    for (int i = 0; i < 9; ++i)
        aColX[i] = kBaseColX[i] * W / 640;
    aColX[8] = W - 16; // leave room for scrollbar

    const int Y_TITLE   = 0;
    const int Y_MENU    = 20;
    const int Y_TOOLBAR = 36;
    const int Y_FORMULA = 58;
    const int Y_COLHDR  = 76;
    const int Y_ROWS    = 94;
    const int ROW_H     = 22;
    const int Y_STATUS  = H - 16;

    // ── Title bar ────────────────────────────────────────────────────────
    XlBox(0, Y_TITLE, W, Y_MENU, 0, 0, 128);
    // Green "X" Excel icon placeholder
    XlBox(2, Y_TITLE+2, 16, Y_MENU-2, 0, 128, 0);
    XlText(poFont, "X", 4, Y_TITLE+3, 255, 255, 0);
    XlText(poFont, "Microsoft Excel - Q4_Sales_Budget_FINAL_v3.xlsx",
           20, Y_TITLE+4, 255, 255, 255);
    // Window control buttons: _, O, X
    Xl3DBox(W-53, Y_TITLE+2, W-39, Y_MENU-2);
    XlText(poFont, "_", W-50, Y_TITLE+3, 0, 0, 0);
    Xl3DBox(W-38, Y_TITLE+2, W-24, Y_MENU-2);
    XlText(poFont, "o", W-35, Y_TITLE+3, 0, 0, 0);
    Xl3DBox(W-23, Y_TITLE+2, W-2,  Y_MENU-2);
    XlTextBg(poFont, "X", W-18, Y_TITLE+3, 255, 255, 255, 192, 0, 0);

    // ── Menu bar ─────────────────────────────────────────────────────────
    XlBox(0, Y_MENU, W, Y_TOOLBAR, 212, 208, 200);
    {
        int mx = 4;
        for (int i = 0; s_apcMenuItems[i]; ++i)
        {
            XlText(poFont, s_apcMenuItems[i], mx, Y_MENU+2, 0, 0, 0);
            mx += XlTextW(poFont, s_apcMenuItems[i]) + 10;
        }
    }

    // ── Toolbar ──────────────────────────────────────────────────────────
    XlBox(0, Y_TOOLBAR, W, Y_FORMULA, 212, 208, 200);
    XlHLine(0, W, Y_TOOLBAR,    255, 255, 255);
    XlHLine(0, W, Y_FORMULA-1,  128, 128, 128);
    // Flat toolbar button stubs (16x16 each)
    for (int bx = 3; bx < W - 20; bx += 20)
        Xl3DBox(bx, Y_TOOLBAR+3, bx+16, Y_FORMULA-3);

    // ── Formula bar ──────────────────────────────────────────────────────
    XlBox(0, Y_FORMULA, W, Y_COLHDR, 212, 208, 200);
    XlHLine(0, W, Y_FORMULA, 128, 128, 128);
    // Name box (cell reference A1)
    Xl3DBox(0, Y_FORMULA+2, 58, Y_COLHDR-2);
    XlText(poFont, "A1", 4, Y_FORMULA+4, 0, 0, 0);
    XlVLine(60, Y_FORMULA, Y_COLHDR, 128, 128, 128);
    // Function buttons: fx
    XlText(poFont, "fx", 64, Y_FORMULA+4, 128, 0, 0);
    XlVLine(78, Y_FORMULA, Y_COLHDR, 180, 180, 180);
    // Formula / content preview
    Xl3DBox(80, Y_FORMULA+2, W-2, Y_COLHDR-2);
    XlText(poFont, "Product", 84, Y_FORMULA+4, 0, 0, 0);

    // ── Column headers ───────────────────────────────────────────────────
    XlBox(0, Y_COLHDR, W, Y_ROWS, 218, 214, 206);
    XlHLine(0, W, Y_COLHDR,   255, 255, 255);
    XlHLine(0, W, Y_ROWS-1,   128, 128, 128);
    for (int c = 0; c < 8; ++c)
    {
        XlVLine(aColX[c], Y_COLHDR, Y_ROWS, 128, 128, 128);
        if (c > 0 && poFont)
        {
            int cx  = aColX[c];
            int cw  = aColX[c+1] - cx;
            int tw  = XlTextW(poFont, s_apcColHeaders[c]);
            XlText(poFont, s_apcColHeaders[c],
                   cx + (cw - tw) / 2, Y_COLHDR + 3, 0, 0, 0);
        }
    }
    XlVLine(aColX[8], Y_COLHDR, H, 128, 128, 128);

    // ── Data rows ────────────────────────────────────────────────────────
    for (int row = 0; ; ++row)
    {
        int y1 = Y_ROWS + row * ROW_H;
        int y2 = y1 + ROW_H;
        if (y2 > Y_STATUS) break;

        bool bHaveData  = (row < NUM_ROWS);
        bool bBold      = bHaveData && s_aoRows[row].m_bBold;
        bool bHeaderRow = (row == 0);
        bool bTotalRow  = bHaveData && bBold && !bHeaderRow;

        // Row background colour
        Uint8 bgR, bgG, bgB;
        if      (bHeaderRow) { bgR=198; bgG=224; bgB=180; } // green tint
        else if (bTotalRow)  { bgR=255; bgG=255; bgB=204; } // yellow tint
        else if (row % 2)    { bgR=242; bgG=242; bgB=255; } // light blue stripe
        else                 { bgR=255; bgG=255; bgB=255; } // white

        XlBox(0, y1, W - 16, y2, bgR, bgG, bgB);

        // Row-number cell (always gray header style)
        XlBox(0, y1, aColX[1], y2, 218, 214, 206);
        if (poFont)
        {
            char acNum[8];
            snprintf(acNum, sizeof(acNum), "%d", row + 1);
            int tw = XlTextW(poFont, acNum);
            XlText(poFont, acNum,
                   aColX[1] - tw - 3, y1 + 4, 0, 0, 0);
        }

        // Data cells
        if (bHaveData)
        {
            sge_TTFont* pF = bBold ? poBold : poFont;
            if (!pF) pF = poFont;

            // Col A: product name (left-aligned)
            if (pF)
                XlText(pF, s_aoRows[row].m_pcName,
                       aColX[1] + 3, y1 + 4, 0, 0, 0);

            // Cols B-G: numbers / labels (right-aligned)
            for (int c = 0; c < 6; ++c)
            {
                const char* pcVal = s_aoRows[row].m_apcCells[c];
                if (!pcVal || !*pcVal) continue;
                if (!pF) continue;
                int cx  = aColX[c + 2];
                int cw  = aColX[c + 3] - cx;
                int tw  = XlTextW(pF, pcVal);
                int tx  = cx + cw - tw - 4;
                if (tx < cx + 2) tx = cx + 2;
                XlText(pF, pcVal, tx, y1 + 4, 0, 0, 0);
            }
        }

        // Grid lines
        XlHLine(0, W - 16, y2, 180, 180, 180);
        for (int c = 1; c < 8; ++c)
            XlVLine(aColX[c], y1, y2, 180, 180, 180);
    }

    // ── Status bar ───────────────────────────────────────────────────────
    XlBox(0, Y_STATUS, W, H, 212, 208, 200);
    XlHLine(0, W, Y_STATUS, 255, 255, 255);
    XlText(poFont, "Ready", 4, Y_STATUS + 2, 0, 0, 0);
    XlText(poFont, "Sheet1  /  Sheet2  /  Sheet3",
           W - 220, Y_STATUS + 2, 0, 0, 0);

    // ── Scroll bars ──────────────────────────────────────────────────────
    // Vertical
    XlBox(W-16, Y_ROWS,    W,    Y_STATUS, 212, 208, 200);
    XlVLine(W-16, Y_ROWS,  Y_STATUS, 128, 128, 128);
    Xl3DBox(W-15, Y_ROWS+2, W-2, Y_ROWS+30);
    // Horizontal
    XlBox(0, H-16, W-16, H, 212, 208, 200);
    XlHLine(0, W-16, H-16, 128, 128, 128);
    Xl3DBox(80, H-15, 120, H-2);

    SDL_Flip(gamescreen);

    // ── Wait for any key ─────────────────────────────────────────────────
    {
        SDL_Event ev;
        bool bDone = false;
        while (!bDone)
        {
            while (SDL_PollEvent(&ev))
            {
                if (ev.type == SDL_KEYDOWN)
                    bDone = true;
                else if (ev.type == SDL_QUIT)
                {
                    bDone = true;
                    g_oState.m_bQuitFlag = true;
                }
            }
            SDL_Delay(10);
        }
    }

    // ── Restore caption, volume, screen ──────────────────────────────────
    SDL_WM_SetCaption("OpenMortal", "OpenMortal");
    Audio->SetMusicVolume(iOldVol);

    if (poBak)
    {
        SDL_BlitSurface(poBak, NULL, gamescreen, NULL);
        SDL_FreeSurface(poBak);
        SDL_Flip(gamescreen);
    }

    if (poFont)  sge_TTF_CloseFont(poFont);
    if (poBold)  sge_TTF_CloseFont(poBold);
}

#endif // MSZ_BOSS_KEY
