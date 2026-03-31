#include "utils.h"

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
