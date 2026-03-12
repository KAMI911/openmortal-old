/***************************************************************************
                         BossKey.cpp  -  description
                            -------------------
   begin                : 2025
   copyright            : (C) 2025 OpenMortal contributors

   Pressing Shift+Esc overlays a fake Microsoft Excel 2003 spreadsheet on
   the screen and silences music until any key is pressed, then restores
   the previous screen.  Compiled in by default; disable with
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
// Spreadsheet content
// ---------------------------------------------------------------------------

struct SBossRow
{
    const char* m_pcName;
    const char* m_apcCells[6];   // Q1-Q4, YTD, Change
    bool        m_bBold;
};

static const SBossRow s_aoRows[] =
{
    { "Product",      { "Q1 2024", "Q2 2024", "Q3 2024", "Q4 2024", "YTD Total", "% Change" }, true  },
    { "Widget Alpha", { "12,450",  "13,200",  "14,890",  "16,234",  "56,774",    "+14.2%"  }, false },
    { "Widget Beta",  {  "8,234",   "7,981",   "9,103",  "10,456",  "35,774",    "+15.0%"  }, false },
    { "Gadget Pro",   { "34,100",  "32,500",  "36,800",  "41,200", "144,600",    "+12.1%"  }, false },
    { "Gadget Mini",  {  "5,670",   "6,120",   "5,890",   "7,340",  "25,020",    "+25.0%"  }, false },
    { "Deluxe Unit",  { "22,300",  "24,100",  "26,400",  "29,100", "101,900",    "+10.2%"  }, false },
    { "Budget Line",  {  "3,120",   "4,205",   "3,890",   "5,670",  "16,885",    "+81.7%"  }, false },
    { "",             {      "",       "",        "",        "",        "",           ""    }, false },
    { "TOTAL",        { "85,874",  "88,106",  "96,973", "110,000", "380,953",    "+13.7%"  }, true  },
    { "",             {      "",       "",        "",        "",        "",           ""    }, false },
    { "",             {      "",       "",        "",        "",        "",           ""    }, false },
    { "",             {      "",       "",        "",        "",        "",           ""    }, false },
};
static const int NUM_DATA_ROWS = (int)(sizeof(s_aoRows) / sizeof(s_aoRows[0]));

// "Selected" cell shown in the name box / formula bar and with a thick border.
// SEL_DATA_ROW=5 = "Deluxe Unit",  SEL_DATA_COL=2 = Q3 2024 = "26,400"
// Excel cell reference is C6  (col offset 2 → column C, data row 5 + header → row 6)
static const int SEL_DATA_ROW  = 5;
static const int SEL_DATA_COL  = 2;
static const char* SEL_CELLREF = "C6";
static const char* SEL_CONTENT = "26400";

// ---------------------------------------------------------------------------
// Module-level font handles (valid only inside ShowBossKey)
// ---------------------------------------------------------------------------
static sge_TTFont* s_pF  = NULL;   // 11 pt normal
static sge_TTFont* s_pFB = NULL;   // 11 pt bold
static sge_TTFont* s_pFS = NULL;   //  9 pt small (toolbar labels)

// ---------------------------------------------------------------------------
// Low-level drawing helpers
// ---------------------------------------------------------------------------

static void Fill(Sint16 x1, Sint16 y1, Sint16 x2, Sint16 y2,
                 Uint8 r, Uint8 g, Uint8 b)
{
    sge_FilledRect(gamescreen, x1, y1, x2, y2, r, g, b);
}

static void HLine(Sint16 x1, Sint16 x2, Sint16 y, Uint8 r, Uint8 g, Uint8 b)
{
    sge_HLine(gamescreen, x1, x2, y, r, g, b);
}

static void VLine(Sint16 x, Sint16 y1, Sint16 y2, Uint8 r, Uint8 g, Uint8 b)
{
    sge_VLine(gamescreen, x, y1, y2, r, g, b);
}

static void Outline(Sint16 x1, Sint16 y1, Sint16 x2, Sint16 y2,
                    Uint8 r, Uint8 g, Uint8 b)
{
    sge_Rect(gamescreen, x1, y1, x2, y2, r, g, b);
}

// Transparent-background text; returns rendered width.
static int Txt(sge_TTFont* f, const char* s, Sint16 x, Sint16 y,
               Uint8 r, Uint8 g, Uint8 b)
{
    if (!f || !s || !*s) return 0;
    return sge_tt_textout(gamescreen, f, s, x, y, r, g, b, 0, 0, 0, -1).w;
}

// Solid-background text.
static int TxtBg(sge_TTFont* f, const char* s, Sint16 x, Sint16 y,
                 Uint8 r, Uint8 g, Uint8 b, Uint8 br, Uint8 bg_, Uint8 bb)
{
    if (!f || !s || !*s) return 0;
    return sge_tt_textout(gamescreen, f, s, x, y, r, g, b, br, bg_, bb, 255).w;
}

static int TxtW(sge_TTFont* f, const char* s)
{
    if (!f || !s || !*s) return (int)(strlen(s) * 6);
    return sge_TTF_TextSize(f, s).w;
}

// ---------------------------------------------------------------------------
// Win-XP Luna style raised button border
// ---------------------------------------------------------------------------
static void Button3D(Sint16 x1, Sint16 y1, Sint16 x2, Sint16 y2)
{
    HLine(x1,   x2,   y1,   255, 255, 255);  // top-light
    VLine(x1,   y1,   y2,   255, 255, 255);  // left-light
    HLine(x1+1, x2-1, y2-1, 160, 157, 149);  // inner bottom
    VLine(x2-1, y1+1, y2-1, 160, 157, 149);  // inner right
    HLine(x1,   x2,   y2,   108, 108, 108);  // outer bottom
    VLine(x2,   y1,   y2,   108, 108, 108);  // outer right
}

// Sunken (pressed) button
static void Button3DIn(Sint16 x1, Sint16 y1, Sint16 x2, Sint16 y2)
{
    HLine(x1, x2, y1, 108, 108, 108);
    VLine(x1, y1, y2, 108, 108, 108);
    HLine(x1, x2, y2, 255, 255, 255);
    VLine(x2, y1, y2, 255, 255, 255);
}

// Flat dropdown box (sunken edge, white inside)
static void DropBox(Sint16 x1, Sint16 y1, Sint16 x2, Sint16 y2)
{
    Fill(x1, y1, x2, y2, 255, 255, 255);
    // thin border
    HLine(x1, x2, y1, 150, 150, 150);
    VLine(x1, y1, y2, 150, 150, 150);
    HLine(x1, x2, y2, 150, 150, 150);
    VLine(x2, y1, y2, 150, 150, 150);
    // small dropdown arrow button on the right
    Fill(x2-14, y1+1, x2-1, y2-1, 212, 208, 200);
    VLine(x2-14, y1+1, y2-1, 150, 150, 150);
    // small triangle arrow (3 lines)
    Sint16 ay = y1 + (y2-y1)/2 - 1;
    Sint16 ax = x2 - 8;
    HLine(ax-3, ax+3, ay,   60, 60, 60);
    HLine(ax-2, ax+2, ay+1, 60, 60, 60);
    HLine(ax-1, ax+1, ay+2, 60, 60, 60);
    HLine(ax,   ax,   ay+3, 60, 60, 60);
}

// Draw a small 18×18 toolbar icon button with a label
static void TbBtn(Sint16 x, Sint16 y, const char* label,
                  Uint8 iR, Uint8 iG, Uint8 iB, bool bActive = false)
{
    Fill(x, y, x+17, y+17, 234, 232, 220);
    if (bActive)
    {
        Button3DIn(x, y, x+17, y+17);
    }
    // no border on normal toolbar buttons in Excel 2003 (border appears on hover)
    if (s_pFS && label && *label)
    {
        int tw = TxtW(s_pFS, label);
        int th = sge_TTF_FontHeight(s_pFS);
        Txt(s_pFS, label,
            x + (18 - tw) / 2,
            y + (18 - th) / 2,
            iR, iG, iB);
    }
}

// Toolbar separator line
static void TbSep(Sint16 x, Sint16 y1, Sint16 y2)
{
    VLine(x,   y1, y2, 172, 168, 153);
    VLine(x+1, y1, y2, 255, 255, 255);
}

// ---------------------------------------------------------------------------
// Title bar (XP Luna blue gradient)
// ---------------------------------------------------------------------------
static void DrawTitleBar(int W, int Y_TITLE, int Y_MENU,
                         const char* pcFilename, sge_TTFont* f)
{
    // Gradient: top = (0,84,166)  bottom = (41,128,193)
    int h = Y_MENU - Y_TITLE;
    for (int dy = 0; dy < h; ++dy)
    {
        Uint8 r = (Uint8)(  0 + dy * 41 / h);
        Uint8 g = (Uint8)( 84 + dy * 44 / h);
        Uint8 b = (Uint8)(166 + dy * 27 / h);
        HLine(0, W, (Sint16)(Y_TITLE + dy), r, g, b);
    }

    // Excel 2003 icon: green "X" on white square
    Fill(3, Y_TITLE + 3, 17, Y_MENU - 3, 255, 255, 255);
    Fill(4, Y_TITLE + 4, 16, Y_MENU - 4,   0, 113,  44);  // dark green
    // Draw an X shape in white inside the green box
    sge_Line(gamescreen, 5,  Y_TITLE+5, 15, Y_MENU-5, 255, 255, 255);
    sge_Line(gamescreen, 6,  Y_TITLE+5, 15, Y_MENU-6, 255, 255, 255);
    sge_Line(gamescreen, 15, Y_TITLE+5, 5,  Y_MENU-5, 255, 255, 255);
    sge_Line(gamescreen, 15, Y_TITLE+6, 6,  Y_MENU-5, 255, 255, 255);

    // Title text
    char acTitle[128];
    snprintf(acTitle, sizeof(acTitle),
             "Microsoft Excel - %s", pcFilename);
    if (f) Txt(f, acTitle, 22, Y_TITLE + 4, 255, 255, 255);

    // Control buttons (Win XP style)
    int cy = Y_TITLE + 2;
    int ch = h - 4;
    // Minimize
    Fill(W-54, cy, W-38, cy+ch, 80, 130, 200);
    Outline(W-54, cy, W-38, cy+ch, 140, 185, 230);
    HLine(W-51, W-42, cy + ch/2 + 3, 255, 255, 255);
    HLine(W-51, W-42, cy + ch/2 + 2, 255, 255, 255);
    // Restore/Max
    Fill(W-37, cy, W-21, cy+ch, 80, 130, 200);
    Outline(W-37, cy, W-21, cy+ch, 140, 185, 230);
    Outline(W-35, cy+3, W-23, cy+ch-3, 255, 255, 255);
    HLine(W-35, W-23, cy+4, 255, 255, 255);  // double top line
    // Close (red)
    Fill(W-20, cy, W-3, cy+ch, 196,  50,  50);
    Outline(W-20, cy, W-3, cy+ch, 230, 100, 100);
    // white X on close button
    sge_Line(gamescreen, W-17, cy+3, W-6,  cy+ch-3, 255, 255, 255);
    sge_Line(gamescreen, W-16, cy+3, W-6,  cy+ch-4, 255, 255, 255);
    sge_Line(gamescreen, W-6,  cy+3, W-17, cy+ch-3, 255, 255, 255);
    sge_Line(gamescreen, W-6,  cy+4, W-17, cy+ch-3, 255, 255, 255);
}

// ---------------------------------------------------------------------------
// Menu bar
// ---------------------------------------------------------------------------
static void DrawMenuBar(int W, int Y_MENU, int Y_STDBAR)
{
    // Excel 2003 menu bar: warm silver (#EAE8DC)
    Fill(0, Y_MENU, W, Y_STDBAR, 234, 232, 220);
    HLine(0, W, Y_STDBAR - 1, 160, 158, 148);

    static const char* menus[] = {
        "File", "Edit", "View", "Insert", "Format", "Tools", "Data", "Window", "Help", NULL
    };
    int mx = 6;
    for (int i = 0; menus[i]; ++i)
    {
        TxtBg(s_pF, menus[i], mx, Y_MENU + 3, 0, 0, 0, 234, 232, 220);
        mx += TxtW(s_pF, menus[i]) + 10;
    }
}

// ---------------------------------------------------------------------------
// Standard toolbar
// ---------------------------------------------------------------------------
static void DrawStdToolbar(int W, int Y_STDBAR, int Y_FMTBAR)
{
    Fill(0, Y_STDBAR, W, Y_FMTBAR, 234, 232, 220);
    HLine(0, W, Y_FMTBAR - 1, 160, 158, 148);

    int x = 2;
    int y = Y_STDBAR + 1;
    // New, Open, Save
    TbBtn(x, y, "N", 30,  30, 200);  x += 19;
    TbBtn(x, y, "O", 200, 150,  0);  x += 19;
    TbBtn(x, y, "S",   0,   0, 180); x += 19;
    x += 3; TbSep(x, y + 2, y + 14); x += 5;
    // Print, Preview
    TbBtn(x, y, "P",  40,  40,  40); x += 19;
    TbBtn(x, y, "p",  40,  40,  40); x += 19;
    x += 3; TbSep(x, y + 2, y + 14); x += 5;
    // Spelling
    TbBtn(x, y, "AB", 180,   0,   0); x += 19;
    x += 3; TbSep(x, y + 2, y + 14); x += 5;
    // Cut, Copy, Paste, Format Painter
    TbBtn(x, y, "X",  80,  80,  80); x += 19;
    TbBtn(x, y, "C",  80,  80,  80); x += 19;
    TbBtn(x, y, "V",  80,  80,  80); x += 19;
    TbBtn(x, y, "FP", 200, 150,   0); x += 19;
    x += 3; TbSep(x, y + 2, y + 14); x += 5;
    // Undo, Redo
    TbBtn(x, y, "<Z", 30,  30, 180); x += 19;
    TbBtn(x, y, "Y>", 30,  30, 180); x += 19;
    x += 3; TbSep(x, y + 2, y + 14); x += 5;
    // Hyperlink, AutoSum, Sort
    TbBtn(x, y, "HL",   0,   0, 200); x += 19;
    TbBtn(x, y, "E",  180,   0,   0); x += 19;  // Sigma = E approx
    TbBtn(x, y, "AZ",  40,  40,  40); x += 19;
    TbBtn(x, y, "ZA",  40,  40,  40); x += 19;
    x += 3; TbSep(x, y + 2, y + 14); x += 5;
    // Chart, Drawing
    TbBtn(x, y, "Ch", 180,  80,   0); x += 19;
    TbBtn(x, y, "Dr", 200, 150,   0); x += 19;
    x += 3; TbSep(x, y + 2, y + 14); x += 5;

    // Zoom dropdown (right side)
    int zx = W - 62;
    DropBox(zx, y, zx + 50, y + 16);
    TxtBg(s_pFS, "100%", zx + 3, y + 2, 0, 0, 0, 255, 255, 255);

    // Help button
    TbBtn(W - 20, y, "?", 0, 0, 0);
}

// ---------------------------------------------------------------------------
// Formatting toolbar
// ---------------------------------------------------------------------------
static void DrawFmtToolbar(int W, int Y_FMTBAR, int Y_FORMULA)
{
    Fill(0, Y_FMTBAR, W, Y_FORMULA, 234, 232, 220);
    HLine(0, W, Y_FORMULA - 1, 160, 158, 148);

    int y  = Y_FMTBAR + 1;

    // Font name dropdown  (120 px)
    DropBox(2, y, 122, y + 16);
    TxtBg(s_pF, "Arial", 5, y + 2, 0, 0, 0, 255, 255, 255);
    // Font size dropdown (36 px)
    DropBox(126, y, 162, y + 16);
    TxtBg(s_pF, "10", 129, y + 2, 0, 0, 0, 255, 255, 255);

    int x = 166;
    TbSep(x, y + 2, y + 14); x += 5;

    // Bold, Italic, Underline
    TbBtn(x, y, "B",   0,   0,   0); x += 19;
    TbBtn(x, y, "I",   0,   0,   0); x += 19;
    TbBtn(x, y, "U",   0,   0,   0); x += 19;
    x += 3; TbSep(x, y + 2, y + 14); x += 5;

    // Alignment: left, center, right, merge
    TbBtn(x, y, "=L",  40,  40,  40); x += 19;
    TbBtn(x, y, "=C",  40,  40,  40); x += 19;
    TbBtn(x, y, "=R",  40,  40,  40); x += 19;
    TbBtn(x, y, "][",  40,  40,  40); x += 19;
    x += 3; TbSep(x, y + 2, y + 14); x += 5;

    // Number format: $, %, comma
    TbBtn(x, y, "$",   0, 128,   0); x += 19;
    TbBtn(x, y, "%",   0,   0, 128); x += 19;
    TbBtn(x, y, ",",  80,  80,  80); x += 19;
    x += 3; TbSep(x, y + 2, y + 14); x += 5;

    // Decimal places
    TbBtn(x, y, ".0+", 80,  80,  80); x += 19;
    TbBtn(x, y, ".0-", 80,  80,  80); x += 19;
    x += 3; TbSep(x, y + 2, y + 14); x += 5;

    // Indent
    TbBtn(x, y, ">>",  80,  80,  80); x += 19;
    TbBtn(x, y, "<<",  80,  80,  80); x += 19;
    x += 3; TbSep(x, y + 2, y + 14); x += 5;

    // Borders, Fill color, Font color
    TbBtn(x, y, "[B]", 40,  40,  40); x += 19;
    TbBtn(x, y, "A.",  200, 50,  50); x += 19;  // red fill
    TbBtn(x, y, "A_",  200, 50,  50); x += 19;  // red font
}

// ---------------------------------------------------------------------------
// Formula bar
// ---------------------------------------------------------------------------
static void DrawFormulaBar(int W, int Y_FORMULA, int Y_COLHDR)
{
    Fill(0, Y_FORMULA, W, Y_COLHDR, 234, 232, 220);
    HLine(0, W, Y_FORMULA,    160, 158, 148);
    HLine(0, W, Y_COLHDR - 1, 160, 158, 148);

    // Name box (cell reference, ~68 px)
    DropBox(2, Y_FORMULA + 2, 70, Y_COLHDR - 2);
    TxtBg(s_pF, SEL_CELLREF, 6, Y_FORMULA + 4, 0, 0, 0, 255, 255, 255);

    // Separator
    VLine(72, Y_FORMULA + 2, Y_COLHDR - 2, 160, 158, 148);

    // fx button
    int fxW = TxtW(s_pFB, "fx") + 6;
    Fill(74, Y_FORMULA + 2, 74 + fxW, Y_COLHDR - 2, 234, 232, 220);
    TxtBg(s_pFB, "fx", 77, Y_FORMULA + 4, 0, 0, 160, 234, 232, 220);
    VLine(74 + fxW, Y_FORMULA + 2, Y_COLHDR - 2, 160, 158, 148);

    // Formula / content box
    int fx0 = 74 + fxW + 4;
    Fill(fx0, Y_FORMULA + 2, W - 2, Y_COLHDR - 2, 255, 255, 255);
    Outline(fx0, Y_FORMULA + 2, W - 2, Y_COLHDR - 2, 150, 150, 150);
    TxtBg(s_pF, SEL_CONTENT, fx0 + 4, Y_FORMULA + 4, 0, 0, 0, 255, 255, 255);
}

// ---------------------------------------------------------------------------
// Column headers row
// ---------------------------------------------------------------------------
static void DrawColHeaders(int W, int Y_COLHDR, int Y_ROWS,
                           const int* aColX, int nCols, int iSelCol)
{
    static const char* kColNames[] = { "", "A", "B", "C", "D", "E", "F", "G", "H" };

    Fill(0, Y_COLHDR, W, Y_ROWS, 218, 218, 218);
    HLine(0, W, Y_COLHDR, 255, 255, 255);  // top highlight
    HLine(0, W, Y_ROWS - 1, 130, 130, 130);  // bottom shadow

    for (int c = 0; c < nCols; ++c)
    {
        int cx1 = aColX[c];
        int cx2 = aColX[c + 1];

        if (c == 0)
        {
            // Row-# column header corner cell (empty, slightly darker)
            Fill(cx1, Y_COLHDR, cx2, Y_ROWS, 206, 206, 206);
        }
        else
        {
            // Selected-column highlight (orange) or normal
            bool bSel = (c == iSelCol);
            if (bSel)
                Fill(cx1, Y_COLHDR, cx2, Y_ROWS, 246, 202, 152);
            else
                Fill(cx1, Y_COLHDR, cx2, Y_ROWS, 218, 218, 218);
        }

        // 3D border
        VLine(cx2 - 1, Y_COLHDR, Y_ROWS, 130, 130, 130);  // right shadow
        VLine(cx2,     Y_COLHDR, Y_ROWS, 130, 130, 130);

        // Column letter (centered)
        if (c > 0 && c < (int)(sizeof(kColNames)/sizeof(kColNames[0])) && s_pF)
        {
            const char* lbl = kColNames[c];
            int tw = TxtW(s_pF, lbl);
            int th = sge_TTF_FontHeight(s_pF);
            int tx = cx1 + (cx2 - cx1 - tw) / 2;
            int ty = Y_COLHDR + (Y_ROWS - Y_COLHDR - th) / 2;
            Txt(s_pF, lbl, tx, ty, 0, 0, 0);
        }
    }
    VLine(0, Y_COLHDR, Y_ROWS, 130, 130, 130);
}

// ---------------------------------------------------------------------------
// Data rows
// ---------------------------------------------------------------------------
static void DrawDataRows(int W, int Y_ROWS, int Y_TABS,
                         const int* aColX, int nCols,
                         int iSelRow, int iSelCol, int iRowH)
{
    for (int row = 0; ; ++row)
    {
        int y1 = Y_ROWS + row * iRowH;
        int y2 = y1 + iRowH;
        if (y2 > Y_TABS) break;

        bool bHaveData   = (row < NUM_DATA_ROWS);
        bool bBold       = bHaveData && s_aoRows[row].m_bBold;
        bool bHeaderRow  = (row == 0);  // first data row = column header row
        bool bTotalRow   = bBold && !bHeaderRow;

        // Row background
        Uint8 bgR, bgG, bgB;
        if      (bHeaderRow) { bgR = 198; bgG = 224; bgB = 180; }  // green tint (header)
        else if (bTotalRow)  { bgR = 255; bgG = 255; bgB = 204; }  // yellow (total)
        else if (row % 2)    { bgR = 242; bgG = 242; bgB = 255; }  // light blue stripe
        else                 { bgR = 255; bgG = 255; bgB = 255; }  // white

        Fill(0, y1, W - 16, y2, bgR, bgG, bgB);

        // Row-number cell (gray, right-aligned number)
        bool bSelRow = (row == iSelRow);
        Uint8 rhR = bSelRow ? 246 : 218;
        Uint8 rhG = bSelRow ? 202 : 218;
        Uint8 rhB = bSelRow ? 152 : 218;
        Fill(aColX[0], y1, aColX[1], y2, rhR, rhG, rhB);
        if (s_pFS)
        {
            char acNum[8];
            snprintf(acNum, sizeof(acNum), "%d", row + 1);
            int tw = TxtW(s_pFS, acNum);
            Txt(s_pFS, acNum, aColX[1] - tw - 3, y1 + 3, 0, 0, 0);
        }

        // Data cells
        if (bHaveData)
        {
            sge_TTFont* pF = bBold ? s_pFB : s_pF;
            if (!pF) pF = s_pF;

            // Column A: product name (left-aligned)
            if (pF)
                Txt(pF, s_aoRows[row].m_pcName, aColX[1] + 3, y1 + 3, 0, 0, 0);

            // Columns B-G: values (right-aligned)
            for (int c = 0; c < 6 && c + 2 < nCols + 1; ++c)
            {
                const char* pcVal = s_aoRows[row].m_apcCells[c];
                if (!pcVal || !*pcVal || !pF) continue;

                int cx1 = aColX[c + 2];
                int cx2 = aColX[c + 3];
                int tw  = TxtW(pF, pcVal);
                int tx  = cx2 - tw - 4;
                if (tx < cx1 + 2) tx = cx1 + 2;
                Txt(pF, pcVal, tx, y1 + 3, 0, 0, 0);
            }
        }

        // Selected cell: thick blue border around the single cell
        if (row == iSelRow && iSelCol >= 1 && iSelCol < nCols)
        {
            int sx1 = aColX[iSelCol];
            int sx2 = aColX[iSelCol + 1];
            // Thick blue border (2px)
            Outline(sx1,     y1,     sx2,     y2,     0,  0, 128);
            Outline(sx1 + 1, y1 + 1, sx2 - 1, y2 - 1, 0,  0, 128);
        }

        // Gridlines
        HLine(0, W - 16, y2, 212, 212, 212);
        for (int c = 1; c < nCols; ++c)
            VLine(aColX[c], y1, y2, 212, 212, 212);
    }
}

// ---------------------------------------------------------------------------
// Scroll bars
// ---------------------------------------------------------------------------
static void DrawScrollBars(int W, int H, int Y_ROWS, int Y_TABS)
{
    const int SW = 16;

    // Vertical scroll bar
    Fill(W - SW, Y_ROWS, W, Y_TABS, 230, 230, 230);
    VLine(W - SW, Y_ROWS, Y_TABS, 160, 160, 160);
    // Up arrow button
    Fill(W - SW + 1, Y_ROWS, W - 1, Y_ROWS + SW, 212, 208, 200);
    Button3D(W - SW + 1, Y_ROWS, W - 1, Y_ROWS + SW);
    // Down arrow button
    Fill(W - SW + 1, Y_TABS - SW, W - 1, Y_TABS, 212, 208, 200);
    Button3D(W - SW + 1, Y_TABS - SW, W - 1, Y_TABS);
    // Scroll thumb
    Fill(W - SW + 2, Y_ROWS + SW + 2, W - 2, Y_ROWS + SW + 28, 212, 208, 200);
    Button3D(W - SW + 1, Y_ROWS + SW + 1, W - 2, Y_ROWS + SW + 29);

    // Horizontal scroll bar
    Fill(0, Y_TABS, W - SW, H - 10, 230, 230, 230);
    HLine(0, W, Y_TABS, 160, 160, 160);
    // Left button
    Fill(1, Y_TABS + 1, SW, H - 11, 212, 208, 200);
    Button3D(1, Y_TABS + 1, SW, H - 11);
    // Right button
    Fill(W - SW - SW, Y_TABS + 1, W - SW - 1, H - 11, 212, 208, 200);
    Button3D(W - SW - SW, Y_TABS + 1, W - SW - 1, H - 11);
    // Scroll thumb
    Fill(SW + 2, Y_TABS + 2, SW + 32, H - 12, 212, 208, 200);
    Button3D(SW + 1, Y_TABS + 1, SW + 33, H - 11);
}

// ---------------------------------------------------------------------------
// Sheet tabs
// ---------------------------------------------------------------------------
static void DrawSheetTabs(int W, int H, int Y_TABS)
{
    const int Y_STATUS = H - 10;
    Fill(0, Y_TABS, W, Y_STATUS, 196, 196, 196);
    HLine(0, W, Y_TABS,   160, 160, 160);

    static const char* kTabs[] = { "Sheet1", "Sheet2", "Sheet3", NULL };
    int tx = 4;
    for (int i = 0; kTabs[i]; ++i)
    {
        int tw  = TxtW(s_pF, kTabs[i]) + 12;
        bool bActive = (i == 0);

        if (bActive)
        {
            // Active tab: white, no bottom line (connected to cell area)
            Fill(tx, Y_TABS + 2, tx + tw, Y_STATUS, 255, 255, 255);
            VLine(tx,       Y_TABS + 2, Y_STATUS, 160, 160, 160);
            HLine(tx,       tx + tw, Y_TABS + 2, 160, 160, 160);
            VLine(tx + tw,  Y_TABS + 2, Y_STATUS, 160, 160, 160);
            if (s_pF) TxtBg(s_pF, kTabs[i], tx + 6, Y_TABS + 4, 0, 0, 0, 255, 255, 255);
        }
        else
        {
            // Inactive tab: gray, with bottom line
            Fill(tx, Y_TABS + 4, tx + tw, Y_STATUS - 2, 210, 210, 210);
            VLine(tx,       Y_TABS + 4, Y_STATUS - 2, 160, 160, 160);
            HLine(tx,       tx + tw, Y_TABS + 4, 160, 160, 160);
            VLine(tx + tw,  Y_TABS + 4, Y_STATUS - 2, 160, 160, 160);
            HLine(tx,       tx + tw, Y_STATUS - 2, 160, 160, 160);
            if (s_pF) Txt(s_pF, kTabs[i], tx + 6, Y_TABS + 6, 60, 60, 60);
        }
        tx += tw + 2;
    }
}

// ---------------------------------------------------------------------------
// Status bar
// ---------------------------------------------------------------------------
static void DrawStatusBar(int W, int H)
{
    const int Y_STATUS = H - 10;
    Fill(0, Y_STATUS, W, H, 234, 232, 220);
    HLine(0, W, Y_STATUS, 160, 158, 148);

    if (s_pFS)
    {
        Txt(s_pFS, "Ready",       4,       Y_STATUS + 1, 0, 0, 0);
        Txt(s_pFS, "Sum=380,953", W - 180, Y_STATUS + 1, 0, 0, 0);
        Txt(s_pFS, "NUM",         W - 36,  Y_STATUS + 1, 0, 0, 0);
    }

    // Resize grip (bottom-right corner)
    for (int d = 2; d <= 8; d += 3)
    {
        HLine(W - d - 2, W - 2, H - d, 140, 140, 140);
        HLine(W - d - 1, W - 2, H - d, 255, 255, 255);
    }
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

    // ── Silence music, change caption ────────────────────────────────────
    int iOldVol = g_oState.m_iMusicVolume;
    Audio->SetMusicVolume(0);
    SDL_WM_SetCaption("Microsoft Excel", "Microsoft Excel");

    // ── Load fonts ───────────────────────────────────────────────────────
    std::string sFontPath = std::string(MSZ_DATADIR) + "/fonts/thin.ttf";
    s_pF  = sge_TTF_OpenFont(sFontPath.c_str(), 11);
    s_pFB = sge_TTF_OpenFont(sFontPath.c_str(), 11);
    s_pFS = sge_TTF_OpenFont(sFontPath.c_str(),  9);
    if (s_pFB) sge_TTF_SetFontStyle(s_pFB, SGE_TTF_BOLD);

    // ── Layout (scale columns proportionally if W > 640) ─────────────────
    //  col 0: row-number gutter  (35 px base)
    //  col 1: A – product names  (130 px base)
    //  cols 2-7: B-G numeric     (70 px each base)
    static const int kBase[] = { 0, 35, 165, 235, 305, 375, 445, 515, 585, 640 };
    const int nCols = 8;  // number of visible columns (including row-# col)
    int aColX[10];
    for (int i = 0; i < 10; ++i)
        aColX[i] = kBase[i] * W / 640;
    aColX[9] = W - 16;  // stop before vertical scrollbar

    const int Y_TITLE   = 0;
    const int Y_MENU    = 22;
    const int Y_STDBAR  = 40;
    const int Y_FMTBAR  = 60;
    const int Y_FORMULA = 80;
    const int Y_COLHDR  = 98;
    const int Y_ROWS    = 116;
    const int ROW_H     = 17;
    const int Y_TABS    = H - 20;

    // Selected column in aColX:
    //   SEL_DATA_COL=2 = Q3 2024 = column C = aColX index 3
    //   (col 0 = row#, col 1 = A, col 2 = B, col 3 = C)
    const int iSelColIdx = SEL_DATA_COL + 2;  // 0+row# +1(A) +col = index

    // ── Draw everything ──────────────────────────────────────────────────
    DrawTitleBar(W, Y_TITLE, Y_MENU, "Q4_Sales_Budget_FINAL_v3.xlsx", s_pF);
    DrawMenuBar(W, Y_MENU, Y_STDBAR);
    DrawStdToolbar(W, Y_STDBAR, Y_FMTBAR);
    DrawFmtToolbar(W, Y_FMTBAR, Y_FORMULA);
    DrawFormulaBar(W, Y_FORMULA, Y_COLHDR);
    DrawColHeaders(W, Y_COLHDR, Y_ROWS, aColX, nCols, iSelColIdx);
    DrawDataRows(W, Y_ROWS, Y_TABS, aColX, nCols,
                 SEL_DATA_ROW, iSelColIdx, ROW_H);
    DrawScrollBars(W, H, Y_ROWS, Y_TABS);
    DrawSheetTabs(W, H, Y_TABS);
    DrawStatusBar(W, H);

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

    // ── Restore ──────────────────────────────────────────────────────────
    SDL_WM_SetCaption("OpenMortal", "OpenMortal");
    Audio->SetMusicVolume(iOldVol);

    if (poBak)
    {
        SDL_BlitSurface(poBak, NULL, gamescreen, NULL);
        SDL_FreeSurface(poBak);
        SDL_Flip(gamescreen);
    }

    if (s_pF)  sge_TTF_CloseFont(s_pF);
    if (s_pFB) sge_TTF_CloseFont(s_pFB);
    if (s_pFS) sge_TTF_CloseFont(s_pFS);
    s_pF = s_pFB = s_pFS = NULL;
}

#endif // MSZ_BOSS_KEY
