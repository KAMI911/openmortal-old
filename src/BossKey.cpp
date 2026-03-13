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

// "Selected" cell is C6 = Deluxe Unit / Q3 2024 = "26,400"
static const int SEL_DATA_ROW  = 5;
static const int SEL_DATA_COL  = 2;   // 0=Q1, 1=Q2, 2=Q3 ...
static const char* SEL_CELLREF = "C6";
static const char* SEL_CONTENT = "26400";

// ---------------------------------------------------------------------------
// Font handles (valid only inside ShowBossKey)
// ---------------------------------------------------------------------------
static sge_TTFont* s_pF  = NULL;   // 11 pt regular  – cell text / menus
static sge_TTFont* s_pFB = NULL;   // 11 pt bold     – header / total rows
static sge_TTFont* s_pFS = NULL;   //  9 pt regular  – toolbar button labels

// ---------------------------------------------------------------------------
// System-font loading  (Liberation Sans → DejaVu → Ubuntu → Windows → game)
// ---------------------------------------------------------------------------
static sge_TTFont* LoadSysFont(int ptsize, bool bold)
{
    static const char* kRegular[] = {
        // Linux – Liberation (metric-compatible with Arial, looks most like Office)
        "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
        "/usr/share/fonts/truetype/liberation2/LiberationSans-Regular.ttf",
        "/usr/share/fonts/liberation/LiberationSans-Regular.ttf",
        // Linux – DejaVu Sans
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/dejavu/DejaVuSans.ttf",
        // Linux – FreeSans
        "/usr/share/fonts/truetype/freefont/FreeSans.ttf",
        // Linux – Ubuntu
        "/usr/share/fonts/truetype/ubuntu/Ubuntu-R.ttf",
        // macOS
        "/Library/Fonts/Arial.ttf",
        "/System/Library/Fonts/Helvetica.ttc",
        // Windows
        "C:/Windows/Fonts/arial.ttf",
        "C:\\Windows\\Fonts\\arial.ttf",
        NULL
    };
    static const char* kBold[] = {
        "/usr/share/fonts/truetype/liberation/LiberationSans-Bold.ttf",
        "/usr/share/fonts/truetype/liberation2/LiberationSans-Bold.ttf",
        "/usr/share/fonts/liberation/LiberationSans-Bold.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",
        "/usr/share/fonts/dejavu/DejaVuSans-Bold.ttf",
        "/usr/share/fonts/truetype/freefont/FreeSansBold.ttf",
        "/usr/share/fonts/truetype/ubuntu/Ubuntu-B.ttf",
        "/Library/Fonts/Arial Bold.ttf",
        "C:/Windows/Fonts/arialbd.ttf",
        "C:\\Windows\\Fonts\\arialbd.ttf",
        NULL
    };

    const char** paths = bold ? kBold : kRegular;
    for (int i = 0; paths[i]; ++i)
    {
        sge_TTFont* f = sge_TTF_OpenFont(paths[i], ptsize);
        if (f) return f;
    }

    // If no bold file found, load regular and fake-bold it
    if (bold)
    {
        sge_TTFont* f = LoadSysFont(ptsize, false);
        if (f) { sge_TTF_SetFontStyle(f, SGE_TTF_BOLD); return f; }
    }

    // Last resort: game's own font
    std::string sFallback = std::string(MSZ_DATADIR) + "/fonts/thin.ttf";
    return sge_TTF_OpenFont(sFallback.c_str(), ptsize);
}

// ---------------------------------------------------------------------------
// Drawing helpers
// ---------------------------------------------------------------------------

static void Fill(Sint16 x1, Sint16 y1, Sint16 x2, Sint16 y2,
                 Uint8 r, Uint8 g, Uint8 b)
{ sge_FilledRect(gamescreen, x1, y1, x2, y2, r, g, b); }

static void HLine(Sint16 x1, Sint16 x2, Sint16 y, Uint8 r, Uint8 g, Uint8 b)
{ sge_HLine(gamescreen, x1, x2, y, r, g, b); }

static void VLine(Sint16 x, Sint16 y1, Sint16 y2, Uint8 r, Uint8 g, Uint8 b)
{ sge_VLine(gamescreen, x, y1, y2, r, g, b); }

static void Outline(Sint16 x1, Sint16 y1, Sint16 x2, Sint16 y2,
                    Uint8 r, Uint8 g, Uint8 b)
{ sge_Rect(gamescreen, x1, y1, x2, y2, r, g, b); }

// Return pixel width of string
static int TxtW(sge_TTFont* f, const char* s)
{
    if (!s || !*s) return 0;
    if (!f) return (int)(strlen(s) * 6);
    return sge_TTF_TextSize(f, s).w;
}

// Ascent used as visual text height (excludes descenders = better centering)
static int TxtAsc(sge_TTFont* f)
{
    return f ? sge_TTF_FontAscent(f) : 10;
}

// Text centered in a box  (x1,y1)-(x2,y2), transparent bg
// Note: sge_tt_textout takes a BASELINE y, so add ascent to the centering offset.
static void TxtCenter(sge_TTFont* f, const char* s,
                      Sint16 x1, Sint16 y1, Sint16 x2, Sint16 y2,
                      Uint8 r, Uint8 g, Uint8 b)
{
    if (!f || !s || !*s) return;
    int bw  = x2 - x1;
    int bh  = y2 - y1;
    int tw  = TxtW(f, s);
    int asc = TxtAsc(f);
    Sint16 tx = (Sint16)(x1 + (bw - tw) / 2);
    Sint16 ty = (Sint16)(y1 + (bh + asc) / 2);
    sge_tt_textout(gamescreen, f, s, tx, ty, r, g, b, 0, 0, 0, -1);
}

// Text left-aligned, vertically centred in a row  (y1..y2), transparent bg
static void TxtVCL(sge_TTFont* f, const char* s,
                   Sint16 x, Sint16 y1, Sint16 y2,
                   Uint8 r, Uint8 g, Uint8 b)
{
    if (!f || !s || !*s) return;
    int asc = TxtAsc(f);
    Sint16 ty = (Sint16)(y1 + (y2 - y1 + asc) / 2);
    sge_tt_textout(gamescreen, f, s, x, ty, r, g, b, 0, 0, 0, -1);
}

// Text right-aligned, vertically centred in a row
static void TxtVCR(sge_TTFont* f, const char* s,
                   Sint16 xRight, Sint16 y1, Sint16 y2,
                   Uint8 r, Uint8 g, Uint8 b)
{
    if (!f || !s || !*s) return;
    int tw  = TxtW(f, s);
    int asc = TxtAsc(f);
    Sint16 tx = (Sint16)(xRight - tw - 3);
    Sint16 ty = (Sint16)(y1 + (y2 - y1 + asc) / 2);
    sge_tt_textout(gamescreen, f, s, tx, ty, r, g, b, 0, 0, 0, -1);
}

// ---------------------------------------------------------------------------
// UI component drawing helpers
// ---------------------------------------------------------------------------

// Win-XP raised 3-D border
static void Button3D(Sint16 x1, Sint16 y1, Sint16 x2, Sint16 y2)
{
    HLine(x1,   x2,   y1,   255, 255, 255);
    VLine(x1,   y1,   y2,   255, 255, 255);
    HLine(x1+1, x2-1, y2-1, 160, 157, 149);
    VLine(x2-1, y1+1, y2-1, 160, 157, 149);
    HLine(x1,   x2,   y2,   108, 108, 108);
    VLine(x2,   y1,   y2,   108, 108, 108);
}

// Sunken border
static void Button3DIn(Sint16 x1, Sint16 y1, Sint16 x2, Sint16 y2)
{
    HLine(x1, x2, y1, 108, 108, 108);
    VLine(x1, y1, y2, 108, 108, 108);
    HLine(x1, x2, y2, 255, 255, 255);
    VLine(x2, y1, y2, 255, 255, 255);
}

// Flat input box with thin border (white inside, gray border)
static void InputBox(Sint16 x1, Sint16 y1, Sint16 x2, Sint16 y2)
{
    Fill(x1, y1, x2, y2, 255, 255, 255);
    HLine(x1, x2, y1, 150, 150, 150);
    VLine(x1, y1, y2, 150, 150, 150);
    HLine(x1, x2, y2, 150, 150, 150);
    VLine(x2, y1, y2, 150, 150, 150);
}

// Dropdown box  (white field + dropdown arrow button on the right)
static void DropBox(Sint16 x1, Sint16 y1, Sint16 x2, Sint16 y2)
{
    InputBox(x1, y1, x2, y2);
    // Arrow panel
    Fill(x2-14, y1+1, x2-1, y2-1, 212, 208, 200);
    VLine(x2-14, y1+1, y2-1, 150, 150, 150);
    Button3D(x2-14, y1+1, x2-1, y2-1);
    // Small down-arrow triangle
    Sint16 ax = (Sint16)(x2 - 7);
    Sint16 ay = (Sint16)(y1 + (y2 - y1) / 2 - 1);
    HLine(ax-3, ax+3, ay,   50, 50, 50);
    HLine(ax-2, ax+2, ay+1, 50, 50, 50);
    HLine(ax-1, ax+1, ay+2, 50, 50, 50);
    HLine(ax,   ax,   ay+3, 50, 50, 50);
}

// Toolbar separator  (thin vertical line)
static void TbSep(Sint16 x, Sint16 y1, Sint16 y2)
{
    VLine(x,   y1, y2, 160, 158, 148);
    VLine(x+1, y1, y2, 255, 255, 255);
}

// 20×20 toolbar button  (label centred, no hover border in Excel 2003 default state)
static void TbBtn(Sint16 x, Sint16 y,
                  const char* label, Uint8 iR, Uint8 iG, Uint8 iB,
                  bool bActive = false)
{
    Fill(x, y, x+19, y+19, 234, 232, 220);
    if (bActive) Button3DIn(x, y, x+19, y+19);
    TxtCenter(s_pFS, label, x, y, x+19, y+19, iR, iG, iB);
}

// ---------------------------------------------------------------------------
// Title bar  (XP Luna gradient)
// ---------------------------------------------------------------------------
static void DrawTitleBar(int W, int Y_TITLE, int Y_MENU,
                         const char* pcFilename)
{
    int h = Y_MENU - Y_TITLE;
    for (int dy = 0; dy < h; ++dy)
    {
        Uint8 r = (Uint8)(  0 + dy * 41 / h);
        Uint8 g = (Uint8)( 84 + dy * 44 / h);
        Uint8 b = (Uint8)(166 + dy * 27 / h);
        HLine(0, (Sint16)W, (Sint16)(Y_TITLE + dy), r, g, b);
    }

    // Excel green-X icon
    Fill(3, Y_TITLE+3, 17, Y_MENU-3, 255, 255, 255);
    Fill(4, Y_TITLE+4, 16, Y_MENU-4,   0, 113,  44);
    sge_Line(gamescreen,  5, Y_TITLE+5, 15, Y_MENU-5, 255, 255, 255);
    sge_Line(gamescreen,  6, Y_TITLE+5, 15, Y_MENU-6, 255, 255, 255);
    sge_Line(gamescreen, 15, Y_TITLE+5,  5, Y_MENU-5, 255, 255, 255);
    sge_Line(gamescreen, 15, Y_TITLE+6,  6, Y_MENU-5, 255, 255, 255);

    // Title text
    char acTitle[128];
    snprintf(acTitle, sizeof(acTitle), "Microsoft Excel - %s", pcFilename);
    TxtVCL(s_pF, acTitle, 22, Y_TITLE, Y_MENU, 255, 255, 255);

    // Window control buttons
    int cy = Y_TITLE + 2;
    int ch = h - 4;
    // Minimize
    Fill(W-54, cy, W-38, cy+ch, 80, 130, 200);
    Outline(W-54, cy, W-38, cy+ch, 140, 185, 230);
    HLine(W-51, W-41, (Sint16)(cy + ch/2 + 3), 255, 255, 255);
    HLine(W-51, W-41, (Sint16)(cy + ch/2 + 2), 255, 255, 255);
    // Restore
    Fill(W-37, cy, W-21, cy+ch, 80, 130, 200);
    Outline(W-37, cy, W-21, cy+ch, 140, 185, 230);
    Outline(W-35, cy+3, W-23, cy+ch-3, 255, 255, 255);
    HLine(W-35, W-23, (Sint16)(cy+4), 255, 255, 255);
    // Close (red)
    Fill(W-20, cy, W-3, cy+ch, 196, 50, 50);
    Outline(W-20, cy, W-3, cy+ch, 230, 100, 100);
    sge_Line(gamescreen, W-17, cy+3, W-6, cy+ch-3, 255, 255, 255);
    sge_Line(gamescreen, W-16, cy+3, W-6, cy+ch-4, 255, 255, 255);
    sge_Line(gamescreen, W-6,  cy+3, W-17, cy+ch-3, 255, 255, 255);
    sge_Line(gamescreen, W-6,  cy+4, W-17, cy+ch-3, 255, 255, 255);
}

// ---------------------------------------------------------------------------
// Menu bar
// ---------------------------------------------------------------------------
static void DrawMenuBar(int W, int Y_MENU, int Y_STDBAR)
{
    Fill(0, Y_MENU, W, Y_STDBAR, 234, 232, 220);
    HLine(0, (Sint16)W, (Sint16)(Y_STDBAR-1), 160, 158, 148);

    static const char* kMenus[] = {
        "File", "Edit", "View", "Insert", "Format",
        "Tools", "Data", "Window", "Help", NULL
    };
    int mx = 6;
    for (int i = 0; kMenus[i]; ++i)
    {
        TxtVCL(s_pF, kMenus[i], mx, Y_MENU, Y_STDBAR, 0, 0, 0);
        mx += TxtW(s_pF, kMenus[i]) + 10;
    }
}

// ---------------------------------------------------------------------------
// Standard toolbar
// ---------------------------------------------------------------------------
static void DrawStdToolbar(int W, int Y_STDBAR, int Y_FMTBAR)
{
    Fill(0, Y_STDBAR, W, Y_FMTBAR, 234, 232, 220);
    HLine(0, (Sint16)W, (Sint16)(Y_FMTBAR-1), 160, 158, 148);

    Sint16 x = 2;
    Sint16 y = (Sint16)(Y_STDBAR + 1);

    TbBtn(x, y, "N",  30,  30, 200); x += 21;   // New
    TbBtn(x, y, "O", 200, 150,   0); x += 21;   // Open
    TbBtn(x, y, "S",   0,   0, 180); x += 21;   // Save
    x += 4; TbSep(x, y+2, y+15); x += 6;
    TbBtn(x, y, "P",  40,  40,  40); x += 21;   // Print
    TbBtn(x, y, "Q",  40,  40,  40); x += 21;   // Print Preview (magnifier)
    x += 4; TbSep(x, y+2, y+15); x += 6;
    TbBtn(x, y, "Abc",180,  0,   0); x += 21;   // Spelling
    x += 4; TbSep(x, y+2, y+15); x += 6;
    TbBtn(x, y, "X",  80,  80,  80); x += 21;   // Cut
    TbBtn(x, y, "C",  80,  80,  80); x += 21;   // Copy
    TbBtn(x, y, "V",  80,  80,  80); x += 21;   // Paste
    TbBtn(x, y, "F",  200,150,   0); x += 21;   // Format Painter
    x += 4; TbSep(x, y+2, y+15); x += 6;
    TbBtn(x, y, "<",  30,  30, 180); x += 21;   // Undo
    TbBtn(x, y, ">",  30,  30, 180); x += 21;   // Redo
    x += 4; TbSep(x, y+2, y+15); x += 6;
    TbBtn(x, y, "@",   0,   0, 200); x += 21;   // Hyperlink
    TbBtn(x, y, "=",  180,  0,   0); x += 21;   // AutoSum (Sigma)
    TbBtn(x, y, "Az",  40, 40,  40); x += 21;   // Sort A-Z
    TbBtn(x, y, "Za",  40, 40,  40); x += 21;   // Sort Z-A
    x += 4; TbSep(x, y+2, y+15); x += 6;
    TbBtn(x, y, "ch", 180, 80,   0); x += 21;   // Chart Wizard
    TbBtn(x, y, "dr", 200,150,   0); x += 21;   // Drawing
    x += 4; TbSep(x, y+2, y+15); x += 6;

    // Zoom dropdown (right side)
    Sint16 zx = (Sint16)(W - 64);
    DropBox(zx, y, zx+52, y+16);
    TxtVCL(s_pFS, "100%", zx+3, y, y+16, 0, 0, 0);
    // Help
    TbBtn((Sint16)(W-21), y, "?", 0, 0, 0);
}

// ---------------------------------------------------------------------------
// Formatting toolbar
// ---------------------------------------------------------------------------
static void DrawFmtToolbar(int W, int Y_FMTBAR, int Y_FORMULA)
{
    Fill(0, Y_FMTBAR, W, Y_FORMULA, 234, 232, 220);
    HLine(0, (Sint16)W, (Sint16)(Y_FORMULA-1), 160, 158, 148);

    Sint16 y = (Sint16)(Y_FMTBAR + 1);

    // Font name dropdown (120 px)
    DropBox(2, y, 122, y+16);
    TxtVCL(s_pF, "Arial", 5, y, y+16, 0, 0, 0);
    // Font size dropdown (36 px)
    DropBox(126, y, 162, y+16);
    TxtVCL(s_pF, "10", 129, y, y+16, 0, 0, 0);

    Sint16 x = 166;
    x += 4; TbSep(x, y+2, y+15); x += 6;
    TbBtn(x, y, "B",   0,   0,   0); x += 21;   // Bold
    TbBtn(x, y, "I",   0,   0,   0); x += 21;   // Italic
    TbBtn(x, y, "U",   0,   0,   0); x += 21;   // Underline
    x += 4; TbSep(x, y+2, y+15); x += 6;
    TbBtn(x, y, "L",  40,  40,  40); x += 21;   // Align Left
    TbBtn(x, y, "C",  40,  40,  40); x += 21;   // Center
    TbBtn(x, y, "R",  40,  40,  40); x += 21;   // Align Right
    TbBtn(x, y, "M",  40,  40,  40); x += 21;   // Merge & Center
    x += 4; TbSep(x, y+2, y+15); x += 6;
    TbBtn(x, y, "$",   0, 128,   0); x += 21;   // Currency
    TbBtn(x, y, "%",   0,   0, 128); x += 21;   // Percent
    TbBtn(x, y, ",",  80,  80,  80); x += 21;   // Thousands separator
    x += 4; TbSep(x, y+2, y+15); x += 6;
    TbBtn(x, y, ".0", 80,  80,  80); x += 21;   // Increase decimal
    TbBtn(x, y, "0.", 80,  80,  80); x += 21;   // Decrease decimal
    x += 4; TbSep(x, y+2, y+15); x += 6;
    TbBtn(x, y, "|>", 80,  80,  80); x += 21;   // Increase indent
    TbBtn(x, y, "<|", 80,  80,  80); x += 21;   // Decrease indent
    x += 4; TbSep(x, y+2, y+15); x += 6;
    TbBtn(x, y, "#",  40,  40,  40); x += 21;   // Borders
    TbBtn(x, y, "a",  200, 50,  50); x += 21;   // Fill color  (a with bg)
    TbBtn(x, y, "A",  200, 50,  50);             // Font color  (A underlined)
}

// ---------------------------------------------------------------------------
// Formula bar
// ---------------------------------------------------------------------------
static void DrawFormulaBar(int W, int Y_FORMULA, int Y_COLHDR)
{
    Fill(0, Y_FORMULA, W, Y_COLHDR, 234, 232, 220);
    HLine(0, (Sint16)W, (Sint16)(Y_FORMULA),    150, 150, 150);
    HLine(0, (Sint16)W, (Sint16)(Y_COLHDR - 1), 150, 150, 150);

    // Name box  (cell reference)
    DropBox(2, Y_FORMULA+2, 68, Y_COLHDR-2);
    TxtCenter(s_pF, SEL_CELLREF, 2, Y_FORMULA+2, 68, Y_COLHDR-2, 0, 0, 0);

    // "fx" button
    int fxW = TxtW(s_pFB, "fx") + 10;
    Fill(70, Y_FORMULA+2, 70+fxW, Y_COLHDR-2, 234, 232, 220);
    TxtCenter(s_pFB, "fx", 70, Y_FORMULA+2, 70+fxW, Y_COLHDR-2, 0, 0, 160);
    VLine(70,       (Sint16)(Y_FORMULA+2), (Sint16)(Y_COLHDR-2), 150, 150, 150);
    VLine(70+fxW,   (Sint16)(Y_FORMULA+2), (Sint16)(Y_COLHDR-2), 150, 150, 150);

    // Formula / content input box
    Sint16 fx0 = (Sint16)(70 + fxW + 4);
    InputBox(fx0, Y_FORMULA+2, W-2, Y_COLHDR-2);
    TxtVCL(s_pF, SEL_CONTENT, fx0+4, Y_FORMULA+2, Y_COLHDR-2, 0, 0, 0);
}

// ---------------------------------------------------------------------------
// Column headers
// ---------------------------------------------------------------------------
static void DrawColHeaders(int W, int Y_COLHDR, int Y_ROWS,
                           const int* aColX, int nCols, int iSelCol)
{
    static const char* kNames[] = { "", "A","B","C","D","E","F","G","H","I" };

    Fill(0, Y_COLHDR, W, Y_ROWS, 218, 218, 218);
    HLine(0, (Sint16)W, (Sint16)(Y_COLHDR),   255, 255, 255);
    HLine(0, (Sint16)W, (Sint16)(Y_ROWS - 1),  130, 130, 130);

    for (int c = 0; c < nCols; ++c)
    {
        Sint16 cx1 = (Sint16)aColX[c];
        Sint16 cx2 = (Sint16)aColX[c + 1];

        if (c == 0)
            Fill(cx1, Y_COLHDR, cx2, Y_ROWS, 206, 206, 206);
        else if (c == iSelCol)
            Fill(cx1, Y_COLHDR, cx2, Y_ROWS, 246, 202, 152);
        else
            Fill(cx1, Y_COLHDR, cx2, Y_ROWS, 218, 218, 218);

        // Column letter – centered
        if (c > 0 && c < (int)(sizeof(kNames)/sizeof(kNames[0])))
            TxtCenter(s_pF, kNames[c], cx1, Y_COLHDR, cx2, Y_ROWS, 0, 0, 0);

        // Right border
        VLine(cx2-1, (Sint16)Y_COLHDR, (Sint16)Y_ROWS, 130, 130, 130);
        VLine(cx2,   (Sint16)Y_COLHDR, (Sint16)Y_ROWS, 130, 130, 130);
    }
    VLine(0, (Sint16)Y_COLHDR, (Sint16)Y_ROWS, 130, 130, 130);
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

        bool bHaveData  = (row < NUM_DATA_ROWS);
        bool bBold      = bHaveData && s_aoRows[row].m_bBold;
        bool bHeaderRow = (row == 0);
        bool bTotalRow  = bBold && !bHeaderRow;

        Uint8 bgR, bgG, bgB;
        if      (bHeaderRow) { bgR=198; bgG=224; bgB=180; }
        else if (bTotalRow)  { bgR=255; bgG=255; bgB=204; }
        else if (row % 2)    { bgR=242; bgG=242; bgB=255; }
        else                 { bgR=255; bgG=255; bgB=255; }

        Fill(0, (Sint16)y1, (Sint16)(W-16), (Sint16)y2, bgR, bgG, bgB);

        // Row-number gutter
        bool  bSelRow = (row == iSelRow);
        Uint8 rhR = bSelRow ? 246 : 218;
        Uint8 rhG = bSelRow ? 202 : 218;
        Uint8 rhB = bSelRow ? 152 : 218;
        Fill((Sint16)aColX[0], (Sint16)y1,
             (Sint16)aColX[1], (Sint16)y2, rhR, rhG, rhB);

        char acNum[8];
        snprintf(acNum, sizeof(acNum), "%d", row + 1);
        TxtVCR(s_pFS, acNum,
               (Sint16)aColX[1], (Sint16)y1, (Sint16)y2, 0, 0, 0);

        // Cell content
        if (bHaveData)
        {
            sge_TTFont* pF = bBold ? s_pFB : s_pF;
            if (!pF) pF = s_pF;

            // Col A: left-aligned product name
            TxtVCL(pF, s_aoRows[row].m_pcName,
                   (Sint16)(aColX[1] + 3), (Sint16)y1, (Sint16)y2, 0, 0, 0);

            // Cols B-G: right-aligned numbers
            for (int c = 0; c < 6 && (c + 2) < nCols; ++c)
            {
                const char* pcVal = s_aoRows[row].m_apcCells[c];
                if (!pcVal || !*pcVal || !pF) continue;
                TxtVCR(pF, pcVal,
                       (Sint16)aColX[c + 3],
                       (Sint16)y1, (Sint16)y2, 0, 0, 0);
            }
        }

        // Selected cell: 2-px dark-blue border
        if (row == iSelRow && iSelCol >= 1 && iSelCol < nCols)
        {
            Sint16 sx1 = (Sint16)aColX[iSelCol];
            Sint16 sx2 = (Sint16)aColX[iSelCol + 1];
            Outline(sx1,   (Sint16)y1,   sx2,   (Sint16)y2,   0, 0, 128);
            Outline(sx1+1, (Sint16)(y1+1), sx2-1, (Sint16)(y2-1), 0, 0, 128);
        }

        // Grid lines
        HLine(0, (Sint16)(W-16), (Sint16)y2, 212, 212, 212);
        for (int c = 1; c < nCols; ++c)
            VLine((Sint16)aColX[c], (Sint16)y1, (Sint16)y2, 212, 212, 212);
    }
}

// ---------------------------------------------------------------------------
// Scroll bars
// ---------------------------------------------------------------------------
static void DrawScrollBars(int W, int H, int Y_ROWS, int Y_TABS)
{
    const int SW = 16;
    // Vertical
    Fill(W-SW, Y_ROWS, W, Y_TABS, 230, 230, 230);
    VLine(W-SW, Y_ROWS, Y_TABS, 160, 160, 160);
    Fill(W-SW+1, Y_ROWS+1,      W-1, Y_ROWS+SW,   212, 208, 200);
    Button3D(W-SW+1, Y_ROWS+1,  W-1, Y_ROWS+SW);
    // Up arrow triangle
    {
        Sint16 cx = (Sint16)(W - SW/2);
        Sint16 cy = (Sint16)(Y_ROWS + SW/2);
        HLine(cx,   cx,   cy-2, 80, 80, 80);
        HLine(cx-1, cx+1, cy-1, 80, 80, 80);
        HLine(cx-2, cx+2, cy,   80, 80, 80);
        HLine(cx-3, cx+3, cy+1, 80, 80, 80);
    }
    Fill(W-SW+1, Y_TABS-SW,     W-1, Y_TABS,       212, 208, 200);
    Button3D(W-SW+1, Y_TABS-SW, W-1, Y_TABS);
    // Down arrow triangle
    {
        Sint16 cx = (Sint16)(W - SW/2);
        Sint16 cy = (Sint16)(Y_TABS - SW/2);
        HLine(cx-3, cx+3, cy-1, 80, 80, 80);
        HLine(cx-2, cx+2, cy,   80, 80, 80);
        HLine(cx-1, cx+1, cy+1, 80, 80, 80);
        HLine(cx,   cx,   cy+2, 80, 80, 80);
    }
    Fill(W-SW+2, Y_ROWS+SW+2,   W-2, Y_ROWS+SW+28, 212, 208, 200);
    Button3D(W-SW+1, Y_ROWS+SW+1, W-2, Y_ROWS+SW+29);
    // Horizontal
    Fill(0, Y_TABS, W-SW, H-10, 230, 230, 230);
    HLine(0, W, (Sint16)Y_TABS, 160, 160, 160);
    Fill(1, Y_TABS+1, SW, H-11,       212, 208, 200);
    Button3D(1, Y_TABS+1, SW, H-11);
    Fill(W-SW-SW, Y_TABS+1, W-SW-1, H-11, 212, 208, 200);
    Button3D(W-SW-SW, Y_TABS+1, W-SW-1, H-11);
    Fill(SW+2, Y_TABS+2, SW+32, H-12, 212, 208, 200);
    Button3D(SW+1, Y_TABS+1, SW+33, H-11);
}

// ---------------------------------------------------------------------------
// Sheet tabs
// ---------------------------------------------------------------------------
static void DrawSheetTabs(int W, int H, int Y_TABS)
{
    const int Y_STATUS = H - 10;
    Fill(0, Y_TABS, W, Y_STATUS, 200, 200, 200);
    HLine(0, W, (Sint16)Y_TABS, 160, 160, 160);

    static const char* kTabs[] = { "Sheet1", "Sheet2", "Sheet3", NULL };
    int tx = 4;
    for (int i = 0; kTabs[i]; ++i)
    {
        int tw = TxtW(s_pF, kTabs[i]) + 14;
        if (i == 0)
        {
            // Active
            Fill((Sint16)tx, (Sint16)(Y_TABS+1), (Sint16)(tx+tw), (Sint16)(Y_STATUS),
                 255, 255, 255);
            VLine(tx,    (Sint16)(Y_TABS+1), (Sint16)(Y_STATUS), 160, 160, 160);
            HLine((Sint16)tx, (Sint16)(tx+tw), (Sint16)(Y_TABS+1), 160, 160, 160);
            VLine(tx+tw, (Sint16)(Y_TABS+1), (Sint16)(Y_STATUS), 160, 160, 160);
            TxtCenter(s_pF, kTabs[i],
                      (Sint16)tx, (Sint16)(Y_TABS+1),
                      (Sint16)(tx+tw), (Sint16)Y_STATUS, 0, 0, 0);
        }
        else
        {
            // Inactive
            Fill((Sint16)tx, (Sint16)(Y_TABS+3), (Sint16)(tx+tw), (Sint16)(Y_STATUS-1),
                 214, 214, 214);
            VLine(tx,    (Sint16)(Y_TABS+3), (Sint16)(Y_STATUS-1), 160, 160, 160);
            HLine((Sint16)tx, (Sint16)(tx+tw), (Sint16)(Y_TABS+3), 160, 160, 160);
            VLine(tx+tw, (Sint16)(Y_TABS+3), (Sint16)(Y_STATUS-1), 160, 160, 160);
            HLine((Sint16)tx, (Sint16)(tx+tw), (Sint16)(Y_STATUS-1), 160, 160, 160);
            TxtCenter(s_pF, kTabs[i],
                      (Sint16)tx, (Sint16)(Y_TABS+3),
                      (Sint16)(tx+tw), (Sint16)(Y_STATUS-1), 60, 60, 60);
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
    HLine(0, (Sint16)W, (Sint16)Y_STATUS, 160, 158, 148);

    TxtVCL(s_pFS, "Ready",       4,       Y_STATUS, H, 0, 0, 0);
    TxtVCL(s_pFS, "Sum=380,953", W-180,   Y_STATUS, H, 0, 0, 0);
    TxtVCL(s_pFS, "NUM",         W-36,    Y_STATUS, H, 0, 0, 0);

    // Resize grip
    for (int d = 2; d <= 8; d += 3)
    {
        HLine((Sint16)(W-d-2), (Sint16)(W-2), (Sint16)(H-d), 140, 140, 140);
        HLine((Sint16)(W-d-1), (Sint16)(W-2), (Sint16)(H-d), 255, 255, 255);
    }
}

// ---------------------------------------------------------------------------
// ShowBossKey – main entry point
// ---------------------------------------------------------------------------

void ShowBossKey()
{
    const int W = gamescreen->w;
    const int H = gamescreen->h;

    // Save screen
    SDL_Surface* poBak = SDL_CreateRGBSurface(
        SDL_SWSURFACE, W, H,
        gamescreen->format->BitsPerPixel,
        gamescreen->format->Rmask, gamescreen->format->Gmask,
        gamescreen->format->Bmask, gamescreen->format->Amask);
    if (poBak) SDL_BlitSurface(gamescreen, NULL, poBak, NULL);

    // Silence music + change caption
    int iOldVol = g_oState.m_iMusicVolume;
    Audio->SetMusicVolume(0);
    SDL_WM_SetCaption("Microsoft Excel", "Microsoft Excel");

    // Load fonts: prefer Liberation Sans (Arial-compatible), fall back to game font
    s_pF  = LoadSysFont(11, false);
    s_pFB = LoadSysFont(11, true);
    s_pFS = LoadSysFont( 9, false);

    // Column layout (base positions for 640 px, scaled proportionally)
    static const int kBase[] = { 0, 35, 165, 235, 305, 375, 445, 515, 585, 640 };
    const int nCols = 8;
    int aColX[10];
    for (int i = 0; i < 10; ++i)
        aColX[i] = kBase[i] * W / 640;
    aColX[9] = W - 16;

    // Y layout
    const int Y_TITLE   = 0;
    const int Y_MENU    = 22;
    const int Y_STDBAR  = 40;
    const int Y_FMTBAR  = 61;
    const int Y_FORMULA = 82;
    const int Y_COLHDR  = 100;
    const int Y_ROWS    = 118;
    const int ROW_H     = 17;
    const int Y_TABS    = H - 20;

    // iSelColIdx: col 0=row#, 1=A, 2=B, 3=C → SEL_DATA_COL(=2) + 2 = 4... wait
    // aColX[0]=row#, aColX[1]=A, aColX[2]=B, aColX[3]=C
    // SEL_DATA_COL=2 means Q3 = third data column = column C = aColX index 3
    const int iSelColIdx = SEL_DATA_COL + 2;   // 0-based into aColX

    // Draw
    DrawTitleBar   (W, Y_TITLE, Y_MENU, "Q4_Sales_Budget_FINAL_v3.xlsx");
    DrawMenuBar    (W, Y_MENU,  Y_STDBAR);
    DrawStdToolbar (W, Y_STDBAR, Y_FMTBAR);
    DrawFmtToolbar (W, Y_FMTBAR, Y_FORMULA);
    DrawFormulaBar (W, Y_FORMULA, Y_COLHDR);
    DrawColHeaders (W, Y_COLHDR, Y_ROWS,  aColX, nCols, iSelColIdx);
    DrawDataRows   (W, Y_ROWS,   Y_TABS,  aColX, nCols,
                    SEL_DATA_ROW, iSelColIdx, ROW_H);
    DrawScrollBars (W, H, Y_ROWS, Y_TABS);
    DrawSheetTabs  (W, H, Y_TABS);
    DrawStatusBar  (W, H);

    SDL_Flip(gamescreen);

    // Wait for any key
    {
        SDL_Event ev;
        bool bDone = false;
        while (!bDone)
        {
            while (SDL_PollEvent(&ev))
            {
                if      (ev.type == SDL_KEYDOWN) bDone = true;
                else if (ev.type == SDL_QUIT)
                {
                    bDone = true;
                    g_oState.m_bQuitFlag = true;
                }
            }
            SDL_Delay(10);
        }
    }

    // Restore
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
