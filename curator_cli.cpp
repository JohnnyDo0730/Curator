#include <windows.h>
// #include <shlwapi.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <random>
#include <io.h>
#include <fcntl.h>
#include <string>
#include <vector>

#pragma comment(lib, "Advapi32.lib")
#pragma comment(lib, "Shlwapi.lib")

namespace fs = std::filesystem;

// -------------------- early declaration --------------------
std::wstring toW(const std::string &s);
std::string toA(const std::wstring &w);

// -------------------- Config（極簡解析，使用wstring支持中文路径）
struct Config {
  std::wstring mode = L"random"; // random | round
  int interval_minutes = 10;
  std::wstring cursor_dir = L"C:/cursor_tool/cursors";
  bool shadow = true;
  std::wstring task_name = L"CursorTool";
  std::wstring state_idx_path =
      L"state.idx"; // absolute path for round-robin state file
};

std::string readAll(const std::wstring &path) {
  std::ifstream ifs(path);
  return std::string((std::istreambuf_iterator<char>(ifs)),
                     std::istreambuf_iterator<char>());
}

// 非嚴謹 JSON（最小可用，处理wstring路径）
std::wstring getJsonStringW(const std::string &s, const std::string &key,
                            const std::wstring &def) {
  auto p = s.find("\"" + key + "\"");
  if (p == std::string::npos)
    return def;
  auto q = s.find(":", p);
  auto a = s.find("\"", q);
  auto b = s.find("\"", a + 1);
  std::string result = (a != std::string::npos && b != std::string::npos)
                           ? s.substr(a + 1, b - a - 1)
                           : toA(def);
  return toW(result);
}
std::string getJsonString(const std::string &s, const std::string &key,
                          const std::string &def) {
  auto p = s.find("\"" + key + "\"");
  if (p == std::string::npos)
    return def;
  auto q = s.find(":", p);
  auto a = s.find("\"", q);
  auto b = s.find("\"", a + 1);
  return (a != std::string::npos && b != std::string::npos)
             ? s.substr(a + 1, b - a - 1)
             : def;
}
int getJsonInt(const std::string &s, const std::string &key, int def) {
  auto p = s.find("\"" + key + "\"");
  if (p == std::string::npos)
    return def;
  auto q = s.find(":", p);
  return std::stoi(s.substr(q + 1));
}
bool getJsonBool(const std::string &s, const std::string &key, bool def) {
  auto p = s.find("\"" + key + "\"");
  if (p == std::string::npos)
    return def;
  auto q = s.find(":", p);
  auto val = s.substr(q + 1, 5);
  return val.find("true") != std::string::npos;
}

Config loadConfig(const std::wstring &path) {
  Config c;
  std::string s = readAll(path);
  if (s.empty())
    return c;
  c.mode = getJsonStringW(s, "mode", c.mode);
  c.interval_minutes = getJsonInt(s, "interval_minutes", c.interval_minutes);
  c.cursor_dir = getJsonStringW(s, "cursor_dir", c.cursor_dir);
  c.shadow = getJsonBool(s, "shadow", c.shadow);
  c.task_name = getJsonStringW(s, "task_name", c.task_name);
  c.state_idx_path = getJsonStringW(s, "state_idx_path", c.state_idx_path);
  return c;
}

// -------------------- Registry helpers --------------------

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

std::wstring toW(const std::string &s) {
  int sz = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, NULL, 0);
  std::wstring w(sz, L'\0');
  MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, &w[0], sz);
  w.pop_back();
  return w;
}

std::string toA(const std::wstring &w) {
  int sz = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, NULL, 0, NULL, NULL);
  std::string s(sz, '\0');
  WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, &s[0], sz, NULL, NULL);
  s.pop_back();
  return s;
}

// -------------------- Cursor mapping --------------------
struct CursorMapItem {
  const wchar_t *regName;  // registry key name
  const wchar_t *fileName; // expected file in pack
};

// 常用子集（可自行擴充）
std::vector<CursorMapItem> kMap = {
    {L"Arrow", L"Normal.ani"},
    {L"Help", L"Help.ani"},
    {L"Hand", L"Link.ani"},
    {L"IBeam", L"Text.ani"},
    {L"Wait", L"Working.ani"}};

bool fileExists(const std::wstring &p) { return fs::exists(fs::path(p)); }

// -------------------- Core: apply cursor pack --------------------
void applyPack(const fs::path &packPath) {
  std::wstring regPath = L"Control Panel\\Cursors";

  for (auto &m : kMap) {
    std::wstring p = (packPath / fs::path(m.fileName)).wstring();
    // 若找不到自訂游標檔案，寫入空字串讓 Windows 使用該指標的系統預設值
    std::wstring use = fileExists(p) ? p : L"";
    setRegSz(HKEY_CURRENT_USER, regPath, m.regName, use);
  }

  // reload cursors
  SystemParametersInfoW(SPI_SETCURSORS, 0, NULL,
                        SPIF_UPDATEINIFILE | SPIF_SENDCHANGE);

}

// -------------------- CursorShadow --------------------
void setCursorShadow(bool on) {
  std::wstring val = on ? L"1" : L"0";
  setRegSz(HKEY_CURRENT_USER, L"Control Panel\\Desktop", L"CursorShadow", val);

  // 提交滑鼠陰影的狀態更新，以便動態生效而不需要登出
  SystemParametersInfoW(SPI_SETCURSORSHADOW, 0, (PVOID)(INT_PTR)(on ? TRUE : FALSE),
                        SPIF_UPDATEINIFILE | SPIF_SENDCHANGE);
}

// -------------------- Packs discovery & selection --------------------
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

// -------------------- Task Scheduler (schtasks) --------------------
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

  // std::wcout << L"[CMD] " << cmd << L"\n";
  runCmd(cmd);
}

void deleteTask(const Config &cfg) {
  std::wstring cmd = L"schtasks /delete /tn \"" + cfg.task_name + L"\" /f";
  runCmd(cmd);
}

// -------------------- Default restore --------------------
void restoreDefault() {
  std::wstring regPath = L"Control Panel\\Cursors";
  for (auto &m : kMap) {
    // 寫入空字串即可讓 Windows 使用該游標型別的系統預設值（等同於點擊「使用預設值」按鈕）
    setRegSz(HKEY_CURRENT_USER, regPath, m.regName, L"");
  }

  // 讓系統重新讀取 Registry 並套用
  SystemParametersInfoW(SPI_SETCURSORS, 0, NULL,
                        SPIF_UPDATEINIFILE | SPIF_SENDCHANGE);

  setCursorShadow(false);
}

// -------------------- CLI --------------------
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