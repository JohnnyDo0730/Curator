#include <windows.h>
#include <iostream>
#include <io.h>
#include <fcntl.h>
#include <string>
#include <stdexcept>

#include "../core/config.h"
#include "../core/cursor.h"
#include "../core/scheduler.h"
#include "../core/utils.h"

void execute_main(const Config &cfg, const std::wstring &arg, const std::wstring &configPath, bool silent) {
  try {
    if (arg == L"--run-once") {
      auto packs = listPacks(cfg.cursor_dir);
      auto p = choosePack(packs, cfg);
      applyPack(p);
      
      setCursorShadow(cfg.shadow);

      if (!silent) {
        std::wcout << L"Applied: " << p.wstring() << L"\n";
      }
    } else if (arg == L"--start") {
      createTask(cfg, configPath);
      // 立即套用一次
      auto packs = listPacks(cfg.cursor_dir);
      auto p = choosePack(packs, cfg);
      applyPack(p);

      setCursorShadow(cfg.shadow);

      if (!silent) {
        std::wcout << L"Started\n";
      }
    } else if (arg == L"--stop") {
      deleteTask(cfg);
      restoreDefault();
      if (!silent) {
        std::wcout << L"Stopped & restored\n";
      }
    } else {
      if (!silent) {
        std::wcout << L"Unknown arg\n";
      }
    }
  } catch (const std::exception &e) {
    if (!silent) {
      std::wcerr << L"Error: " << toW(e.what()) << L"\n";
    }
  }
}

int wmain(int argc, wchar_t **argv) {
  _setmode(_fileno(stdout), _O_U16TEXT);
  _setmode(_fileno(stderr), _O_U16TEXT);

  if (argc < 4) {
    std::wcout << L"argument fault\n";
    std::wcout << L"Config Path: --config <path>\n";
    std::wcout << L"Mode: --start | --stop | --run-once\n";
    std::wcout << L"Silent: --silent | -s\n";
    return 0;
  }

  Config cfg;
  std::wstring configPath;
  if (std::wstring(argv[1]) == L"--config") {
    configPath = argv[2];
    cfg = loadConfig(configPath);
  }

  std::wstring arg_mode = argv[3];
  if (argc >= 5 && (std::wstring(argv[4]) == L"--silent" ||
                    std::wstring(argv[4]) == L"-s")) {
    
    std::wcout << L"Silent mode\n";
    execute_main(cfg, arg_mode, configPath, true);
  } else {

    std::wcout << L"Normal mode\n";
    execute_main(cfg, arg_mode, configPath, false);
  }

  return 0;
}
