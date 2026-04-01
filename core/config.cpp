#include "config.h"
#include "utils.h"
#include <fstream>

#include <nlohmann/json.hpp>

using json = nlohmann::json;

// ──────────────────────────────────────────────
// 內部輔助：將 json mapping 物件解析成 wstring map
// ──────────────────────────────────────────────
static std::map<std::wstring, std::wstring> parseMappingObj(const json &jMap) {
  std::map<std::wstring, std::wstring> m;
  for (auto &[k, v] : jMap.items()) {
    if (v.is_string()) {
      m[toW(k)] = toW(v.get<std::string>());
    }
  }
  return m;
}

// ──────────────────────────────────────────────
// loadConfig
// ──────────────────────────────────────────────
Config loadConfig(const std::wstring &path) {
  Config c;
  std::ifstream ifs(path);
  if (!ifs.is_open())
    return c;

  try {
    json j;
    ifs >> j;

    // ── [setting] 節點 ──
    if (j.contains("setting") && j["setting"].is_object()) {
      const json &s = j["setting"];

      if (s.contains("mode") && s["mode"].is_string())
        c.mode = toW(s["mode"].get<std::string>());

      if (s.contains("interval_minutes") && s["interval_minutes"].is_number())
        c.interval_minutes = s["interval_minutes"].get<int>();

      if (s.contains("shadow") && s["shadow"].is_boolean())
        c.shadow = s["shadow"].get<bool>();

      if (s.contains("cursor_size") && s["cursor_size"].is_number())
        c.cursor_size = s["cursor_size"].get<int>();

      if (s.contains("task_name") && s["task_name"].is_string())
        c.task_name = toW(s["task_name"].get<std::string>());

      if (s.contains("state_idx_path") && s["state_idx_path"].is_string())
        c.state_idx_path = toW(s["state_idx_path"].get<std::string>());
    }

    // ── [packages] 節點 ──
    if (j.contains("packages") && j["packages"].is_object()) {
      const json &pkg = j["packages"];

      if (pkg.contains("path") && pkg["path"].is_string())
        c.packages_path = toW(pkg["path"].get<std::string>());

      if (pkg.contains("list") && pkg["list"].is_array()) {
        for (const auto &item : pkg["list"]) {
          CursorPackage cp;
          if (item.contains("name") && item["name"].is_string())
            cp.name = toW(item["name"].get<std::string>());

          if (item.contains("mapping") && item["mapping"].is_object())
            cp.mapping = parseMappingObj(item["mapping"]);

          c.packages.push_back(std::move(cp));
        }
      }
    }

  } catch (const std::exception &) {
    // 解析失敗則回傳預設值
  }

  return c;
}

// ──────────────────────────────────────────────
// saveConfig
// ──────────────────────────────────────────────
void saveConfig(const Config &c, const std::wstring &path) {
  json j;

  // ── [setting] ──
  j["setting"]["mode"]             = toA(c.mode);
  j["setting"]["interval_minutes"] = c.interval_minutes;
  j["setting"]["shadow"]           = c.shadow;
  j["setting"]["cursor_size"]      = c.cursor_size;
  j["setting"]["task_name"]        = toA(c.task_name);
  j["setting"]["state_idx_path"]   = toA(c.state_idx_path);

  // ── [packages] ──
  j["packages"]["path"]   = toA(c.packages_path);
  j["packages"]["number"] = (int)c.packages.size();

  json jList = json::array();
  for (const auto &cp : c.packages) {
    json jItem;
    jItem["name"] = toA(cp.name);
    json jMap = json::object();
    for (const auto &[role, file] : cp.mapping) {
      jMap[toA(role)] = toA(file);
    }
    jItem["mapping"] = jMap;
    jList.push_back(std::move(jItem));
  }
  j["packages"]["list"] = jList;

  std::ofstream ofs(path);
  if (ofs.is_open()) {
    ofs << j.dump(2);
  }
}

// ──────────────────────────────────────────────
// findPackageIndex
// ──────────────────────────────────────────────
int findPackageIndex(const Config &c, const std::wstring &name) {
  for (int i = 0; i < (int)c.packages.size(); ++i) {
    if (c.packages[i].name == name)
      return i;
  }
  return -1;
}
