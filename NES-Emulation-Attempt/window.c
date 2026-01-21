#include "window.h"
#include <Windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <stdint.h>
#include <stdio.h>
#include "nes.h"
#include "ram.h"
#include "6502.h"

#pragma comment(lib, "comctl32.lib")

HINSTANCE hInst = NULL;

/*###########global wnd handles###############*/

static HWND  g_hwndMain = NULL;
static HWND  g_btnRun = NULL, g_btnStep = NULL, g_btnStepN = NULL, g_btnRefresh = NULL;
static HWND  g_lvRegs = NULL, g_lvDisasm = NULL, g_lvMem = NULL;
static HWND  g_status = NULL;

static HFONT g_fontMono = NULL;
static HFONT g_fontUI = NULL;
/*############################################*/

/*###########IDS###################*/
#define ID_FILE_OPEN     40001
#define ID_FILE_EXIT     40002

#define ID_BTN_RUN       41001
#define ID_BTN_STEP      41002
#define ID_BTN_STEPN     41003
#define ID_BTN_REFRESH   41004
#define ID_BTN_BREAK     41005

#define ID_LV_REGS       42001
#define ID_LV_DISASM     42002
#define ID_LV_MEM        42003
#define ID_EDIT_LOG      42004
#define ID_STATUS        42005
/*################################*/

static HFONT create_mono_font(int pt)
{
    HDC hdc = GetDC(NULL);
    int logPixelsY = GetDeviceCaps(hdc, LOGPIXELSY);
    ReleaseDC(NULL, hdc);

    int height = -MulDiv(pt, logPixelsY, 72);

    HFONT f = CreateFontW(
        height, 0, 0, 0,
        FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY,
        FIXED_PITCH | FF_MODERN,
        L"Consolas"
    );
    if (!f) f = (HFONT)GetStockObject(OEM_FIXED_FONT);
    return f;
}

static HFONT create_ui_font(int pt)
{
    NONCLIENTMETRICSW ncm = { 0 };
    ncm.cbSize = sizeof(ncm);
    SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0);

    // scale size a bit; simplest: use system message font directly
    HFONT f = CreateFontIndirectW(&ncm.lfMessageFont);
    (void)pt;
    return f ? f : (HFONT)GetStockObject(DEFAULT_GUI_FONT);
}

static void set_font_recursive(HWND h, HFONT f)
{
    SendMessageW(h, WM_SETFONT, (WPARAM)f, TRUE);
}

static int clampi(int v, int lo, int hi) { return (v < lo) ? lo : (v > hi) ? hi : v; }

static void layout_controls(HWND hwnd)
{
    RECT rc;
    GetClientRect(hwnd, &rc);

    int W = rc.right - rc.left;
    int H = rc.bottom - rc.top;

    const int pad = 8;
    const int gap = 8;

    // Toolbar button row
    const int btnH = 28;
    const int btnW = 110;

    // Status bar height (we can query it)
    int statusH = 0;
    if (g_status) {
        RECT rs;
        GetWindowRect(g_status, &rs);
        statusH = rs.bottom - rs.top;
    }

    int x = pad;
    int y = pad;

    // Buttons
    if (g_btnRun)     MoveWindow(g_btnRun, x + (btnW + gap) * 0, y, btnW, btnH, TRUE);
    if (g_btnStep)    MoveWindow(g_btnStep, x + (btnW + gap) * 1, y, btnW, btnH, TRUE);
    if (g_btnStepN)   MoveWindow(g_btnStepN, x + (btnW + gap) * 2, y, btnW, btnH, TRUE);
    if (g_btnRefresh) MoveWindow(g_btnRefresh, x + (btnW + gap) * 3, y, btnW, btnH, TRUE);

    y += btnH + gap;

    // Available height for main panes + log + status
    int availH = H - y - statusH - pad - gap;
    if (availH < 100) availH = 100;

    int panesTop = y;
    int panesH = availH;

    // Three panes across: regs | disasm | memory
    int regsW = clampi(W / 5, 180, 320);
    int memW = clampi(W / 2, 360, 560);
    int disasmW = (W - pad * 2 - gap * 2) - regsW - memW;
    if (disasmW < 240) disasmW = 240;

    int regsX = pad;
    int disX = regsX + regsW + gap;
    int memX = disX + disasmW + gap;

    // Position list views
    if (g_lvRegs)   MoveWindow(g_lvRegs, regsX, panesTop, regsW, panesH, TRUE);
    if (g_lvDisasm) MoveWindow(g_lvDisasm, disX, panesTop, disasmW, panesH, TRUE);
    if (g_lvMem)    MoveWindow(g_lvMem, memX, panesTop, memW, panesH, TRUE);

    // Status at bottom
    if (g_status) SendMessageW(g_status, WM_SIZE, 0, 0);
}

static void lv_set_extended(HWND lv)
{
    ListView_SetExtendedListViewStyle(lv, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_DOUBLEBUFFER);
}

static void lv_add_col(HWND lv, int col, int width, const wchar_t* title)
{
    LVCOLUMNW c = { 0 };
    c.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
    c.pszText = (LPWSTR)title;
    c.cx = width;
    c.iSubItem = col;
    ListView_InsertColumn(lv, col, &c);
}

static void lv_clear(HWND lv)
{
    ListView_DeleteAllItems(lv);
}

static void lv_add_row(HWND lv, int row, const wchar_t* c0)
{
    LVITEMW it = { 0 };
    it.mask = LVIF_TEXT;
    it.iItem = row;
    it.iSubItem = 0;
    it.pszText = (LPWSTR)c0;
    ListView_InsertItem(lv, &it);
}

static void lv_set_cell(HWND lv, int row, int col, const wchar_t* txt)
{
    ListView_SetItemText(lv, row, col, (LPWSTR)txt);
}

static void lv_populate_ram() 
{
    uint8_t* ram = get_ram_buffer();

    for (int row = 0; row < 128; row++)
    {
        char ascii[17];
        memcpy(ascii, ram + row * 16, 16);
        ascii[16] = '\0';

        for (int i = 0; i < 16; i++) {
            if (ascii[i] < 32 || ascii[i] > 126)
                ascii[i] = '.';
        }

        //its annoying to have to convert to wide char but thats Microsoft for forcing wide char windows on me (ascii windows don't work anymore :( )
        wchar_t wascii[32];
        swprintf(wascii, sizeof(wascii)/2, L"%hs", ascii);
        lv_set_cell(g_lvMem, row, 17, wascii);

        for (int column = 1; column <= 16; column++)
        {
            wchar_t temp[8];
            swprintf(temp, sizeof(temp)/2, L"%02X", ram[(row*16)+(column-1)]);
            lv_set_cell(g_lvMem, row, column, temp);
        }
    }
}

static int      currentDisassembledIndex = 0;
static uint16_t last_pc = 0;
static uint16_t expected_next_pc = 0;
static int      have_expected = 0;

static void lv_populate_disassembly(void)
{
    Debug_instructions* di_list = NULL;

    Cpu6502_Regs r = cpu6502_get_regs();
    uint16_t pc = r.pc;

    // If we don't have a valid expected next pc yet, or pc isn't sequential -> full regen
    if (!have_expected || pc != expected_next_pc || currentDisassembledIndex >= 24)
    {
        currentDisassembledIndex = 0;
        di_list = reset_debug_instructions();

        // Set expectation based on line 0 (the instruction at current pc)
        last_pc = di_list[0].address;
        expected_next_pc = (uint16_t)(last_pc + di_list[0].numberOfBytes);
        have_expected = 1;
    }
    else
    {
        // Sequential step: just use existing list and advance highlight
        di_list = get_debug_instructions();
    }

    // Update UI (mnemonics + highlight)
    for (int i = 0; i < 24; i++)
    {
        wchar_t temp[160];
        if (i == currentDisassembledIndex)
            swprintf(temp, 160, L"> %s", di_list[i].mneumonics);
        else
            swprintf(temp, 160, L"  %s", di_list[i].mneumonics);

        lv_set_cell(g_lvDisasm, i, 2, temp);

        wchar_t temp1[32];
        switch (di_list[i].numberOfBytes)
        {
        case 1:
            swprintf(temp1, 32, L"%02X", di_list[i].bytes[0]);
            break;
        case 2:
            swprintf(temp1, 32, L"%02X %02X", di_list[i].bytes[0], di_list[i].bytes[1]);
            break;
        case 3:
            swprintf(temp1, 32, L"%02X %02X %02X", di_list[i].bytes[0], di_list[i].bytes[1], di_list[i].bytes[2]);
            break;
        default:
            swprintf(temp1, 32, L"%02X", di_list[i].bytes[0]);
            break;
        }
        lv_set_cell(g_lvDisasm, i, 1, temp1);
    }

    // Advance expected PC based on the *highlighted* instruction
    last_pc = di_list[currentDisassembledIndex].address;
    expected_next_pc = (uint16_t)(last_pc + di_list[currentDisassembledIndex].numberOfBytes);

    currentDisassembledIndex++;
}

#define REG_COUNT 6

static uint32_t prev_regs[REG_COUNT] = { 0 };
static BOOL     reg_changed[REG_COUNT] = { 0 };
static BOOL     regs_first = TRUE;

static void lv_populate_registers() 
{
    Cpu6502_Regs r = cpu6502_get_regs();
    
    uint32_t cur[REG_COUNT] = {
        r.pc,
        r.sp,
        r.a,
        r.x,
        r.y,
        r.status
    };

    for (int i = 0; i < REG_COUNT; i++)
    {
        if (regs_first)
            reg_changed[i] = FALSE;
        else
            reg_changed[i] = (cur[i] != prev_regs[i]);

        prev_regs[i] = cur[i];
    }
    regs_first = FALSE;

    for (int i = 0; i < 6; i++)
    {
        wchar_t register_hex_Value[8];
        wchar_t register_decimal_value[16];
        switch (i)
        {
        case 0:
            swprintf(register_hex_Value, 8, L"%04X", r.pc);
            swprintf(register_decimal_value, 16, L"%u", r.pc);
            break;
        case 1:
            swprintf(register_hex_Value, 8, L"%02X", r.sp);
            swprintf(register_decimal_value, 16, L"%u", r.sp);
            break;
        case 2:
            swprintf(register_hex_Value, 8, L"%02X", r.a);
            swprintf(register_decimal_value, 16, L"%u", r.a);
            break;
        case 3:
            swprintf(register_hex_Value, 8, L"%02X", r.x);
            swprintf(register_decimal_value, 16, L"%u", r.x);
            break;
        case 4:
            swprintf(register_hex_Value, 8, L"%02X", r.y);
            swprintf(register_decimal_value, 16, L"%u", r.y);
            break;
        case 5:
            swprintf(register_hex_Value, 8, L"%02X", r.status);
            swprintf(register_decimal_value, 8, L"%u", r.status);
            break;
        }

        lv_set_cell(g_lvRegs, i, 1, register_hex_Value);
        lv_set_cell(g_lvRegs, i, 2, register_decimal_value);
    }

    InvalidateRect(g_lvRegs, NULL, TRUE);
}

static void refresh_view()
{
    lv_populate_ram();
    lv_populate_disassembly();
    lv_populate_registers();
}

// -------------------- Creation --------------------
static void create_controls(HWND hwnd)
{
    // Buttons row
    g_btnRun = CreateWindowExW(0, L"BUTTON", L"Continue [C]",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        0, 0, 10, 10, hwnd, (HMENU)ID_BTN_RUN, NULL, NULL);

    g_btnStep = CreateWindowExW(0, L"BUTTON", L"Step [S]",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        0, 0, 10, 10, hwnd, (HMENU)ID_BTN_STEP, NULL, NULL);

    g_btnStepN = CreateWindowExW(0, L"BUTTON", L"Step N",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        0, 0, 10, 10, hwnd, (HMENU)ID_BTN_STEPN, NULL, NULL);

    g_btnRefresh = CreateWindowExW(0, L"BUTTON", L"Refresh",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        0, 0, 10, 10, hwnd, (HMENU)ID_BTN_REFRESH, NULL, NULL);

    // Registers ListView
    g_lvRegs = CreateWindowExW(
        WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"",
        WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS,
        0, 0, 10, 10, hwnd, (HMENU)ID_LV_REGS, NULL, NULL);

    // Disassembly ListView
    g_lvDisasm = CreateWindowExW(
        WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"",
        WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS,
        0, 0, 10, 10, hwnd, (HMENU)ID_LV_DISASM, NULL, NULL);

    // Memory dump ListView
    g_lvMem = CreateWindowExW(
        WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"",
        WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS,
        0, 0, 10, 10, hwnd, (HMENU)ID_LV_MEM, NULL, NULL);

    // Status bar
    g_status = CreateWindowExW(
        0, STATUSCLASSNAMEW, NULL,
        WS_CHILD | WS_VISIBLE,
        0, 0, 0, 0, hwnd, (HMENU)ID_STATUS, NULL, NULL);

    // Fonts
    g_fontMono = create_mono_font(14);
    g_fontUI = create_ui_font(9);

    // Apply fonts
    //Use mono for the three panes + log; UI font for buttons (or mono if you prefer)
    set_font_recursive(g_lvRegs, g_fontMono);
    set_font_recursive(g_lvDisasm, g_fontMono);
    set_font_recursive(g_lvMem, g_fontMono);

    set_font_recursive(g_btnRun, g_fontUI);
    set_font_recursive(g_btnStep, g_fontUI);
    set_font_recursive(g_btnStepN, g_fontUI);
    set_font_recursive(g_btnRefresh, g_fontUI);

    // Configure listviews
    lv_set_extended(g_lvRegs);
    lv_set_extended(g_lvDisasm);
    lv_set_extended(g_lvMem);

    // Columns: Registers
    lv_add_col(g_lvRegs, 0, 80, L"Reg");
    lv_add_col(g_lvRegs, 1, 80, L"Hex");
    lv_add_col(g_lvRegs, 2, 62, L"Dec");

    // Columns: Disasm
    lv_add_col(g_lvDisasm, 0, 90, L"Address");
    lv_add_col(g_lvDisasm, 1, 110, L"Bytes");
    lv_add_col(g_lvDisasm, 2, 150, L"Mnemonic");

    // Columns: Memory dump (Address, 16 bytes, ASCII)
    lv_add_col(g_lvMem, 0, 90, L"Address");
    for (int i = 0; i < 16; i++) {
        wchar_t t[8];
        swprintf(t, 8, L"%X", i);
        lv_add_col(g_lvMem, 1 + i, 34, t);
    }
    lv_add_col(g_lvMem, 17, 175, L"ASCII");

    //add rows
    lv_add_row(g_lvRegs, 0, L"PC");
    lv_add_row(g_lvRegs, 1, L"SP");
    lv_add_row(g_lvRegs, 2, L"A");
    lv_add_row(g_lvRegs, 3, L"X");
    lv_add_row(g_lvRegs, 4, L"Y");
    lv_add_row(g_lvRegs, 5, L"Status");

    //memory rows
    for (int i = 0; i < 128; i++) {
        wchar_t t[8];
        swprintf(t, 8, L"%04X", i*16);
        lv_add_row(g_lvMem, i, t);
    }

    //dissassembly rows
    for (int i = 0; i < 24; i++)
    {
        lv_add_row(g_lvDisasm, i, L"");
    }

    // Initial content
    refresh_view();

    layout_controls(hwnd);

    // Status parts
    int parts[3] = { 200, 520, -1 };
    SendMessageW(g_status, SB_SETPARTS, 3, (LPARAM)parts);
    SendMessageW(g_status, SB_SETTEXTW, 0, (LPARAM)L"Break");
    SendMessageW(g_status, SB_SETTEXTW, 1, (LPARAM)L"CPU: 6502");
    SendMessageW(g_status, SB_SETTEXTW, 2, (LPARAM)L"");
}

static LRESULT nes_dbg_proc(HANDLE hwnd,UINT msg,WPARAM wparam, LPARAM lparam)
{
    switch (msg)
    {
    case WM_CREATE:
        create_controls(hwnd);
        break;
    case WM_NOTIFY:
    {
        LPNMHDR hdr = (LPNMHDR)lparam;
        if (hdr->hwndFrom == g_lvRegs && hdr->code == NM_CUSTOMDRAW)
        {
            LPNMLVCUSTOMDRAW cd = (LPNMLVCUSTOMDRAW)lparam;

            switch (cd->nmcd.dwDrawStage)
            {
            case CDDS_PREPAINT:
                return CDRF_NOTIFYITEMDRAW;
            case CDDS_ITEMPREPAINT:
            {
                int row = (int)cd->nmcd.dwItemSpec;

                if (row >= 0 && row < REG_COUNT && reg_changed[row])
                {
                    cd->clrText = RGB(220, 40, 40); // red text
                }
                return CDRF_DODEFAULT;
            }
            }
        }

        break;
    }
    case WM_COMMAND:
    {
        switch (LOWORD(wparam))
        {
        case ID_FILE_OPEN: break;
        case ID_FILE_EXIT: DestroyWindow(hwnd); break;

        case ID_BTN_RUN:    break;
        case ID_BTN_STEP: 
        {
            nes_clock();
            while(get_cycles() != 0) nes_clock();
            refresh_view();
            break;
        }
        case ID_BTN_STEPN: break;
        case ID_BTN_REFRESH: refresh_view(); break;
        case ID_BTN_BREAK: break;
        }
        break;
    }
    case WM_CLOSE:
        PostQuitMessage(1);
        break;
    case WM_DESTROY:
        PostQuitMessage(1);
        if (g_fontMono) DeleteObject(g_fontMono);
        if (g_fontUI)   DeleteObject(g_fontUI);
        break;
    default:
        return DefWindowProcW(hwnd, msg, wparam, lparam);
    }
    return DefWindowProcW(hwnd, msg, wparam, lparam);
}

static HMENU create_menu_bar(void)
{
    HMENU hMenuBar = CreateMenu();
    HMENU hFile = CreatePopupMenu();

    AppendMenu(hFile, MF_STRING, ID_FILE_OPEN, L"&Open ROM...\tCtrl+O");
    AppendMenu(hFile, MF_SEPARATOR, 0, NULL);
    AppendMenu(hFile, MF_STRING, ID_FILE_EXIT, L"E&xit");

    AppendMenu(hMenuBar, MF_POPUP, (UINT_PTR)hFile, L"&File");
    return hMenuBar;
}

bool create_windows()
{
    //debug window
    INITCOMMONCONTROLSEX icc = { 0 };
    icc.dwSize = sizeof(icc);
    icc.dwICC = ICC_LISTVIEW_CLASSES | ICC_BAR_CLASSES;
    InitCommonControlsEx(&icc);

    hInst = GetModuleHandle(NULL);

    WNDCLASSA wc = { 0 };
    wc.lpfnWndProc = nes_dbg_proc;
    wc.hInstance = hInst;
    wc.lpszClassName = L"NesDbgWnd";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);

    RegisterClassW(&wc);

    HWND hwnd = CreateWindowEx(
        0, wc.lpszClassName, L"NES Debugger",
        WS_OVERLAPPEDWINDOW & ~WS_SIZEBOX & ~WS_MAXIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 1200, 720,
        NULL, NULL, hInst, NULL);

    g_hwndMain = hwnd;

    SetMenu(hwnd, create_menu_bar());

    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    //TO-DO directX window

	return true;
}

int updateWindows()
{
    MSG msg;
    while (PeekMessage(&msg, NULL, 0,0, PM_REMOVE))
    {
        if (msg.message == WM_QUIT)
        {
            return msg.wParam;
        }

        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}