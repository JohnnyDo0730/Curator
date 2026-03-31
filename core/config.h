#pragma once
#include <string>
#include <vector>
#include <map>

// ------------------------------------------------------------------
// CursorPackage: 單一套裝定義
//   name    — 套裝名稱（對應 packages-list-name 中的資料夾名稱）
//   mapping — 游標角色 -> 檔案名稱  (Arrow -> "Normal.ani" 等)
//             若 mapping 為空，代表套裝未設定自訂映射
// ------------------------------------------------------------------
struct CursorPackage {
  std::wstring name;
  std::map<std::wstring, std::wstring> mapping; // e.g. {L"Arrow" -> L"Normal.ani"}
};

// ------------------------------------------------------------------
// Config: 全域設定 + 套裝清單
// ------------------------------------------------------------------
struct Config {
  // [setting]
  std::wstring mode            = L"random"; // random | round
  int          interval_minutes = 10;
  bool         shadow           = true;
  std::wstring task_name        = L"CursorTool";
  std::wstring state_idx_path   = L"state.idx";

  // [packages]
  std::wstring packages_path;              // 套裝根資料夾
  std::vector<CursorPackage> packages;     // 套裝清單 (00_alternative = 預設)
};

// 已知游標角色名稱清單（與 registry 鍵值一致）
static const wchar_t* kCursorRoles[] = {
  L"Arrow", L"Hand", L"Help", L"IBeam", L"Wait",
  L"AppStarting", L"Crosshair", L"No",
  L"SizeAll", L"SizeNS", L"SizeWE", L"SizeNWSE", L"SizeNESW",
  L"Person", L"Pin", L"NWPen", L"UpArrow"
};

// 預設映射（角色 -> 檔案名稱），供套裝未指定時 fallback 使用
// （空字串代表清除該游標，使用系統預設樣式）
static inline std::map<std::wstring, std::wstring> defaultMapping() {
  return {
    {L"Arrow",        L"Normal.ani"},
    {L"Hand",         L"Link.ani"},
    {L"Help",         L"Help.ani"},
    {L"IBeam",        L"Text.ani"},
    {L"Wait",         L"Working.ani"},
    {L"AppStarting",  L"Working.ani"},
    {L"Crosshair",    L""},
    {L"No",           L""},
    {L"SizeAll",      L""},
    {L"SizeNS",       L""},
    {L"SizeWE",       L""},
    {L"SizeNWSE",     L""},
    {L"SizeNESW",     L""},
    {L"Person",       L""},
    {L"Pin",          L""},
    {L"NWPen",        L""},
    {L"UpArrow",      L""},
  };
}

Config loadConfig(const std::wstring &path);
void   saveConfig(const Config &c, const std::wstring &path);

// 取得指定套裝名稱的套裝索引（-1 若不存在）
int findPackageIndex(const Config &c, const std::wstring &name);
