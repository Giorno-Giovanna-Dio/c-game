# c-game — Roguelike 地城原型（混合式：C 核心 + Python Host）

一人組期末專題骨架：**遊戲本體邏輯與程序化地城生成在 C**，**終端機顯示與輸入在 Python**（`ctypes` 載入 `libroguecore.so`）。符合課程「整體須至少一部份使用 C」且邊界清楚，方便書面報告說明。

## 題目與 MVP

- **主題**：類 Roguelike — 在隨機生成的地城中以 WASD 移動，**走到樓梯 `>` 即勝利**。
- **C 核心**：可重現隨機數（seed）、房間+走廊地城生成、地磚網格、移動碰撞、勝利判定。
- **Host**：繪製 ASCII 地圖並讀取鍵盤；不依賴 pip 套件。

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

畫面上 **`@` 是你**，**`.` 是可走的地板**，**`#` 是牆**（不能穿過）。地圖裡會有一個 **`>`** 代表樓梯；**走到那一格（`@` 與 `>` 重疊）就獲勝**。

程式跑起來後，**最下面**會出現一行提示，例如：`移動 (w/a/s/d，? 說明，q 離開) >`  
請在 **`>` 右邊**用鍵盤輸入**一個字母**，再按 **Enter**（不是用滑鼠點地圖）：

| 鍵 | 動作 |
|----|------|
| `w` | 往上移一格 |
| `s` | 往下 |
| `a` | 往左 |
| `d` | 往右 |
| `?` 或 `h` | 再顯示說明 |
| `q` | 結束 |

> **易混淆**：提示符號最後的 `>` 只是「等你打字」；**關卡目標**是地圖格子裡的 **`>`** 樓梯符號。

## 驗收（助教照抄應可看到）

成功建置後執行 `./run.sh --seed 1`，應出現以 `#` `.` `>` 與 `@` 構成的矩形地圖，並在輸入 `w/a/s/d` 後看到 `@` 移動；抵達 `>` 時顯示勝利訊息。

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
