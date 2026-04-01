#pragma once
#include <vector>
#include <map>
#include <filesystem>
#include "config.h"

// 套用套裝（使用套裝自帶 mapping，若 mapping 為空退回預設）
void applyPackWithMapping(const std::filesystem::path &packPath,
                          const std::map<std::wstring, std::wstring> &mapping);

// 套用套裝（從 Config packages list 中套用一個具名套裝，支援 default fallback）
void applyPackage(const Config &cfg, const CursorPackage &pkg);

// 套用套裝（舊介面相容，使用內建預設映射）
void applyPack(const std::filesystem::path &packPath);

void setCursorShadow(bool on);
void setCursorSize(int size);
std::vector<std::filesystem::path> listPacks(const std::wstring &dir);
std::filesystem::path choosePack(const std::vector<std::filesystem::path> &packs,
                                 const Config &cfg);
void restoreDefault();
void clearCustomCursors();
