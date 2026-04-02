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

bool setRegDword(HKEY root, const std::wstring &subkey, const std::wstring &name,
                 DWORD value) {
  HKEY hKey;
  LONG rc = RegOpenKeyExW(root, subkey.c_str(), 0, KEY_SET_VALUE, &hKey);
  if (rc != ERROR_SUCCESS) return false;
  rc = RegSetValueExW(hKey, name.c_str(), 0, REG_DWORD, (const BYTE *)&value, sizeof(DWORD));
  RegCloseKey(hKey);
  return rc == ERROR_SUCCESS;
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
//   2. 若無，嘗試從 default 套裝取得檔案
//   3. 若皆無，退回系統預設 (空字串)
// 套用套裝（從 Config packages list 中套用一個具名套裝，支援 default fallback）
// 回傳 true 代表找到套裝資料夾，false 代表資料夾不存在（將回歸系統預設）
// ──────────────────────────────────────────────
bool applyPackage(const Config &cfg, const CursorPackage &pkg) {
  fs::path packRoot = cfg.packages_path;
  fs::path pkgDir = packRoot / pkg.name;
  bool pkgExists = fs::exists(pkgDir);
  
  // 取得 00_default 作為備援
  const CursorPackage* altPkg = nullptr;
  int altIdx = findPackageIndex(cfg, L"00_default");
  if (altIdx >= 0) altPkg = &cfg.packages[altIdx];

  // 封裝成最終要套用的 mapping (只放入確定的絕對路徑)
  std::map<std::wstring, std::wstring> finalMapping;

  for (auto const* role : kCursorRoles) {
    std::wstring roleStr = role;
    std::wstring resolvedPath;

    // A. 檢查指定套裝，且檔案必須存在
    auto it = pkg.mapping.find(roleStr);
    if (it != pkg.mapping.end() && !it->second.empty()) {
      fs::path p = packRoot / pkg.name / it->second;
      if (fs::exists(p)) {
        resolvedPath = p.wstring();
      }
    }

    // B. 若指定套裝無此角色或檔案不存在，檢查 default 套裝
    if (resolvedPath.empty() && altPkg) {
      auto itAlt = altPkg->mapping.find(roleStr);
      if (itAlt != altPkg->mapping.end() && !itAlt->second.empty()) {
        fs::path p = packRoot / altPkg->name / itAlt->second;
        if (fs::exists(p)) {
          resolvedPath = p.wstring();
        }
      }
    }

    // 若找到了路徑（來自 pkg 或 altPkg），則存入 finalMapping
    // 若皆無，則不放入 mapping，讓 applyPackWithMapping 使用 defaultMapping 檢查 pkg 下的預設檔名
    if (!resolvedPath.empty()) {
      finalMapping[roleStr] = resolvedPath;
    }
  }

  // 以指定套裝的路徑作為基礎，執行最後的 registry 寫入
  applyPackWithMapping(pkgDir, finalMapping);
  return pkgExists;
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
// setCursorSize
//   size: 1 (預設) ~ 16
// ──────────────────────────────────────────────
#ifndef SPI_SETCURSORSIZE
#define SPI_SETCURSORSIZE 0x2029
#endif

void setCursorSize(int size) {
  if (size < 1) size = 1;
  if (size > 16) size = 16;
  
  // 1. Accessibility Registry
  setRegDword(HKEY_CURRENT_USER, L"Software\\Microsoft\\Accessibility", L"CursorSize", (DWORD)size);
  
  // 2. Control Panel Cursors Registry
  setRegDword(HKEY_CURRENT_USER, L"Control Panel\\Cursors", L"CursorBaseSize", (DWORD)(size * 32));
  
  // 3. System Apply (Using undocumented 0x2029 for accessibility cursor size)
  SystemParametersInfoW(SPI_SETCURSORSIZE, (UINT)size, NULL, SPIF_UPDATEINIFILE | SPIF_SENDCHANGE);
  
  // 4. Force redraw
  SystemParametersInfoW(SPI_SETCURSORS, 0, NULL, SPIF_UPDATEINIFILE | SPIF_SENDCHANGE);
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
      if (name != L"00_default") {
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
// clearCustomCursors — 清除所有游標自定義路徑，強制回到系統預設
// ──────────────────────────────────────────────
void clearCustomCursors() {
  static const std::wstring regPath = L"Control Panel\\Cursors";
  auto defMap = defaultMapping();
  for (auto &[role, _] : defMap) {
    setRegSz(HKEY_CURRENT_USER, regPath, role, L"");
  }
  SystemParametersInfoW(SPI_SETCURSORS, 0, NULL,
                        SPIF_UPDATEINIFILE | SPIF_SENDCHANGE);
}

// ──────────────────────────────────────────────
// restoreDefault — 完整恢復出廠設定
// ──────────────────────────────────────────────
void restoreDefault() {
  clearCustomCursors();
  setCursorShadow(false);
  setCursorSize(1);
}
