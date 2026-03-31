#include "cursor.h"
#include "utils.h"
#include <windows.h>
#include <iostream>
#include <random>
#include <fstream>
#include <stdexcept>

#pragma comment(lib, "Advapi32.lib")

namespace fs = std::filesystem;

bool setRegSz(HKEY root, const std::wstring &subkey, const std::wstring &name,
              const std::wstring &value) {

  HKEY hKey;
  LONG rc = RegOpenKeyExW(root, subkey.c_str(), 0, KEY_SET_VALUE, &hKey);

  if (rc != ERROR_SUCCESS) {
    std::wcerr << L"[RegOpenKeyExW FAIL] " << name << L" code=" << rc
               << std::endl;
    return false;
  }

  DWORD bytes = (DWORD)((value.size() + 1) * sizeof(wchar_t));

  rc = RegSetValueExW(hKey, name.c_str(), 0, REG_SZ,
                      (const BYTE *)value.c_str(), bytes);

  if (rc != ERROR_SUCCESS) {
    std::wcerr << L"[RegSetValueExW FAIL] " << name << L" code=" << rc
               << std::endl;
    RegCloseKey(hKey);
    return false;
  }

  RegCloseKey(hKey);

  return true;
}

struct CursorMapItem {
  const wchar_t *regName;  // registry key name
  const wchar_t *fileName; // expected file in pack
};

std::vector<CursorMapItem> kMap = {
    {L"Arrow", L"Normal.ani"},
    {L"Help", L"Help.ani"},
    {L"Hand", L"Link.ani"},
    {L"IBeam", L"Text.ani"},
    {L"Wait", L"Working.ani"}};

bool fileExists(const std::wstring &p) { return fs::exists(fs::path(p)); }

void applyPack(const fs::path &packPath) {
  std::wstring regPath = L"Control Panel\\Cursors";

  for (auto &m : kMap) {
    std::wstring p = (packPath / fs::path(m.fileName)).wstring();
    std::wstring use = fileExists(p) ? p : L"";
    setRegSz(HKEY_CURRENT_USER, regPath, m.regName, use);
  }

  SystemParametersInfoW(SPI_SETCURSORS, 0, NULL,
                        SPIF_UPDATEINIFILE | SPIF_SENDCHANGE);
}

void setCursorShadow(bool on) {
  std::wstring val = on ? L"1" : L"0";
  setRegSz(HKEY_CURRENT_USER, L"Control Panel\\Desktop", L"CursorShadow", val);

  SystemParametersInfoW(SPI_SETCURSORSHADOW, 0, (PVOID)(INT_PTR)(on ? TRUE : FALSE),
                        SPIF_UPDATEINIFILE | SPIF_SENDCHANGE);
}

std::vector<fs::path> listPacks(const std::wstring &dir) {
  std::vector<fs::path> v;
  for (auto &e : fs::directory_iterator(dir)) {
    if (e.is_directory())
      v.push_back(e.path());
  }
  return v;
}

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

fs::path choosePack(const std::vector<fs::path> &packs, const Config &cfg) {
  if (packs.empty())
    throw std::runtime_error("No packs found");
  if (cfg.mode == L"random") {
    static std::mt19937 rng((unsigned)time(NULL));
    std::uniform_int_distribution<int> dist(0, (int)packs.size() - 1);
    return packs[dist(rng)];
  } else { // round-robin
    int i = loadIndex(cfg.state_idx_path);
    fs::path p = packs[i % packs.size()];
    saveIndex(cfg.state_idx_path, (i + 1) % (int)packs.size());
    return p;
  }
}

void restoreDefault() {
  std::wstring regPath = L"Control Panel\\Cursors";
  for (auto &m : kMap) {
    setRegSz(HKEY_CURRENT_USER, regPath, m.regName, L"");
  }

  SystemParametersInfoW(SPI_SETCURSORS, 0, NULL,
                        SPIF_UPDATEINIFILE | SPIF_SENDCHANGE);

  setCursorShadow(false);
}
