# Curator: Windows Cursor Rotator Tool (自動更換游標小工具)

一個基於 C++ 撰寫的 Windows 游標自動化專案。目前已完成**核心命令列 (CLI) 引擎**的開發，能夠幫助你自動且定期地更換 Windows 電腦的系統游標 (Cursor)。透過串接 Windows 工作排程器 (Task Scheduler)，讓每天的電腦操作都有一點小驚喜！

> **🚀 發展近況與 Roadmap**
> 目前釋出的版本為主打輕量且穩定可靠的 CLI 核心，背景切換與排程邏輯已完整就緒。
> **下一個目標是實現 GUI 圖形化介面**，屆時將支援更直覺的：
> - 視覺化排程設定 (Schedule Settings)
> - 游標套裝綁定與預覽 (Cursor Pack Bindings)
> 敬請期待！

## ✨ 特色功能 (CLI 核心)

- **多種模式支援**：支援 `random` (隨機抽籤) 以及 `round` (依序輪替) 模式。
- **背景自動化**：內建 `--start` 參數能直接將此工具加入「Windows 工作排程器」中，自動以設定的分鐘數在背景輪替游標。
- **安全不留痕**：透過修改 Windows 登錄檔 (Registry) `Control Panel\Cursors` 達成切換，並可使用 `--stop` 一鍵移除排程並還原 Windows 預設游標。
- **極簡高效**：無參雜任何肥大的框架，純 Windows API 與 C++ 標準函式庫，執行檔極度輕巧。

## 🗺️ 未來規劃 (Roadmap)

- [x] **階段一：CLI 核心引擎** (當前進度) - 穩定的游標切換、狀態還原、以及排程自動化功能。
- [ ] **階段二：GUI 圖形化控制面板** - 開發視覺化設定工具，免除手動編輯 JSON 的繁瑣。
- [ ] **階段三：進階套裝管理** - 支援快速匯入主題、套裝綁定與進階的輪替策略。

## 📦 如何建置 (Build)

本專案可以直接使用支援 C++ 17 以上的編譯器進行編譯（例如 MSVC, MinGW）：
（使用底下的指令或是你的 IDE 或是 VSCode 預設快捷鍵直接編譯 `curator_cli.cpp`）

```cmd
# 編譯範例 (使用 clang):
clang++ curator_cli.cpp -o curator_cli.exe -ladvapi32 -municode -mwindows
```

## ⚙️ 環境設定 (Config)

初次下載回來使用，請參考專案中的 `config.example.json`，將其複製並命名為 `config.json`：

```json
{
  "mode": "round", 
  "interval_minutes": 10,
  "cursor_dir": "C:\\Your\\Path\\To\\cursors",
  "shadow": true,
  "task_name": "CursorTool",
  "state_idx_path": "C:\\Your\\Path\\To\\state.idx"
}
```

### 參數說明：
- `mode`: `random` 為隨機切換；`round` 為依資料夾順序切換。
- `interval_minutes`: 排程執行的間隔時間 (以分鐘為單位)。
- `cursor_dir`: 存放所有游標組合 (Packs) 的「母目錄」絕對路徑。（注意：JSON 中的反斜線 `\` 必須寫成雙斜線 `\\`）。
- `shadow`: 是否為游標加上系統原生陰影 (`true`/`false`)，建議開啟，否則游標可能呈現透明等問題。
- `task_name`: 註冊進 Windows 排程器的工作名稱，未來可用來識別或手動刪除。
- `state_idx_path`: 存放當前游標組合的索引檔，請使用絕對路徑。

*(註：使用絕對路徑是因為 Windows 排程器在背景執行時，工作目錄通常為系統目錄而非程式所在目錄，為確保檔案不會分散，建議將 config.json 與 state.idx 放在專案目錄下。)*

## 🚀 使用方式

你可以透過終端機 (命令列) 來執行此工具：

```cmd
# 立即切換游標一次 (不加入背景排程)
curator_cli.exe --config "Your\Path\To\config.json" --run-once

# 若加上 --silent 將不印出任何執行訊息，適合背景執行
curator_cli.exe --config "Your\Path\To\config.json" --run-once --silent

# 建立 Windows 排程工作 (以 config 設定的分鐘數自動輪替)，並立刻套用新游標
curator_cli.exe --config "Your\Path\To\config.json" --start

# 移除排程工作，並立刻將游標還原為 Windows 預設狀態
curator_cli.exe --config "Your\Path\To\config.json" --stop
```

## 📂 游標包目錄結構

你的 `cursor_dir` 資料夾下，可以放入各個不同主題的游標子資料夾。每個子資料夾至少應該包含以下幾種游標檔案名稱（程式預設對應的檔名）：
- `Normal.ani` 或 `Normal.cur`
- `Help.ani` 或 `Help.cur`
- `Link.ani` 或 `Link.cur`
- `Text.ani` 或 `Text.cur`
- `Working.ani` 或 `Working.cur`

*(若有缺少檔案，程式會幫你 fallback 到 Windows 的預設游標；如果你想改變配對的檔名，可以直接至 `curator_cli.cpp` 中的 `kMap` 變數擴充與修改)。*

## ⚠️ 聲明
此工具純粹修改目前使用者的游標登錄檔並發送刷新 UI 訊號，如需解除安裝，只要執行 `--stop` 即可；請放心使用。
