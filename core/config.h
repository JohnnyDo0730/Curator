#pragma once
#include <string>

struct Config {
  std::wstring mode = L"random"; // random | round
  int interval_minutes = 10;
  std::wstring cursor_dir = L"C:/cursor_tool/cursors";
  bool shadow = true;
  std::wstring task_name = L"CursorTool";
  std::wstring state_idx_path = L"state.idx";
};

Config loadConfig(const std::wstring &path);
