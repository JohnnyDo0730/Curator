#include "cursor.h"
#include "utils.h"
#include <windows.h>
#include <iostream>
#include <random>
#include <fstream>
#include <stdexcept>

#pragma comment(lib, "Advapi32.lib")

namespace fs = std::filesystem;

// ──────────────────────────────────────────────
// 低層 Registry 輔助
// ──────────────────────────────────────────────
bool setRegSz(HKEY root, const std::wstring &subkey, const std::wstring &name,
              const std::wstring &value) {
  HKEY hKey;
  LONG rc = RegOpenKeyExW(root, subkey.c_str(), 0, KEY_SET_VALUE, &hKey);
  if (rc != ERROR_SUCCESS) {
    std::wcerr << L"[RegOpenKeyExW FAIL] " << name << L" code=" << rc << std::endl;
    return false;
  }

  DWORD bytes = (DWORD)((value.size() + 1) * sizeof(wchar_t));
  rc = RegSetValueExW(hKey, name.c_str(), 0, REG_SZ,
                      (const BYTE *)value.c_str(), bytes);
  RegCloseKey(hKey);

  if (rc != ERROR_SUCCESS) {
    std::wcerr << L"[RegSetValueExW FAIL] " << name << L" code=" << rc << std::endl;
    return false;
  }
  return true;
}

// ──────────────────────────────────────────────
// applyPackWithMapping
//   packPath — 游標套裝資料夾
//   mapping  — 角色 -> 檔案名稱映射表
//              若 mapping 為空，退回內建預設映射
// ──────────────────────────────────────────────
void applyPackWithMapping(const fs::path &packPath,
                          const std::map<std::wstring, std::wstring> &mapping) {
  static const std::wstring regPath = L"Control Panel\\Cursors";

  // 1. 建立完整的 17 個角色映射 (預設值)
  auto fullMap = defaultMapping();

  // 2. 若有自訂 mapping，則進行覆蓋
  for (auto const& [role, file] : mapping) {
    fullMap[role] = file;
  }

  // 3. 逐一寫入 Registry
  for (auto const& [role, fileName] : fullMap) {
    std::wstring fullPath;
    if (!fileName.empty()) {
      fs::path candidate = packPath / fs::path(fileName);
      if (fs::exists(candidate)) {
        fullPath = candidate.wstring();
      }
    }
    setRegSz(HKEY_CURRENT_USER, regPath, role, fullPath);
  }

  SystemParametersInfoW(SPI_SETCURSORS, 0, NULL,
                        SPIF_UPDATEINIFILE | SPIF_SENDCHANGE);
}

// ──────────────────────────────────────────────
// applyPack (舊介面相容，使用內建預設映射)
// ──────────────────────────────────────────────
void applyPack(const fs::path &packPath) {
  applyPackWithMapping(packPath, {}); // 空 mapping → 使用 defaultMapping()
}

// ──────────────────────────────────────────────
// applyPackage
//   1. 嘗試從指定套裝 (pkg) 取得檔案
//   2. 若無，嘗試從 alternative 套裝取得檔案
//   3. 若皆無，退回系統預設 (空字串)
// ──────────────────────────────────────────────
void applyPackage(const Config &cfg, const CursorPackage &pkg) {
  fs::path packRoot = cfg.packages_path;
  fs::path packPath = packRoot / pkg.name;
  
  // 取得 00_alternative
  const CursorPackage* altPkg = nullptr;
  int altIdx = findPackageIndex(cfg, L"00_alternative");
  if (altIdx >= 0) altPkg = &cfg.packages[altIdx];

  // 封裝成最終要套用的 mapping
  std::map<std::wstring, std::wstring> finalMapping;
  auto fallback = defaultMapping();

  for (auto const* role : kCursorRoles) {
    std::wstring roleStr = role;
    std::wstring file;

    // A. 檢查指定套裝
    auto it = pkg.mapping.find(roleStr);
    if (it != pkg.mapping.end() && !it->second.empty()) {
      file = it->second;
    } 
    // B. 檢查 alternative 套裝
    else if (altPkg) {
      auto itAlt = altPkg->mapping.find(roleStr);
      if (itAlt != altPkg->mapping.end() && !itAlt->second.empty()) {
        file = itAlt->second;
      }
    }

    // C. 若皆無，由 applyPackWithMapping 處理 (或此處給予 fallback)
    // 這裡我們直接填入，若 file 仍為空，代表退回系統預設
    finalMapping[roleStr] = file;
  }

  applyPackWithMapping(packPath, finalMapping);
}

// ──────────────────────────────────────────────
// setCursorShadow
// ──────────────────────────────────────────────
void setCursorShadow(bool on) {
  std::wstring val = on ? L"1" : L"0";
  setRegSz(HKEY_CURRENT_USER, L"Control Panel\\Desktop", L"CursorShadow", val);
  SystemParametersInfoW(SPI_SETCURSORSHADOW, 0,
                        (PVOID)(INT_PTR)(on ? TRUE : FALSE),
                        SPIF_UPDATEINIFILE | SPIF_SENDCHANGE);
}

// ──────────────────────────────────────────────
// listPacks — 列舉 dir 下的所有子資料夾
// ──────────────────────────────────────────────
std::vector<fs::path> listPacks(const std::wstring &dir) {
  std::vector<fs::path> v;
  std::error_code ec;
  if (!fs::exists(dir)) return v;
  for (auto &e : fs::directory_iterator(dir, ec)) {
    if (e.is_directory()) {
      std::wstring name = e.path().filename().wstring();
      if (name != L"00_alternative") {
        v.push_back(e.path());
      }
    }
  }
  return v;
}

// ──────────────────────────────────────────────
// State index helpers (round-robin)
// ──────────────────────────────────────────────
int loadIndex(const std::wstring &path) {
  std::ifstream ifs(path);
  int i = 0;
  ifs >> i;
  return i;
}
void saveIndex(const std::wstring &path, int i) {
  std::ofstream ofs(path);
  ofs << i;
}

// ──────────────────────────────────────────────
// choosePack — 依模式選出要套用的套裝路徑
// ──────────────────────────────────────────────
fs::path choosePack(const std::vector<fs::path> &packs, const Config &cfg) {
  if (packs.empty())
    throw std::runtime_error("No packs found");
  if (cfg.mode == L"random") {
    static std::mt19937 rng((unsigned)time(NULL));
    std::uniform_int_distribution<int> dist(0, (int)packs.size() - 1);
    return packs[dist(rng)];
  } else { // round-robin
    int i        = loadIndex(cfg.state_idx_path);
    fs::path p   = packs[i % packs.size()];
    saveIndex(cfg.state_idx_path, (i + 1) % (int)packs.size());
    return p;
  }
}

// ──────────────────────────────────────────────
// restoreDefault — 清除所有游標 reg 值，回到系統預設
// ──────────────────────────────────────────────
void restoreDefault() {
  static const std::wstring regPath = L"Control Panel\\Cursors";
  auto defMap = defaultMapping();
  for (auto &[role, _] : defMap) {
    setRegSz(HKEY_CURRENT_USER, regPath, role, L"");
  }
  SystemParametersInfoW(SPI_SETCURSORS, 0, NULL,
                        SPIF_UPDATEINIFILE | SPIF_SENDCHANGE);
  setCursorShadow(false);
}
