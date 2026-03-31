#pragma once
#include <string>
#include <windows.h>

std::wstring toW(const std::string &s);
std::string toA(const std::wstring &w);
