# c-game — Roguelike 地城原型（混合式：C 核心 + Python Host）

一人組期末專題骨架：**遊戲本體邏輯與程序化地城生成在 C**，**終端機顯示與輸入在 Python**（`ctypes` 載入 `libroguecore.so`）。符合課程「整體須至少一部份使用 C」且邊界清楚，方便書面報告說明。

## 題目與 MVP

- **主題**：類 Roguelike — 在隨機生成的地城中以 WASD 移動，**走到樓梯 `>` 即勝利**。
- **C 核心**：可重現隨機數（seed）、房間+走廊地城生成、地磚網格、移動碰撞、勝利判定。
- **Host**：繪製終端機地圖（清屏、ANSI 色彩、**單鍵移動免 Enter**）；不依賴 pip 套件。

詳見 [docs/PROPOSAL.md](docs/PROPOSAL.md)（提案用五點大綱）。

## 環境：Ubuntu 24.04

助教評分環境以此為準。請至少安裝：

```bash
sudo apt update
sudo apt install -y build-essential python3
```

驗證編譯器（版本僅供參考，課程環境以 `apt` 為準）：

```bash
gcc --version
python3 --version
```

## 建置與執行

於專案根目錄：

```bash
chmod +x build.sh run.sh tests/run_smoke.sh   # 若從 zip 解壓後無執行位元
./build.sh
./run.sh --seed 42
```

或使用 Make：

```bash
make
python3 host/app.py --seed 42
```

**純 C CLI**（不依賴 Python，方便除錯核心）：

```bash
./rogue_cli --seed 42
```

### 怎麼玩（重要）

畫面上 **`@` 是你**；**牆／地板／樓梯**在彩色模式下會用不同顏色標示（地板顯示為 `·`）。**走到樓梯格就獲勝**。

**互動終端機（預設）**：**直接按鍵即可**，不必按 Enter。`w` `a` `s` `d` 或**方向鍵**移動，`?` 說明，`q` 離開。畫面會定時清屏重繪，底部有狀態列（例如「撞牆」）。

**管道／腳本／非 TTY**：會自動改為「每行一個字母 + Enter」，與舊版相同。

手動切換：

- `python3 host/app.py --line-mode` — 強制每行輸入（需 Enter）
- `python3 host/app.py --no-color` 或環境變數 `NO_COLOR=1` — 關閉 ANSI 色彩
- `./rogue_cli --line-mode` / `./rogue_cli --no-color` — C 版同等選項

### 若還想要更「像遊戲」的呈現（選修）

目前刻意**零 pip 依賴**，方便 Ubuntu 上助教一鍵執行。若要明顯升級質感，可在報告中列為「加分項」並在 README 補安裝步驟，例如：

- **pygame**：磚塊精靈圖、視窗全螢幕（需 `pip` / `apt` 依賴說明）
- **textual**（TUI）：面板、邊框、主題（同樣需依賴）
- **網頁前端 + 現有 C API**：以 WebSocket／REST 接 `libroguecore.so`（架構變動較大）

核心規則仍建議留在 C，Host 只換「皮」。

## 驗收（助教照抄應可看到）

成功建置後執行 `./run.sh --seed 1`，應出現矩形地圖；在互動終端機用 **WASD（免 Enter）** 可看到 `@` 移動；抵達樓梯時顯示勝利訊息。

自動化煙霧測試：

```bash
make test
```

## 目錄結構

| 路徑 | 說明 |
|------|------|
| `include/roguecore.h` | C 公開 API（給 CLI / ctypes） |
| `src/c/rng.c` | 可重現虛擬亂數 |
| `src/c/dungeon.c` | 房間+走廊地城生成 |
| `src/c/game.c` | 遊戲狀態、移動、勝利判定 |
| `src/c/cli_main.c` | 純 C 終端機前端 |
| `host/app.py` | Python Host |
| `host/terminal_ui.py` | 終端機清屏、色彩、單鍵輸入 |
| `tests/run_smoke.sh` | 建置 + CLI 一筆輸入 + Python `--smoke` |

## 動態函式庫載入說明

- `rogue_cli` 連結時使用 `-Wl,-rpath,'$ORIGIN'`，執行檔與 `libroguecore.so` 需位於**同一目錄**（zip 解壓後保持如此即可）。
- `host/app.py` 以**絕對路徑**載入專案根目錄的 `libroguecore.so`，一般不需設定 `LD_LIBRARY_PATH`。

若於特殊環境下 `python3` 無法載入 `.so`，可嘗試：

```bash
export LD_LIBRARY_PATH="$(pwd):${LD_LIBRARY_PATH:-}"
python3 host/app.py
```

## 報告與誠信（課程要求）

- **LLM 使用**：若使用任何 LLM，請於書面報告載明工具、範圍（哪些檔案／模組）、提示詞粒度與人工審查方式。
- **一魚兩吃／延伸作品**：須於提案與報告主動說明；此範本為課程專用新專案，無外部基底 repo。
- **素材**：本 MVP 僅 ASCII，無外部圖音檔；若你自行加入資產，請於報告列出授權來源。

口頭 demo 檢查清單：[docs/DEMO_CHECKLIST.md](docs/DEMO_CHECKLIST.md)。
