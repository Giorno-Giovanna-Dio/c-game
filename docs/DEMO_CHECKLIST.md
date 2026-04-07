# 口頭報告 Demo 與繳交檢查清單（6/9 上台 / 6/12 截止）

## 上台前（硬體）

- [ ] 筆電可透過 **HDMI** 正常鏡像／延伸螢幕（解析度與縮放先試一次）。
- [ ] 專案可在**離線**下建置與執行（不依賴當場下載 pip 套件；本專題預設僅 `apt` 的 `build-essential`、`python3`）。
- [ ] 準備 **備援**：若現場執行失敗，備用短影片或另一台裝置可播放錄影 demo。

## Demo 腳本（建議 60～90 秒）

1. 開終端機，進入解壓後專案目錄。
2. 執行 `./build.sh`（或 `make`），畫面無錯誤。
3. 執行 `./run.sh --seed 42`，展示地圖與 `@` 移動。
4. （可選）執行 `./rogue_cli --seed 1` 強調「純 C 亦可玩」。
5. 一句話總結 C 與 Python 分工（對齊 `include/roguecore.h`）。

## 繳交 zip（6/12 前）

- [ ] 含**完整原始碼**（勿漏 `include/`、`src/c/`、`host/`）。
- [ ] 根目錄 **README.md** 可在 **Ubuntu 24.04** 照抄指令執行。
- [ ] 測試：`make test` 或手動 `./tests/run_smoke.sh` 通過後再壓縮。
- [ ] PDF 報告 ≤20 頁，含**分工**（一人組亦須填模組表）、**LLM 聲明**。

## 常見現場問題

- **找不到 .so**：確認 zip 解壓後 `libroguecore.so` 與 `rogue_cli` 在同一層目錄；先執行 `./build.sh`。
- **Permission denied**：`chmod +x build.sh run.sh tests/run_smoke.sh`。
