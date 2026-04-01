#include "scheduler.h"
#include <windows.h>
#include <iostream>

std::wstring exePath() {
  wchar_t buf[MAX_PATH];
  GetModuleFileNameW(NULL, buf, MAX_PATH);
  return std::wstring(buf);
}

int runCmd(const std::wstring &cmd) { return _wsystem(cmd.c_str()); }

void createTask(const Config &cfg, const std::wstring &configPath) {
  std::wstring cmd = L"schtasks /create /sc minute /mo " +
                     std::to_wstring(cfg.interval_minutes) +
                     L" /st 00:00 /tn \"" + cfg.task_name + L"\" /tr " +
                     (L"\"" + exePath() + L" --config \"" + configPath + L"\" --run-once --silent\"") + L" /f";

  runCmd(cmd);
}

void deleteTask(const Config &cfg) {
  std::wstring cmd = L"schtasks /delete /tn \"" + cfg.task_name + L"\" /f >nul 2>&1";
  runCmd(cmd);
}

bool checkTaskExists(const Config &cfg) {
  std::wstring cmd = L"schtasks /query /tn \"" + cfg.task_name + L"\" >nul 2>&1";
  return runCmd(cmd) == 0;
}
