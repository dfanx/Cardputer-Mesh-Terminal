# Cardputer Mesh Terminal

Cardputer Mesh Terminal 是針對 **M5Stack Cardputer Adv + Cap LoRa-1262/GNSS** 的實驗性離網通訊韌體。第一版核心是選單式罐頭訊息、英數文字、GNSS Beacon/隊友雷達與軌跡，並內建最多三秒的 Codec2 PTT 語音及受控 Mesh 中繼。

> 這不是 PLB、衛星信使或認證救援設備。LoRa 鏈路可能因地形、天候、天線、干擾、法規與電池狀態失效；進入無訊號山區仍應攜帶正式安全裝備與完整離線計畫。

## Hardware

- M5Stack Cardputer Adv（ESP32-S3FN8、ES8311、240×135 LCD、鍵盤、microSD）
- M5Stack Cap LoRa-1262（SX1262 + ATGM336H/AT6668 GNSS）
- 正確頻段、已安裝的 RP-SMA 天線

**沒有安裝天線時不得讓 LoRa 發射。** M5Stack 官方警告無天線通電/使用可能永久損壞硬體。

## Build

```powershell
python -m venv .venv
.\.venv\Scripts\python -m pip install -r requirements-dev.txt
.\tools\pio.cmd run -e cardputer-adv
```

燒錄與監看是具硬體副作用的獨立操作，本 repo 不會自動執行：

```powershell
.\tools\pio.cmd run -e cardputer-adv -t upload
.\tools\pio.cmd device monitor -e cardputer-adv
```

## Tests

```powershell
.\tools\test-native.cmd
.\tools\pio.cmd check -e cardputer-adv
```

## Portable deployment bundle

Windows x64 部署 ZIP 內含合併韌體、Espressif 官方 esptool、COM 埠偵測/燒錄腳本、完整原始碼與架構文件；目標電腦不需安裝 Python 或 PlatformIO。使用方式見 `deploy/README-DEPLOY.md`，重建 release bundle 的腳本是 `tools/package-release.ps1`。

## First boot and controls

1. 確認 LoRa 天線已安裝，再開機。
2. 輸入相同的 4 位群組 PIN。PIN 會決定 group id、頻道與 sync word。
3. `T` 直接進入英數自由輸入模式，輸入完成後按 Enter 發送；`M` 開啟罐頭訊息選單，使用 `Fn` + `↑/↓` 選取、Enter 發送，數字 `1`–`9` 可快速發送對應項目。
4. 主畫面按住 `Space` 錄音，放開或三秒自動停止；Codec2 1300 壓縮完成後會分片排入發送。接收端只有在所有分片完整且語音格式驗證通過後才自動播放。
5. 主畫面按 `+`／`-` 以 10% 為一級調整喇叭音量，範圍 0–100%，開機預設 50%；Cardputer 的 `+` 需按 `Shift` + `=`。
6. `Fn` + `Esc` 返回主畫面；`Fn` + 方向鍵切換隊友/歷史資訊。

SD 卡的 `/message.txt` 可覆寫快捷短語：UTF-8、每行一則，最多 9 行，每行最多 80 bytes。解析失敗會回退內建短語。

## Radio defaults

台灣開發 profile 使用 920.125–922.725 MHz、125 kHz、SF9、CR 4/7、14 dBm，並用 CAD + 50–200 ms 隨機退避。這是工程預設，不是法規或型式認證結論；更換地區、天線、功率或公開使用前，必須重新確認當地規範與硬體認證。

## Security reality

- Wire payload 使用 PIN-derived AES-128-GCM 做群組辨識、基本內容遮蔽與完整性；未通過 tag 的封包不顯示、不重組、不中繼。
- 4 位 PIN 只有 10,000 種組合，可被有意攔截者離線枚舉；本產品不定位為高機密通訊，也不提供成員個別撤銷或 forward secrecy。
- 精確位置與語音不寫入 Serial；軌跡 CSV 若啟用則存在 SD，持卡者必須自行保護。

詳細架構見 [docs/architecture/FIRMWARE.md](docs/architecture/FIRMWARE.md)，產品邊界見 [docs/PROJECT_BRIEF.md](docs/PROJECT_BRIEF.md)。

## Voice and licensing boundary

- 預設 `cardputer-adv` build 已啟用 `sh123/esp32_codec2@1.0.7`；若音訊初始化失敗，文字、定位與罐頭訊息仍可使用。
- 三秒 Codec2 1300 語音約 525 bytes，另加 8-byte 語音 schema，通常形成三個 LoRa 封包。它是「錄完再傳」而非即時對講，空中時間與等待時間必須以實機量測。
- 此 Codec2 Arduino 套件標示為 GPL-3.0。若散布含它的韌體 binary，需先履行對應的 GPL 原始碼與授權義務；詳見 [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)。
