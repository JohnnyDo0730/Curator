#include "autofill.h"
#include "utils.h"
#include <fstream>
#include <filesystem>
#include <nlohmann/json.hpp>

namespace fs = std::filesystem;
using json = nlohmann::json;

void Autofill::load(const std::wstring& path) {
    std::ifstream ifs(path);
    if (!ifs.is_open()) return;
    try {
        json j;
        ifs >> j;
        for (auto& [k, v] : j.items()) {
            std::wstring role = toW(k);
            if (v.is_array()) {
                for (auto& fn : v) {
                    if (fn.is_string()) {
                        entries[role].push_back(toW(fn.get<std::string>()));
                    }
                }
            }
        }
    } catch (...) {}
}

void Autofill::save(const std::wstring& path) {
    json j = json::object();
    for (auto& [role, list] : entries) {
        json jArray = json::array();
        for (auto& fn : list) {
            jArray.push_back(toA(fn));
        }
        j[toA(role)] = jArray;
    }
    std::ofstream ofs(path);
    if (ofs.is_open()) ofs << j.dump(2);
}

// 根據現有規則對套裝資料夾進行掃描，自動填充 mapping
bool Autofill::applyTo(CursorPackage& pkg, const std::wstring& packageFolder) {
    bool changed = false;
    std::error_code ec;
    
    // 先讀取資料夾內所有可能的游標檔案
    std::vector<std::wstring> localFiles;
    for (auto& p : fs::directory_iterator(packageFolder, ec)) {
        if (p.is_directory()) continue;
        std::wstring fname = p.path().filename().wstring();
        std::wstring ext = p.path().extension().wstring();
        // 僅支持 cur, ani
        if (ext == L".cur" || ext == L".ani" || ext == L".CUR" || ext == L".ANI") {
            localFiles.push_back(fname);
        }
    }
    
    // 如果 localFiles 為空，代表目前 pkg 資料夾不存在或無檔案
    if (localFiles.empty()) return false;

    // 對每個 Role 比對已知規則
    for (int i = 0; i < kCursorRoleCount; ++i) {
        std::wstring role = kCursorRoles[i];
        
        // 如果目前已有值且檔案確實存在於資料夾中，不覆蓋 (尊重手動)
        if (!pkg.mapping[role].empty()) {
            bool exists = false;
            for (auto& lf : localFiles) {
                if (pkg.mapping[role] == lf) { exists = true; break; }
            }
            if (exists) continue;
        }

        // 開始尋找匹配
        if (entries.find(role) != entries.end()) {
            for (auto& ruleFn : entries[role]) {
                // 比對不分大小寫
                std::wstring lowRule = ruleFn;
                for (auto& c : lowRule) c = std::tolower(c);
                
                for (auto& lf : localFiles) {
                    std::wstring lowLF = lf;
                    for (auto& c : lowLF) c = std::tolower(c);
                    
                    if (lowLF == lowRule) {
                        pkg.mapping[role] = lf;
                        changed = true;
                        goto next_role;
                    }
                }
            }
        }
        next_role:;
    }
    
    return changed;
}

void Autofill::addRule(const std::wstring& role, const std::wstring& filename) {
    if (role.empty() || filename.empty()) return;
    auto& list = entries[role];
    // 檢查是否已存在
    for (auto& ex : list) {
        if (ex == filename) return;
    }
    list.push_back(filename);
}

Autofill loadAutofill(const std::wstring& path) {
    Autofill af;
    af.load(path);

    // 如果是全新檔案，加入一些預設常用規則
    if (af.entries.empty()) {
        af.addRule(L"Arrow", L"Normal.cur");
        af.addRule(L"Arrow", L"Normal.ani");
        af.addRule(L"Hand", L"Link.cur");
        af.addRule(L"Hand", L"Link.ani");
        af.addRule(L"Help", L"Help.cur");
        af.addRule(L"Help", L"Help.ani");
        af.addRule(L"Wait", L"Busy.cur");
        af.addRule(L"Wait", L"Busy.ani");
        af.addRule(L"AppStarting", L"Working.cur");
        af.addRule(L"AppStarting", L"Working.ani");
        af.addRule(L"IBeam", L"Text.cur");
        af.addRule(L"IBeam", L"Text.ani");
        af.save(path);
    }
    return af;
}

void saveAutofill(const Autofill& af, const std::wstring& path) {
    const_cast<Autofill&>(af).save(path);
}
