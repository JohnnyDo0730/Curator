#include "config.h"
#include "utils.h"
#include <fstream>

std::string readAll(const std::wstring &path) {
  std::ifstream ifs(path);
  return std::string((std::istreambuf_iterator<char>(ifs)),
                     std::istreambuf_iterator<char>());
}

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
