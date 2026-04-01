#pragma once
#include <string>
#include <vector>
#include <map>
#include "config.h"

// AutofillRule: 角色 -> 可能的檔名清單
struct AutofillEntry {
    std::wstring role;
    std::vector<std::wstring> filenames;
};

class Autofill {
public:
    std::map<std::wstring, std::vector<std::wstring>> entries; // role -> filenames

    void load(const std::wstring& path);
    void save(const std::wstring& path);
    
    // 根據現有規則對套裝資料夾進行掃描，自動填充 mapping
    // 只會填充目前的 mapping 中為空或沒指定的項目
    bool applyTo(CursorPackage& pkg, const std::wstring& packageFolder);
    
    // 新增一條規則
    void addRule(const std::wstring& role, const std::wstring& filename);
};

Autofill loadAutofill(const std::wstring& path);
void saveAutofill(const Autofill& af, const std::wstring& path);
