#include <windows.h>
#include "../core/config.h"
#include "../core/cursor.h"
#include "../core/scheduler.h"

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow) {
    // 這裡預留給未來的 GUI 初始化代碼
    // 未來我們可以在這裡加載配置檔，並提供 UI 介面來修改游標設定與排程
    
    MessageBoxW(NULL, L"GUI is not implemented yet. Please use CLI version.", L"Cursor Tool GUI", MB_OK | MB_ICONINFORMATION);

    return 0;
}
