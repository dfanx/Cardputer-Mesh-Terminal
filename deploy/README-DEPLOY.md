# Cardputer Mesh Terminal v1.1.0-rc1 部署指南

## 最快燒錄方式（Windows 10/11 x64）

1. 解壓縮整個 ZIP；不要直接在壓縮檔內執行。
2. 以 USB 資料線連接 Cardputer Adv。
3. 執行 `deploy\flash-windows.cmd`。工具會優先自動選擇 Espressif/Cardputer 的 COM 埠；無法唯一辨識時才會要求輸入。
4. 若裝置無法進入下載模式，按住 `G0`、短按 `Reset`，放開 `G0` 後重試。
5. 燒錄完成後，先安裝正確頻段的 LoRa 天線，再輸入四位群組 PIN。

進階用法：

```powershell
# 明確指定序列埠
.\deploy\flash-windows.ps1 -Port COM5

# 清除全部 Flash/NVS 後安裝；會移除舊裝置識別與狀態
.\deploy\flash-windows.ps1 -Port COM5 -EraseAll
```

部署包內含 Espressif 官方 `esptool` Windows x64 執行檔，所以目標電腦不需要 Python、PlatformIO 或網路。燒錄腳本只支援 Windows x64；macOS/Linux 可依 `firmware/FLASH-COMMAND.txt` 使用官方 esptool。

## 操作摘要

- 輸入相同的四位 PIN 加入同一群組。
- `T`：直接開啟英數自由輸入，Enter 發送。
- `M`：開啟罐頭訊息選單；數字 `1`–`9` 快速發送。
- `Space`：按住錄音，放開或三秒後壓縮並發送。
- `+`／`-`：主畫面調整喇叭音量，每次 10%。`+` 是 `Shift` + `=`。
- `Fn` + 方向鍵：切換隊友或歷史；`Fn` + `Esc` 返回。
- SD 卡可放置 `/message.txt`，UTF-8、每行一則、最多九則。

## 專案架構閱讀順序

1. `README.md`：產品定位、硬體、操作與安全邊界。
2. `docs/PROJECT_BRIEF.md`：範圍、關鍵旅程與成功條件。
3. `docs/architecture/FIRMWARE.md`：分層、wire format、Mesh 與語音 schema。
4. `docs/DECISIONS.md`：主要技術決策及取捨。
5. `source/include`、`source/src`：核心協定、平台 adapter、UI 與 application state machine。
6. `source/test`：可在一般 Windows 開發機執行的核心回歸測試。

## 誠實邊界

這是可建置且通過軟體測試的 RC 韌體，不是實機驗收或救援認證版本。封包、Codec2、分片與靜態建置已驗證；麥克風增益、喇叭音量、heap 最低水位、兩機語音品質、三機中繼、GNSS/SD 與山區距離仍需目標硬體測試。

四位 PIN 只有群組辨識與基本內容遮蔽能力，不應承載高度機密。公開散布 binary 前也必須處理 `THIRD_PARTY_NOTICES.md` 所列 GPL-3.0 義務及本專案授權決策。
