#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include <windows.h>
#include <commctrl.h>
#include <shobjidl.h>
#include <string>
#include <thread>
#include <algorithm>
#include <filesystem>
#include "../core/config.h"
#include "../core/cursor.h"
#include "../core/scheduler.h"
#include "../core/utils.h"
#include "../core/autofill.h"

namespace fs = std::filesystem;

#define WM_USER_TASK_DONE  (WM_APP + 1)
#define WM_USER_PKG_RELOAD (WM_APP + 2)
#define IDI_ICON1 1

// ─────────────────────────────────────────────────────────────────
// Control IDs
// ─────────────────────────────────────────────────────────────────
#define IDC_TAB_MAIN         400

// --- Tab 0: 執行 ---
#define IDC_LABEL_PATH       101
#define IDC_EDIT_PATH        102
#define IDC_BTN_BROWSE       103

#define IDC_GROUP_LOOP       200
#define IDC_LABEL_MODE       201
#define IDC_RADIO_CYCLE      202
#define IDC_RADIO_RANDOM     203
#define IDC_LABEL_TIME       204
#define IDC_EDIT_TIME        205
#define IDC_LABEL_SEC        206
#define IDC_CHK_SHADOW       207
#define IDC_BTN_START        208
#define IDC_BTN_STOP         209
#define IDC_LABEL_STATUS     211

#define IDC_GROUP_SINGLE     300
#define IDC_RADIO_SINGLE_CYCLE 301
#define IDC_RADIO_SINGLE_PACK  302
#define IDC_LABEL_PACK       303
#define IDC_COMBO_PACK       304
#define IDC_BTN_EXECUTE_SINGLE 305
#define IDC_BTN_RESTORE_DEFAULT 306

#define IDC_GROUP_GEN        310
#define IDC_SLIDER_SIZE      311
#define IDC_LABEL_SIZE_TITLE 312
#define IDC_LABEL_SIZE_VAL   313

// --- Tab 1: 套裝設置 ---
#define IDC_LABEL_PKG_PATH   501
#define IDC_EDIT_PKG_PATH    502
#define IDC_BTN_PKG_BROWSE   503
#define IDC_LIST_PACKAGES    504
#define IDC_BTN_PKG_SAVE     507
#define IDC_GROUP_PKG_MAP    508
#define IDC_LIST_ROLES       509
#define IDC_LABEL_ROLE       510
#define IDC_EDIT_FILE        511
#define IDC_BTN_FILE_BROWSE  512
#define IDC_LABEL_PKG_HINT   514
#define IDC_COMBO_PKG_SELECT 516
#define IDC_BTN_AUTO_FILL    517
#define IDC_BTN_ADD_RULE     518

// ─────────────────────────────────────────────────────────────────
// Globals
// ─────────────────────────────────────────────────────────────────
HINSTANCE hInst;
HFONT hFont;
int g_dpi = 96;
Config g_config;
Autofill g_autofill;
std::wstring g_config_path = L"config.json";
std::wstring g_autofill_path = L"autofill.json";

// Tab 1 state
int g_sel_pkg  = -1; // currently selected package index
int g_sel_role = -1; // currently selected role index
bool g_is_refreshing_ui = false; // 防止無限遞迴崩潰

static const int kRoleCount = kCursorRoleCount;

#ifndef SPI_SETCURSORSIZE
#define SPI_SETCURSORSIZE 0x2029
#endif

LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

int Scale(int v) { return MulDiv(v, g_dpi, 96); }

void SetModernFont(HWND hwnd) {
    LOGFONTW lf = { 0 };
    SystemParametersInfoW(SPI_GETICONTITLELOGFONT, sizeof(lf), &lf, 0);
    lf.lfHeight = -MulDiv(10, g_dpi, 72);
    hFont = CreateFontIndirectW(&lf);
    EnumChildWindows(hwnd, [](HWND child, LPARAM lp) -> BOOL {
        SendMessageW(child, WM_SETFONT, (WPARAM)lp, TRUE);
        return TRUE;
    }, (LPARAM)hFont);
}

// ─────────────────────────────────────────────────────────────────
// 讀取 Tab 0 UI → Config
// ─────────────────────────────────────────────────────────────────
void UpdateConfigFromUI(HWND hwnd) {
    wchar_t buf[256];
    GetWindowTextW(GetDlgItem(hwnd, IDC_EDIT_TIME), buf, 256);
    g_config.interval_minutes = _wtoi(buf);
    if (g_config.interval_minutes <= 0) g_config.interval_minutes = 15;

    g_config.shadow = (SendMessageW(GetDlgItem(hwnd, IDC_CHK_SHADOW), BM_GETCHECK, 0, 0) == BST_CHECKED);

    bool isCycle = (SendMessageW(GetDlgItem(hwnd, IDC_RADIO_CYCLE), BM_GETCHECK, 0, 0) == BST_CHECKED);
    g_config.mode = isCycle ? L"round" : L"random";
}

// ─────────────────────────────────────────────────────────────────
// Tab 1 輔助
// ─────────────────────────────────────────────────────────────────

// 同步資料夾並重新填入套裝清單 (僅在初始化或顯式掃描時執行自動掃描)
void RefreshPackageList(HWND hwnd, bool scanDir = false) {
    if (scanDir) {
        std::error_code ec;
        for (auto &e : fs::directory_iterator(g_config.packages_path, ec)) {
            if (!e.is_directory()) continue;
            std::wstring name = e.path().filename().wstring();
            if (findPackageIndex(g_config, name) == -1) {
                CursorPackage np;
                np.name = name;
                np.mapping = defaultMapping();
                g_config.packages.push_back(std::move(np));
            }
        }
    }

    HWND hComboSelect = GetDlgItem(hwnd, IDC_COMBO_PKG_SELECT);
    HWND hComboTab0 = GetDlgItem(hwnd, IDC_COMBO_PACK);
    SendMessageW(hComboSelect, CB_RESETCONTENT, 0, 0);
    SendMessageW(hComboTab0, CB_RESETCONTENT, 0, 0);

    for (auto &pkg : g_config.packages) {
        SendMessageW(hComboSelect, CB_ADDSTRING, 0, (LPARAM)pkg.name.c_str());
        SendMessageW(hComboTab0, CB_ADDSTRING, 0, (LPARAM)pkg.name.c_str());
    }

    if (g_sel_pkg >= 0 && g_sel_pkg < (int)g_config.packages.size())
        SendMessageW(hComboSelect, CB_SETCURSEL, g_sel_pkg, 0);
    else if (!g_config.packages.empty()) {
        g_sel_pkg = 0;
        SendMessageW(hComboSelect, CB_SETCURSEL, 0, 0);
    }

    if (!g_config.packages.empty())
        SendMessageW(hComboTab0, CB_SETCURSEL, 0, 0);
}

// 重新填入角色清單 (使用 ListView)
void RefreshRoleList(HWND hwnd) {
    if (g_is_refreshing_ui) return;
    g_is_refreshing_ui = true;

    HWND hLV = GetDlgItem(hwnd, IDC_LIST_ROLES);
    SendMessageW(hLV, LVM_DELETEALLITEMS, 0, 0);

    if (g_sel_pkg < 0 || g_sel_pkg >= (int)g_config.packages.size()) {
        g_is_refreshing_ui = false;
        return;
    }

    const auto &pkg = g_config.packages[g_sel_pkg];
    for (int i = 0; i < kRoleCount; ++i) {
        std::wstring role = kCursorRoles[i];
        std::wstring friendly = kCursorFriendlyNames[i];

        auto it = pkg.mapping.find(role);
        std::wstring fileVal = (it != pkg.mapping.end()) ? it->second : L"";
        
        LVITEMW lvi = { 0 };
        lvi.mask = LVIF_TEXT;
        lvi.iItem = i;
        lvi.iSubItem = 0;
        lvi.pszText = (LPWSTR)friendly.c_str();
        SendMessageW(hLV, LVM_INSERTITEMW, 0, (LPARAM)&lvi);
        
        ListView_SetItemText(hLV, i, 1, (LPWSTR)role.c_str());
        ListView_SetItemText(hLV, i, 2, (LPWSTR)(fileVal.empty() ? L"(預設)" : fileVal.c_str()));
    }
    
    if (g_sel_role >= 0) {
        LVITEMW lvi = { 0 };
        lvi.state = LVIS_SELECTED | LVIS_FOCUSED;
        lvi.stateMask = LVIS_SELECTED | LVIS_FOCUSED;
        SendMessageW(hLV, LVM_SETITEMSTATE, g_sel_role, (LPARAM)&lvi);
    }
    
    g_is_refreshing_ui = false;
}

// 更新角色編輯欄位 (僅顯示名稱並啟用瀏覽按鈕)
void UpdateRoleEditor(HWND hwnd) {
    if (g_is_refreshing_ui) return;
    
    bool hasSel = (g_sel_pkg >= 0 && g_sel_role >= 0);
    if (!hasSel) {
        SetWindowTextW(GetDlgItem(hwnd, IDC_LABEL_ROLE), L"角色: 請選擇");
        SetWindowTextW(GetDlgItem(hwnd, IDC_EDIT_FILE), L"");
    } else {
        std::wstring role = kCursorRoles[g_sel_role];
        std::wstring friendly = kCursorFriendlyNames[g_sel_role];
        SetWindowTextW(GetDlgItem(hwnd, IDC_LABEL_ROLE), (L"選中: " + friendly).c_str());
        
        auto &pkg = g_config.packages[g_sel_pkg];
        SetWindowTextW(GetDlgItem(hwnd, IDC_EDIT_FILE), pkg.mapping[role].c_str());
    }
    EnableWindow(GetDlgItem(hwnd, IDC_EDIT_FILE), hasSel);
    EnableWindow(GetDlgItem(hwnd, IDC_BTN_FILE_BROWSE), hasSel);
}

// ─────────────────────────────────────────────────────────────────
// WinMain
// ─────────────────────────────────────────────────────────────────
int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR, int nCmdShow) {
    hInst = hInstance;

    // CLI 參數處理（--run-once 背景執行）
    int argc;
    LPWSTR *argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    
    // 確保 config_path 為絕對路徑，防止 CWD 改變導致儲存位置錯誤
    wchar_t fullPath[MAX_PATH];
    if (GetFullPathNameW(L"config.json", MAX_PATH, fullPath, NULL)) {
        g_config_path = fullPath;
    }

    if (argv && argc >= 2) {
        bool runOnce = false;
        std::wstring cfgPath = g_config_path;
        for (int i = 1; i < argc; ++i) {
            std::wstring arg = argv[i];
            if (arg == L"--run-once") runOnce = true;
            else if (arg == L"--config" && i + 1 < argc) {
                cfgPath = argv[++i];
                // 若用戶提供了自定義路徑，也轉為絕對路徑
                if (GetFullPathNameW(cfgPath.c_str(), MAX_PATH, fullPath, NULL)) {
                    cfgPath = fullPath;
                }
            }
        }
        LocalFree(argv);

        if (runOnce) {
            Config c = loadConfig(cfgPath);
            // 使用 packages_path 作為游標根資料夾
            std::wstring dir = c.packages_path;
            auto packs = listPacks(dir);
            if (!packs.empty()) {
                auto chosen = choosePack(packs, c);
                // 找出對應套裝定義
                std::wstring packName = chosen.filename().wstring();
                int idx = findPackageIndex(c, packName);
                if (idx >= 0) {
                    applyPackage(c, c.packages[idx]);
                } else {
                    applyPack(chosen);
                }
                setCursorShadow(c.shadow);
            }
            return 0;
        }
        g_config_path = cfgPath; // 同步全域配置路徑，確保儲存到正確位置
    }

    g_config = loadConfig(g_config_path);
    g_autofill = loadAutofill(g_autofill_path);

    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

    // DPI Awareness
    HMODULE hUser32 = GetModuleHandleW(L"user32.dll");
    if (hUser32) {
        typedef BOOL(WINAPI* FnSetDpiCtx)(DPI_AWARENESS_CONTEXT);
        auto fn = (FnSetDpiCtx)GetProcAddress(hUser32, "SetProcessDpiAwarenessContext");
        if (fn) fn(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
        typedef UINT(WINAPI* FnGetDpi)();
        auto fn2 = (FnGetDpi)GetProcAddress(hUser32, "GetDpiForSystem");
        if (fn2) g_dpi = fn2();
    }

    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(icex);
    icex.dwICC  = ICC_WIN95_CLASSES | ICC_STANDARD_CLASSES | ICC_TAB_CLASSES;
    InitCommonControlsEx(&icex);

    const wchar_t CLASS_NAME[] = L"CuratorGUIClass";
    WNDCLASSEXW wc = { 0 };
    wc.cbSize        = sizeof(WNDCLASSEXW);
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW);
    wc.hIcon         = LoadIconW(hInstance, MAKEINTRESOURCEW(IDI_ICON1));
    wc.hIconSm       = (HICON)LoadImageW(hInstance, MAKEINTRESOURCEW(IDI_ICON1),
                         IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR);
    if (!RegisterClassExW(&wc)) return 0;

    RECT rc = { 0, 0, Scale(460), Scale(560) };
    AdjustWindowRectEx(&rc, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, FALSE, 0);

    HWND hwnd = CreateWindowExW(
        0, CLASS_NAME, L"Curator (Cursor Tool)",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, rc.right - rc.left, rc.bottom - rc.top,
        NULL, NULL, hInstance, NULL
    );
    if (!hwnd) return 0;

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    MSG msg = { 0 };
    while (GetMessage(&msg, NULL, 0, 0)) {
        if (!IsDialogMessageW(hwnd, &msg)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    if (hFont) DeleteObject(hFont);
    return static_cast<int>(msg.wParam);
}

// ─────────────────────────────────────────────────────────────────
// 建立 Tab 0 控制項 (執行)
// ─────────────────────────────────────────────────────────────────
void CreateTab0Controls(HWND hwnd) {
    int y = 50;

    // 套裝根目錄
    CreateWindowExW(0, L"STATIC", L"套裝根目錄:", WS_CHILD | WS_VISIBLE | SS_LEFT,
        Scale(25), Scale(y), Scale(150), Scale(20), hwnd, (HMENU)IDC_LABEL_PATH, hInst, NULL);
    y += 25;
    CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", g_config.packages_path.c_str(),
        WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL | ES_READONLY,
        Scale(25), Scale(y), Scale(320), Scale(25), hwnd, (HMENU)IDC_EDIT_PATH, hInst, NULL);
    CreateWindowExW(0, L"BUTTON", L"選擇", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        Scale(355), Scale(y), Scale(75), Scale(25), hwnd, (HMENU)IDC_BTN_BROWSE, hInst, NULL);
    y += 40;

    // 輪循執行 GroupBox
    CreateWindowExW(0, L"BUTTON", L"輪循執行", WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
        Scale(25), Scale(y), Scale(410), Scale(160), hwnd, (HMENU)IDC_GROUP_LOOP, hInst, NULL);

    int gy = y + 27;
    CreateWindowExW(0, L"STATIC", L"輪循模式:", WS_CHILD | WS_VISIBLE | SS_LEFT,
        Scale(45), Scale(gy+2), Scale(70), Scale(20), hwnd, (HMENU)IDC_LABEL_MODE, hInst, NULL);
    HWND hRadioCycle = CreateWindowExW(0, L"BUTTON", L"循環", WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON | WS_GROUP,
        Scale(125), Scale(gy), Scale(60), Scale(20), hwnd, (HMENU)IDC_RADIO_CYCLE, hInst, NULL);
    HWND hRadioRandom = CreateWindowExW(0, L"BUTTON", L"隨機", WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON,
        Scale(195), Scale(gy), Scale(60), Scale(20), hwnd, (HMENU)IDC_RADIO_RANDOM, hInst, NULL);
    if (g_config.mode == L"round") SendMessageW(hRadioCycle, BM_SETCHECK, BST_CHECKED, 0);
    else SendMessageW(hRadioRandom, BM_SETCHECK, BST_CHECKED, 0);

    gy += 32;
    CreateWindowExW(0, L"STATIC", L"輪循時間:", WS_CHILD | WS_VISIBLE | SS_LEFT,
        Scale(45), Scale(gy+2), Scale(70), Scale(20), hwnd, (HMENU)IDC_LABEL_TIME, hInst, NULL);
    CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", std::to_wstring(g_config.interval_minutes).c_str(),
        WS_CHILD | WS_VISIBLE | WS_BORDER | ES_NUMBER | ES_AUTOHSCROLL | ES_RIGHT,
        Scale(125), Scale(gy), Scale(55), Scale(22), hwnd, (HMENU)IDC_EDIT_TIME, hInst, NULL);
    CreateWindowExW(0, L"STATIC", L"分鐘", WS_CHILD | WS_VISIBLE | SS_LEFT,
        Scale(185), Scale(gy+2), Scale(40), Scale(20), hwnd, (HMENU)IDC_LABEL_SEC, hInst, NULL);

    gy += 32;
    CreateWindowExW(0, L"STATIC", L"輪循狀態: 偵測中...", WS_CHILD | WS_VISIBLE | SS_LEFT,
        Scale(47), Scale(gy+2), Scale(150), Scale(20), hwnd, (HMENU)IDC_LABEL_STATUS, hInst, NULL);

    gy += 30;
    CreateWindowExW(0, L"BUTTON", L"啟動", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        Scale(110), Scale(gy), Scale(95), Scale(28), hwnd, (HMENU)IDC_BTN_START, hInst, NULL);
    CreateWindowExW(0, L"BUTTON", L"終止", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        Scale(225), Scale(gy), Scale(95), Scale(28), hwnd, (HMENU)IDC_BTN_STOP, hInst, NULL);

    y += 170;

    // 單次執行 GroupBox
    CreateWindowExW(0, L"BUTTON", L"單次執行", WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
        Scale(25), Scale(y), Scale(410), Scale(140), hwnd, (HMENU)IDC_GROUP_SINGLE, hInst, NULL);

    gy = y + 28;
    HWND hRadioSingleCycle = CreateWindowExW(0, L"BUTTON", L"依輪循設置切換一次", WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON | WS_GROUP,
        Scale(45), Scale(gy), Scale(200), Scale(22), hwnd, (HMENU)IDC_RADIO_SINGLE_CYCLE, hInst, NULL);
    SendMessageW(hRadioSingleCycle, BM_SETCHECK, BST_CHECKED, 0);

    gy += 28;
    CreateWindowExW(0, L"BUTTON", L"套用指定套裝", WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON,
        Scale(45), Scale(gy), Scale(120), Scale(22), hwnd, (HMENU)IDC_RADIO_SINGLE_PACK, hInst, NULL);
    CreateWindowExW(0, L"STATIC", L"套裝:", WS_CHILD | WS_VISIBLE | SS_LEFT,
        Scale(170), Scale(gy+2), Scale(45), Scale(20), hwnd, (HMENU)IDC_LABEL_PACK, hInst, NULL);

    HWND hComboPack = CreateWindowExW(0, L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST,
        Scale(218), Scale(gy), Scale(177), Scale(160), hwnd, (HMENU)IDC_COMBO_PACK, hInst, NULL);
    // 填入套裝
    for (auto &pkg : g_config.packages)
        SendMessageW(hComboPack, CB_ADDSTRING, 0, (LPARAM)pkg.name.c_str());
    if (!g_config.packages.empty()) SendMessageW(hComboPack, CB_SETCURSEL, 0, 0);
    EnableWindow(hComboPack, FALSE);

    gy += 42;
    CreateWindowExW(0, L"BUTTON", L"執行", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        Scale(110), Scale(gy), Scale(95), Scale(28), hwnd, (HMENU)IDC_BTN_EXECUTE_SINGLE, hInst, NULL);
    CreateWindowExW(0, L"BUTTON", L"恢復預設", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        Scale(225), Scale(gy), Scale(95), Scale(28), hwnd, (HMENU)IDC_BTN_RESTORE_DEFAULT, hInst, NULL);

    y += 150;

    // 通用設定 GroupBox
    CreateWindowExW(0, L"BUTTON", L"通用顯示設定", WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
        Scale(25), Scale(y), Scale(410), Scale(100), hwnd, (HMENU)IDC_GROUP_GEN, hInst, NULL);
    
    gy = y + 25;
    HWND hChkShadow = CreateWindowExW(0, L"BUTTON", L"啟用游標陰影", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
        Scale(45), Scale(gy), Scale(130), Scale(22), hwnd, (HMENU)IDC_CHK_SHADOW, hInst, NULL);
    SendMessageW(hChkShadow, BM_SETCHECK, g_config.shadow ? BST_CHECKED : BST_UNCHECKED, 0);

    gy += 35;
    CreateWindowExW(0, L"STATIC", L"游標大小: (未實現)", WS_CHILD | WS_VISIBLE | SS_LEFT,
        Scale(45), Scale(gy+2), Scale(120), Scale(20), hwnd, (HMENU)IDC_LABEL_SIZE_TITLE, hInst, NULL);
    
    HWND hSlider = CreateWindowExW(0, TRACKBAR_CLASSW, L"", WS_CHILD | WS_VISIBLE | TBS_AUTOTICKS | TBS_HORZ,
        Scale(165), Scale(gy), Scale(210), Scale(30), hwnd, (HMENU)IDC_SLIDER_SIZE, hInst, NULL);
    SendMessageW(hSlider, TBM_SETRANGE, TRUE, MAKELONG(1, 16));
    SendMessageW(hSlider, TBM_SETPOS, TRUE, g_config.cursor_size);
    EnableWindow(hSlider, FALSE); // 暫未支援

    CreateWindowExW(0, L"STATIC", std::to_wstring(g_config.cursor_size).c_str(), WS_CHILD | WS_VISIBLE | SS_LEFT,
        Scale(380), Scale(gy+4), Scale(30), Scale(20), hwnd, (HMENU)IDC_LABEL_SIZE_VAL, hInst, NULL);
}

// ─────────────────────────────────────────────────────────────────
// 建立 Tab 1 控制項 (套裝設置)
// ─────────────────────────────────────────────────────────────────
void CreateTab1Controls(HWND hwnd) {
    int y = 50;

    // 套裝根目錄 (統一排版與名稱)
    CreateWindowExW(0, L"STATIC", L"套裝根目錄:", WS_CHILD | SS_LEFT,
        Scale(25), Scale(y), Scale(150), Scale(20), hwnd, (HMENU)IDC_LABEL_PKG_PATH, hInst, NULL);
    y += 25;
    CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", g_config.packages_path.c_str(),
        WS_CHILD | WS_BORDER | ES_AUTOHSCROLL | ES_READONLY,
        Scale(25), Scale(y), Scale(320), Scale(25), hwnd, (HMENU)IDC_EDIT_PKG_PATH, hInst, NULL);
    CreateWindowExW(0, L"BUTTON", L"選擇", WS_CHILD | BS_PUSHBUTTON,
        Scale(355), Scale(y), Scale(75), Scale(25), hwnd, (HMENU)IDC_BTN_PKG_BROWSE, hInst, NULL);
    y += 40;

    // 選擇區域
    CreateWindowExW(0, L"STATIC", L"選擇編輯套裝:", WS_CHILD | SS_LEFT,
        Scale(25), Scale(y+2), Scale(100), Scale(20), hwnd, (HMENU)IDC_LABEL_PKG_HINT, hInst, NULL);
    CreateWindowExW(0, L"COMBOBOX", L"", WS_CHILD | CBS_DROPDOWNLIST,
        Scale(125), Scale(y), Scale(205), Scale(200), hwnd, (HMENU)IDC_COMBO_PKG_SELECT, hInst, NULL);
    y += 35;

    // 角色映射區塊
    CreateWindowExW(0, L"BUTTON", L"角色映射設置 (選中後可變更檔案)", WS_CHILD | BS_GROUPBOX,
        Scale(20), Scale(y), Scale(420), Scale(335), hwnd, (HMENU)IDC_GROUP_PKG_MAP, hInst, NULL);

    int gy = y + 27;
    // 角色清單 (ListView)
    HWND hLV = CreateWindowExW(WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"",
        WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS,
        Scale(35), Scale(gy), Scale(390), Scale(245), hwnd, (HMENU)IDC_LIST_ROLES, hInst, NULL);
    
    // 設定 ListView 欄位 (Reg名稱 與 檔名)
    LVCOLUMNW lvc = { 0 };
    lvc.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
    lvc.pszText = (LPWSTR)L"常用名稱";
    lvc.cx = Scale(115);
    SendMessageW(hLV, LVM_INSERTCOLUMNW, 0, (LPARAM)&lvc);
    
    lvc.pszText = (LPWSTR)L"系統名稱";
    lvc.cx = Scale(85);
    SendMessageW(hLV, LVM_INSERTCOLUMNW, 1, (LPARAM)&lvc);

    lvc.pszText = (LPWSTR)L"套用檔案";
    lvc.cx = Scale(175);
    SendMessageW(hLV, LVM_INSERTCOLUMNW, 2, (LPARAM)&lvc);
    
    // 開啟 Full Row Select 整列選取藍底
    SendMessageW(hLV, LVM_SETEXTENDEDLISTVIEWSTYLE, LVS_EX_FULLROWSELECT, LVS_EX_FULLROWSELECT);

    gy += 255;
    // 快速編輯/瀏覽區
    CreateWindowExW(0, L"STATIC", L"選中:", WS_CHILD | SS_LEFT,
        Scale(35), Scale(gy+4), Scale(130), Scale(20), hwnd, (HMENU)IDC_LABEL_ROLE, hInst, NULL);
    
    CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
        WS_CHILD | WS_BORDER | ES_AUTOHSCROLL,
        Scale(165), Scale(gy), Scale(215), Scale(25), hwnd, (HMENU)IDC_EDIT_FILE, hInst, NULL);

    CreateWindowExW(0, L"BUTTON", L"...", WS_CHILD | BS_PUSHBUTTON,
        Scale(390), Scale(gy), Scale(35), Scale(25), hwnd, (HMENU)IDC_BTN_FILE_BROWSE, hInst, NULL);
    
    // 底部操作按鈕 (位於 GroupBox 外)
    int by = 500;
    CreateWindowExW(0, L"BUTTON", L"自動匹配規則", WS_CHILD | BS_PUSHBUTTON,
        Scale(55), Scale(by), Scale(110), Scale(28), hwnd, (HMENU)IDC_BTN_AUTO_FILL, hInst, NULL);

    CreateWindowExW(0, L"BUTTON", L"註冊當前規則", WS_CHILD | BS_PUSHBUTTON,
        Scale(185), Scale(by), Scale(110), Scale(28), hwnd, (HMENU)IDC_BTN_ADD_RULE, hInst, NULL);

    CreateWindowExW(0, L"BUTTON", L"儲存所有設置", WS_CHILD | BS_PUSHBUTTON,
        Scale(315), Scale(by), Scale(110), Scale(28), hwnd, (HMENU)IDC_BTN_PKG_SAVE, hInst, NULL);
    
    // 初始禁用編輯區
    EnableWindow(GetDlgItem(hwnd, IDC_EDIT_FILE),       FALSE);
    EnableWindow(GetDlgItem(hwnd, IDC_BTN_FILE_BROWSE), FALSE);
}

// ─────────────────────────────────────────────────────────────────
// Tab 切換時顯示/隱藏控制項
// ─────────────────────────────────────────────────────────────────
static const int kTab0IDs[] = {
  IDC_LABEL_PATH, IDC_EDIT_PATH, IDC_BTN_BROWSE,
  IDC_GROUP_LOOP, IDC_LABEL_MODE, IDC_RADIO_CYCLE, IDC_RADIO_RANDOM,
  IDC_LABEL_TIME, IDC_EDIT_TIME, IDC_LABEL_SEC, IDC_CHK_SHADOW,
  IDC_BTN_START, IDC_BTN_STOP, IDC_LABEL_STATUS,
  IDC_GROUP_SINGLE, IDC_RADIO_SINGLE_CYCLE, IDC_RADIO_SINGLE_PACK,
  IDC_LABEL_PACK, IDC_COMBO_PACK, IDC_BTN_EXECUTE_SINGLE, IDC_BTN_RESTORE_DEFAULT,
  IDC_GROUP_GEN, IDC_SLIDER_SIZE, IDC_LABEL_SIZE_TITLE, IDC_LABEL_SIZE_VAL
};
static const int kTab1IDs[] = {
  IDC_LABEL_PKG_PATH, IDC_EDIT_PKG_PATH, IDC_BTN_PKG_BROWSE,
  IDC_LABEL_PKG_HINT, IDC_COMBO_PKG_SELECT,
  IDC_BTN_AUTO_FILL, IDC_BTN_PKG_SAVE,
  IDC_GROUP_PKG_MAP, IDC_LIST_ROLES, IDC_LABEL_ROLE,
  IDC_EDIT_FILE, IDC_BTN_ADD_RULE, IDC_BTN_FILE_BROWSE
};

void ShowTabControls(HWND hwnd, int tab) {
    // Tab 0 controls
    for (int id : kTab0IDs)
        ShowWindow(GetDlgItem(hwnd, id), (tab == 0) ? SW_SHOW : SW_HIDE);
    // Tab 1 controls
    for (int id : kTab1IDs)
        ShowWindow(GetDlgItem(hwnd, id), (tab == 1) ? SW_SHOW : SW_HIDE);
}

// ─────────────────────────────────────────────────────────────────
// WndProc
// ─────────────────────────────────────────────────────────────────
void ShowTabControls(HWND hwnd, int tab);

LRESULT CALLBACK WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {

    case WM_HSCROLL: {
        if ((HWND)lParam == GetDlgItem(hwnd, IDC_SLIDER_SIZE)) {
            int pos = (int)SendMessageW((HWND)lParam, TBM_GETPOS, 0, 0);
            g_config.cursor_size = pos;
            SetWindowTextW(GetDlgItem(hwnd, IDC_LABEL_SIZE_VAL), std::to_wstring(pos).c_str());
            
            clearCustomCursors();
            setCursorSize(pos);
            
            saveConfig(g_config, g_config_path);
        }
        return 0;
    }

    case WM_CREATE: {
        SendMessageW(hwnd, WM_SETICON, ICON_BIG,
            (LPARAM)LoadIconW(hInst, MAKEINTRESOURCEW(IDI_ICON1)));
        SendMessageW(hwnd, WM_SETICON, ICON_SMALL,
            (LPARAM)LoadImageW(hInst, MAKEINTRESOURCEW(IDI_ICON1),
                IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR));

        // Tab Control
        HWND hTab = CreateWindowExW(0, WC_TABCONTROLW, L"",
            WS_CHILD | WS_CLIPSIBLINGS | WS_VISIBLE,
            Scale(10), Scale(10), Scale(440), Scale(540),
            hwnd, (HMENU)IDC_TAB_MAIN, hInst, NULL);
        TCITEMW tie = { 0 };
        tie.mask = TCIF_TEXT;
        tie.pszText = (LPWSTR)L"執行";
        SendMessageW(hTab, TCM_INSERTITEMW, 0, (LPARAM)&tie);
        tie.pszText = (LPWSTR)L"套裝設置";
        SendMessageW(hTab, TCM_INSERTITEMW, 1, (LPARAM)&tie);

        CreateTab0Controls(hwnd);
        CreateTab1Controls(hwnd);

        SetModernFont(hwnd);
        SetWindowPos(hTab, HWND_BOTTOM, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);

        // 默認顯示 Tab0
        ShowTabControls(hwnd, 0);
        RefreshPackageList(hwnd, true); // 啟動時掃描一次

        // 初始化 Tab1 套裝清單
        RefreshPackageList(hwnd);
        RefreshRoleList(hwnd);

        // 初始化輪循狀態顯示
        if (checkTaskExists(g_config))
            SetWindowTextW(GetDlgItem(hwnd, IDC_LABEL_STATUS), L"輪循狀態: 運行中");
        else
            SetWindowTextW(GetDlgItem(hwnd, IDC_LABEL_STATUS), L"輪循狀態: 關閉");
        return 0;
    }

    case WM_NOTIFY: {
        NMHDR *nm = (NMHDR *)lParam;
        if (nm->idFrom == IDC_TAB_MAIN && nm->code == TCN_SELCHANGE) {
            int tab = (int)SendMessageW(nm->hwndFrom, TCM_GETCURSEL, 0, 0);
            ShowTabControls(hwnd, tab);
            if (tab == 1) {
                // 切換分頁時僅刷新 UI 顯示，不重新掃描資料夾（避免覆蓋尚未儲存的變更）
                RefreshPackageList(hwnd, false);
                RefreshRoleList(hwnd);
            }
        }
        else if (nm->idFrom == IDC_LIST_ROLES && (nm->code == LVN_ITEMCHANGED)) {
            LPNMLISTVIEW pnmv = (LPNMLISTVIEW)lParam;
            if (pnmv->uNewState & LVIS_SELECTED) {
                g_sel_role = pnmv->iItem;
                UpdateRoleEditor(hwnd);
            }
        }
        return 0;
    }

    case WM_COMMAND: {
        int wmId    = LOWORD(wParam);
        int wmEvent = HIWORD(wParam);

        switch (wmId) {

        // ── Tab 0 ──────────────────────────────────────────
        case IDC_BTN_BROWSE: {
            IFileOpenDialog *pDlg;
            HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_ALL,
                IID_IFileOpenDialog, reinterpret_cast<void**>(&pDlg));
            if (SUCCEEDED(hr)) {
                pDlg->SetOptions(FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM);
                hr = pDlg->Show(hwnd);
                if (SUCCEEDED(hr)) {
                    IShellItem *pItem;
                    hr = pDlg->GetResult(&pItem);
                    if (SUCCEEDED(hr)) {
                        PWSTR pszPath;
                        pItem->GetDisplayName(SIGDN_FILESYSPATH, &pszPath);
                        g_config.packages_path = pszPath;
                        SetWindowTextW(GetDlgItem(hwnd, IDC_EDIT_PATH), pszPath);
                        SetWindowTextW(GetDlgItem(hwnd, IDC_EDIT_PKG_PATH), pszPath);
                        CoTaskMemFree(pszPath);
                        pItem->Release();
                        // 重新掃描新資料夾
                        RefreshPackageList(hwnd, true);
                    }
                }
                pDlg->Release();
            }
            break;
        }

        case IDC_BTN_START: {
            EnableWindow(hwnd, FALSE);
            UpdateConfigFromUI(hwnd);
            saveConfig(g_config, g_config_path);
            std::thread([hwnd]() {
                deleteTask(g_config);
                createTask(g_config, g_config_path);
                auto packs = listPacks(g_config.packages_path);
                if (!packs.empty()) {
                    auto chosen = choosePack(packs, g_config);
                    std::wstring packName = chosen.filename().wstring();
                    int idx = findPackageIndex(g_config, packName);
                    if (idx >= 0) applyPackage(g_config, g_config.packages[idx]);
                    else applyPack(chosen);
                    setCursorShadow(g_config.shadow);
                }
                PostMessageW(hwnd, WM_USER_TASK_DONE, 1, 0);
            }).detach();
            break;
        }

        case IDC_BTN_STOP: {
            EnableWindow(hwnd, FALSE);
            std::thread([hwnd]() {
                deleteTask(g_config);
                PostMessageW(hwnd, WM_USER_TASK_DONE, 2, 0);
            }).detach();
            break;
        }

        case IDC_BTN_EXECUTE_SINGLE: {
            EnableWindow(hwnd, FALSE);
            UpdateConfigFromUI(hwnd);
            saveConfig(g_config, g_config_path);
            bool isSpecificPack = (SendMessageW(GetDlgItem(hwnd, IDC_RADIO_SINGLE_PACK), BM_GETCHECK, 0, 0) == BST_CHECKED);
            int packSel = (int)SendMessageW(GetDlgItem(hwnd, IDC_COMBO_PACK), CB_GETCURSEL, 0, 0);
            std::thread([hwnd, isSpecificPack, packSel]() {
                if (isSpecificPack) {
                    if (packSel >= 0 && packSel < (int)g_config.packages.size()) {
                        applyPackage(g_config, g_config.packages[packSel]);
                        setCursorShadow(g_config.shadow);
                        PostMessageW(hwnd, WM_USER_TASK_DONE, 33, 0);
                    } else {
                        PostMessageW(hwnd, WM_USER_TASK_DONE, 31, 0);
                    }
                } else {
                    auto packs = listPacks(g_config.packages_path);
                    if (packs.empty()) { PostMessageW(hwnd, WM_USER_TASK_DONE, 32, 0); return; }
                    auto chosen = choosePack(packs, g_config);
                    std::wstring packName = chosen.filename().wstring();
                    int idx = findPackageIndex(g_config, packName);
                    if (idx >= 0) applyPackage(g_config, g_config.packages[idx]);
                    else applyPack(chosen);
                    setCursorShadow(g_config.shadow);
                    PostMessageW(hwnd, WM_USER_TASK_DONE, 33, 0);
                }
            }).detach();
            break;
        }

        case IDC_BTN_RESTORE_DEFAULT: {
            EnableWindow(hwnd, FALSE);
            std::thread([hwnd]() {
                restoreDefault();
                PostMessageW(hwnd, WM_USER_TASK_DONE, 4, 0);
            }).detach();
            break;
        }

        case IDC_RADIO_SINGLE_CYCLE:
            if (wmEvent == BN_CLICKED)
                EnableWindow(GetDlgItem(hwnd, IDC_COMBO_PACK), FALSE);
            break;
        case IDC_RADIO_SINGLE_PACK:
            if (wmEvent == BN_CLICKED)
                EnableWindow(GetDlgItem(hwnd, IDC_COMBO_PACK), TRUE);
            break;

        case IDC_CHK_SHADOW:
            if (wmEvent == BN_CLICKED) {
                g_config.shadow = (SendMessageW(GetDlgItem(hwnd, IDC_CHK_SHADOW), BM_GETCHECK, 0, 0) == BST_CHECKED);
                setCursorShadow(g_config.shadow);
                saveConfig(g_config, g_config_path);
            }
            break;

        // ── Tab 1 ──────────────────────────────────────────
        case IDC_BTN_PKG_BROWSE: {
            IFileOpenDialog *pDlg;
            HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_ALL,
                IID_IFileOpenDialog, reinterpret_cast<void**>(&pDlg));
            if (SUCCEEDED(hr)) {
                pDlg->SetOptions(FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM);
                hr = pDlg->Show(hwnd);
                if (SUCCEEDED(hr)) {
                    IShellItem *pItem;
                    hr = pDlg->GetResult(&pItem);
                    if (SUCCEEDED(hr)) {
                        PWSTR pszPath;
                        pItem->GetDisplayName(SIGDN_FILESYSPATH, &pszPath);
                        g_config.packages_path = pszPath;
                        SetWindowTextW(GetDlgItem(hwnd, IDC_EDIT_PKG_PATH), pszPath);
                        SetWindowTextW(GetDlgItem(hwnd, IDC_EDIT_PATH), pszPath);
                        CoTaskMemFree(pszPath);
                        pItem->Release();
                        // 修改資料夾後重新掃描
                        RefreshPackageList(hwnd, true);
                    }
                }
                pDlg->Release();
            }
            break;
        }

        case IDC_COMBO_PKG_SELECT:
            if (wmEvent == CBN_SELCHANGE) {
                g_sel_pkg = (int)SendMessageW(GetDlgItem(hwnd, IDC_COMBO_PKG_SELECT), CB_GETCURSEL, 0, 0);
                g_sel_role = -1;
                RefreshRoleList(hwnd);
                UpdateRoleEditor(hwnd);
            }
            break;

        case IDC_LIST_ROLES:
            if (wmEvent == LBN_SELCHANGE) {
                g_sel_role = (int)SendMessageW(GetDlgItem(hwnd, IDC_LIST_ROLES), LB_GETCURSEL, 0, 0);
                UpdateRoleEditor(hwnd);
            }
            break;

        case IDC_BTN_PKG_SAVE: {
            saveConfig(g_config, g_config_path);
            MessageBoxW(hwnd, L"所有配置與套裝映射設定已儲存。", L"儲存成功", MB_OK | MB_ICONINFORMATION);
            break;
        }

        case IDC_BTN_AUTO_FILL: {
            if (g_sel_pkg < 0) {
                MessageBoxW(hwnd, L"請先選擇一個套裝", L"提示", MB_OK | MB_ICONWARNING);
                break;
            }
            auto &pkg = g_config.packages[g_sel_pkg];
            std::wstring pkgDir = (fs::path(g_config.packages_path) / pkg.name).wstring();
            if (g_autofill.applyTo(pkg, pkgDir)) {
                RefreshRoleList(hwnd);
                UpdateRoleEditor(hwnd);
                MessageBoxW(hwnd, L"已自動匹配已知檔名規則。\n(僅填入尚未設定之項目)", L"自動匹配完成", MB_OK | MB_ICONINFORMATION);
            } else {
                MessageBoxW(hwnd, L"資料夾內未發現符合規則的新檔案。", L"提示", MB_OK | MB_ICONINFORMATION);
            }
            break;
        }

        case IDC_BTN_ADD_RULE: {
            if (g_sel_pkg < 0) {
                MessageBoxW(hwnd, L"請先選擇一個套裝", L"提示", MB_OK | MB_ICONWARNING);
                break;
            }
            int count = 0;
            auto &pkg = g_config.packages[g_sel_pkg];
            for (auto const& [role, file] : pkg.mapping) {
                if (!file.empty()) {
                    g_autofill.addRule(role, file);
                    count++;
                }
            }
            g_autofill.save(g_autofill_path);
            std::wstring msg = L"已從目前套裝 「" + pkg.name + L"」 學習並註冊了 " + std::to_wstring(count) + L" 條匹配規則。";
            MessageBoxW(hwnd, msg.c_str(), L"註冊成功", MB_OK | MB_ICONINFORMATION);
            break;
        }

        case IDC_EDIT_FILE:
            if (wmEvent == EN_CHANGE && !g_is_refreshing_ui) {
                if (g_sel_pkg >= 0 && g_sel_role >= 0) {
                    wchar_t buf[256];
                    GetWindowTextW(GetDlgItem(hwnd, IDC_EDIT_FILE), buf, 256);
                    g_config.packages[g_sel_pkg].mapping[kCursorRoles[g_sel_role]] = buf;
                    
                    // 僅更新 ListView 的子項目字串
                    HWND hLV = GetDlgItem(hwnd, IDC_LIST_ROLES);
                    ListView_SetItemText(hLV, g_sel_role, 2, (LPWSTR)(wcslen(buf) > 0 ? buf : L"(預設)"));
                }
            }
            break;

        case IDC_BTN_FILE_BROWSE: {
            if (g_sel_pkg < 0 || g_sel_role < 0) break;
            wchar_t filePath[MAX_PATH] = L"";
            OPENFILENAMEW ofn = { 0 };
            ofn.lStructSize  = sizeof(ofn);
            ofn.hwndOwner    = hwnd;
            ofn.lpstrFilter  = L"游標檔案 (*.ani;*.cur)\0*.ani;*.cur\0所有檔案 (*.*)\0*.*\0";
            ofn.lpstrFile    = filePath;
            ofn.nMaxFile     = MAX_PATH;
            ofn.Flags        = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
            
            // 嘗試從該套裝目錄開啟
            std::wstring initialDir = (std::filesystem::path(g_config.packages_path) / g_config.packages[g_sel_pkg].name).wstring();
            if (std::filesystem::exists(initialDir)) {
                ofn.lpstrInitialDir = initialDir.c_str();
            }

            if (GetOpenFileNameW(&ofn)) {
                std::filesystem::path src(filePath);
                std::filesystem::path dstDir = std::filesystem::path(g_config.packages_path) / g_config.packages[g_sel_pkg].name;
                std::wstring fname = src.filename().wstring();

                // 若為 default 套裝，且檔案不在套裝目錄內，則自動複製
                if (g_config.packages[g_sel_pkg].name == L"00_default") {
                    if (std::filesystem::exists(dstDir)) {
                        std::filesystem::path target = dstDir / fname;
                        if (src != target) {
                            std::error_code ec;
                            std::filesystem::copy_file(src, target, std::filesystem::copy_options::overwrite_existing, ec);
                            if (ec) {
                                MessageBoxW(hwnd, (L"無法將檔案複製到 default 資料夾: " + toW(ec.message())).c_str(), L"錯誤", MB_OK | MB_ICONERROR);
                            }
                        }
                    }
                }

                SetWindowTextW(GetDlgItem(hwnd, IDC_EDIT_FILE), fname.c_str());
                // EN_CHANGE 會處理記憶體儲存與清單重新整理
            }
            break;
        }

        } // switch wmId
        return 0;
    }

    case WM_USER_TASK_DONE: {
        EnableWindow(hwnd, TRUE);
        SetForegroundWindow(hwnd);
        int action = (int)wParam;
        if      (action == 1) {
            SetWindowTextW(GetDlgItem(hwnd, IDC_LABEL_STATUS), L"輪循狀態: 運行中");
            MessageBoxW(hwnd, L"已建立系統排程並立即套用一次游標", L"啟動成功", MB_OK | MB_ICONINFORMATION);
        }
        else if (action == 2) {
            SetWindowTextW(GetDlgItem(hwnd, IDC_LABEL_STATUS), L"輪循狀態: 關閉");
            MessageBoxW(hwnd, L"已刪除背景輪循排程", L"終止成功", MB_OK | MB_ICONINFORMATION);
        }
        else if (action == 4) {
            SetWindowTextW(GetDlgItem(hwnd, IDC_LABEL_STATUS), L"輪循狀態: 關閉");
            // 恢復預設也會重置 UI 控制項
            SendMessageW(GetDlgItem(hwnd, IDC_SLIDER_SIZE), TBM_SETPOS, TRUE, 1);
            SetWindowTextW(GetDlgItem(hwnd, IDC_LABEL_SIZE_VAL), L"1");
            SendMessageW(GetDlgItem(hwnd, IDC_CHK_SHADOW), BM_SETCHECK, BST_UNCHECKED, 0);
            MessageBoxW(hwnd, L"已恢復系統預設游標與設定", L"恢復成功", MB_OK | MB_ICONINFORMATION);
        }
        else if (action == 31) MessageBoxW(hwnd, L"未選擇有效套裝", L"提示", MB_OK | MB_ICONWARNING);
        else if (action == 32) MessageBoxW(hwnd, L"套裝資料夾內找不到任何子資料夾", L"錯誤", MB_OK | MB_ICONERROR);
        else if (action == 33) MessageBoxW(hwnd, L"游標套用成功！", L"提示", MB_OK | MB_ICONINFORMATION);
        else if (action == 4)  MessageBoxW(hwnd, L"已恢復為系統預設游標", L"提示", MB_OK | MB_ICONINFORMATION);
        return 0;
    }

    case WM_DESTROY:
        CoUninitialize();
        PostQuitMessage(0);
        return 0;

    } // switch uMsg
    return DefWindowProcW(hwnd, uMsg, wParam, lParam);
}
