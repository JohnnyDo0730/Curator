#pragma once
#include <vector>
#include <filesystem>
#include "config.h"

void applyPack(const std::filesystem::path &packPath);
void setCursorShadow(bool on);
std::vector<std::filesystem::path> listPacks(const std::wstring &dir);
std::filesystem::path choosePack(const std::vector<std::filesystem::path> &packs, const Config &cfg);
void restoreDefault();
