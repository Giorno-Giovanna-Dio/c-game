# c-game — Roguelike 地城探索（C 核心 + Python Host）

一人組期末專題：**遊戲規則與地城生成在 C**，**終端機顯示與輸入在 Python**（`ctypes` 載入 `libroguecore.so`）。

玩家在程序化生成的地城中探索、戰鬥、撿道具、下樓，目標是**通關全部 5 層**。亦提供純 C 版 `rogue_cli` 作除錯與備援 demo。

---

## 快速開始（助教驗收用，建議照抄）

**環境**：Ubuntu 24.04（或具 `gcc`、`python3` 的 Linux/macOS）

```bash
# 1. 解壓 zip 後進入專案根目錄
cd c-game

# 2. 若腳本無執行權限（zip 常見情況）
chmod +x build.sh run.sh tests/run_smoke.sh

# 3. 建置
./build.sh

# 4. 執行（建議互動式終端機，視窗寬度 ≥ 115 字元較佳）
./run.sh --seed 42
```

**預期結果**：

- 出現彩色地圖，`@` 為玩家
- 直接按 `w/a/s/d` 或方向鍵可移動（**不需 Enter**）
- 靠近怪物或道具時，右側顯示對應 ASCII 預覽
- 通關第 5 層後顯示勝利訊息

**自動化測試**：

```bash
make test
```

應印出 `smoke OK` 且無錯誤。

---

## 系統需求

### Ubuntu 24.04（課程評分環境）

```bash
sudo apt update
sudo apt install -y build-essential python3
```

驗證：

```bash
gcc --version
python3 --version
```

### 依賴說明

- **不需要** `pip` 或任何 Python 第三方套件
- **不需要** 網路下載（解壓、建置、執行皆可離線）
- 終端機需支援 ANSI 色彩（預設開啟；可用 `--no-color` 關閉）

---

## 建置方式

### 方法一：建置腳本（推薦）

```bash
./build.sh
```

產出：

- `libroguecore.so` — C 遊戲核心（動態函式庫）
- `rogue_cli` — 純 C 終端機前端

### 方法二：Make

```bash
make          # 建置
make clean    # 清除編譯產物
make test     # 建置 + 煙霧測試
```

---

## 執行方式

### Python Host（主要遊玩介面）

```bash
./run.sh --seed 42
# 等同於
python3 host/app.py --seed 42
```

| 參數 | 說明 |
|------|------|
| `--seed N` | 隨機種子（預設 1）；相同 seed 可重現相同地城 |
| `--line-mode` | 每行輸入一個指令並按 Enter（管道/除錯用） |
| `--no-color` | 關閉 ANSI 色彩 |
| `--smoke` | 載入 `.so` 並做最小 API 測試後結束 |
| `-h` / `--help` | 顯示說明 |

環境變數 `NO_COLOR=1` 亦可關閉色彩。

### 純 C CLI（不依賴 Python）

```bash
./rogue_cli --seed 42
```

支援 `--line-mode`、`--no-color`，行為與 Python 版核心規則相同，但無右側 ASCII 預覽面板。

---

## 遊戲說明

### 操作

| 按鍵 | 功能 |
|------|------|
| `w` / `a` / `s` / `d` 或方向鍵 | 移動 |
| `e` | 使用背包藥水 |
| `?` 或 `h` | 說明 |
| `q` | 離開 |

互動終端機（TTY）下**單鍵即動**，不必按 Enter。非 TTY 環境會自動改為 line mode。

### 地圖符號

| 符號 | 意義 |
|------|------|
| `@` | 玩家 |
| `#` / `█` | 牆 |
| `·` / `.` | 地板 |
| `>` | 樓梯（下樓） |
| `E` | 殭屍 |
| `S` | 史萊姆 |
| `H` | 獵人 |
| `!` | 治療藥水 |
| `~` | 閃現卷軸 |
| `%` | 地圖卷軸 |

### 勝利與失敗

- **勝利**：完成第 5 層並在樓梯 `>` 上下樓。
- **失敗**：HP 歸零（被怪物擊殺，或步數耗盡後持續受傷致死）。

### 遊戲機制摘要

- **多層地城**：共 5 層；每層重新隨機生成；下樓回復部分 HP。
- **步數限制**：每層有步數上限；耗盡後每步扣 HP。
- **視野（Fog of War）**：未探索不可見；走過但不在視野內顯示暗色殘影。
- **怪物**（三種 AI）：
  - 殭屍（E）：玩家 6 格內才追蹤
  - 史萊姆（S）：隨機移動，耐打，擊殺回 +2 HP
  - 獵人（H）：一回合兩步，HP 低會逃跑，第 3 層後較常出現
- **道具**：
  - 藥水（!）：拾取回血；滿血可存入背包（最多 5 瓶），按 `e` 使用
  - 閃現卷軸（~）：瞬移到較安全位置
  - 地圖卷軸（%）：揭示整層地形
- **右側 ASCII 預覽**：玩家周圍 3×3 九宮格內有怪物或道具時，右側顯示對應大型 ASCII 圖（怪物優先於道具）。

---

## 專案結構

```
c-game/
├── include/
│   └── roguecore.h      # C 公開 API（Python ctypes / CLI 共用）
├── src/c/
│   ├── rng.c / rng.h    # 可重現亂數
│   ├── dungeon.c / .h   # 房間+走廊地城生成
│   ├── game.c           # 遊戲狀態、戰鬥、道具、視野
│   └── cli_main.c       # 純 C 終端前端
├── host/
│   ├── app.py           # Python Host 主程式
│   └── terminal_ui.py   # 清屏、ANSI、單鍵輸入、ASCII 面板
├── tests/
│   └── run_smoke.sh     # 煙霧測試
├── docs/
│   ├── PROPOSAL.md      # 提案紀錄
│   └── DEMO_CHECKLIST.md
├── Makefile
├── build.sh             # 建置腳本
├── run.sh               # 啟動 Python Host
└── README.md            # 本檔案
```

建置後根目錄會多出：

- `libroguecore.so`
- `rogue_cli`
- `src/c/*.o`（可 `make clean` 清除）

---

## 架構簡述

```
Python Host (host/app.py)
    │ ctypes 呼叫
    ▼
libroguecore.so
    ├── rng.c      — 可重現 RNG
    ├── dungeon.c  — 程序化地城
    └── game.c     — 規則、戰鬥、道具、多層流程
```

公開介面定義於 `include/roguecore.h`。Host 只負責顯示與輸入，不實作遊戲規則。

---

## 動態函式庫載入

- `rogue_cli` 以 `-Wl,-rpath,'$ORIGIN'` 連結，需與 `libroguecore.so` 位於**同一目錄**。
- `host/app.py` 以專案根目錄的絕對路徑載入 `libroguecore.so`，一般不需設定 `LD_LIBRARY_PATH`。

若載入失敗，可嘗試：

```bash
export LD_LIBRARY_PATH="$(pwd):${LD_LIBRARY_PATH:-}"
python3 host/app.py --seed 1
```

---

## 常見問題（Troubleshooting）

| 問題 | 解法 |
|------|------|
| `Permission denied` 執行腳本 | `chmod +x build.sh run.sh tests/run_smoke.sh` |
| 找不到 `libroguecore.so` | 先執行 `./build.sh` 或 `make` |
| `python3` 無法載入 `.so` | 設定 `LD_LIBRARY_PATH`（見上） |
| 畫面沒顏色 | 確認在互動終端機；或加 `--no-color` |
| 按鍵無反應 | 確認非管道模式；或改用 `--line-mode` |
| 右側 ASCII 被裁切 | 將終端機拉寬至 ≥ 115 字元 |
| macOS 建置 | 同樣需 `gcc`（Xcode CLT）與 `python3`；流程相同 |

---

## 繳交 zip 說明（繳交者參考）

### 應包含的內容

- 完整原始碼：`include/`、`src/c/`、`host/`、`tests/`、`docs/`
- `Makefile`、`build.sh`、`run.sh`、`README.md`
- **不要**依賴預先編譯的 `.so`（助教會自行 `make`）；但 zip 內含 `.so` 通常無妨

### 建議排除

- `.git/`、`__pycache__/`、`*.pyc`、`.DS_Store`
- 個人筆記、`.env`、大型暫存檔

### 建議打包流程

```bash
cd /path/to/parent
# 先在本機驗證
cd c-game && make test && cd ..

# 打包（範例：排除 git 與編譯暫存）
zip -r c-game.zip c-game \
  -x "c-game/.git/*" \
  -x "c-game/__pycache__/*" \
  -x "c-game/**/__pycache__/*" \
  -x "c-game/.DS_Store" \
  -x "c-game/src/c/*.o"

# 解壓到新目錄做一次乾淨驗收（強烈建議）
mkdir -p /tmp/zip-test && cd /tmp/zip-test
unzip /path/to/c-game.zip
cd c-game
chmod +x build.sh run.sh tests/run_smoke.sh
./build.sh
make test
./run.sh --seed 1
```

### 繳交前檢查清單

- [ ] 全新解壓後 `./build.sh` 成功
- [ ] `make test` 印出 `smoke OK`
- [ ] `./run.sh --seed 42` 可移動、可戰鬥、可撿道具
- [ ] README 指令與實際行為一致
- [ ] PDF 書面報告已另繳（zip 內可不放，依課程規定）

---

## 測試

```bash
make test
```

`tests/run_smoke.sh` 會：

1. `make all` 建置
2. 以 `printf 'q\n' | ./rogue_cli --seed 1` 做 CLI 冒煙測試
3. 執行 `python3 host/app.py --smoke` 驗證 ctypes API

---

## 誠信聲明

- **一魚兩吃**：無（未同時繳交其他課程之相同專題）。
- **LLM 使用**：開發與文件撰寫有使用 LLM 輔助；詳見書面報告之 LLM 聲明章節。
- **素材**：終端機 ASCII 字符畫，無外部圖片/音訊檔。

更多 demo 檢查項目見 [docs/DEMO_CHECKLIST.md](docs/DEMO_CHECKLIST.md)。
