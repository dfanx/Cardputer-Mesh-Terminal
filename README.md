# Cardputer Mesh Terminal

Cardputer Mesh Terminal 是針對 **M5Stack Cardputer Adv + Cap LoRa-1262/GNSS** 的實驗性離網通訊韌體。第一版核心是選單式罐頭訊息、英數文字、GNSS Beacon 與隊友相對位置雷達，並內建最多 2.2 秒的 Codec2 PTT 語音及受控 Mesh 中繼。

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
2. 第一個畫面是「使用者代號」：輸入 1–8 個大寫英數字元（另可用 `-` `_`），Enter 確認。代號存在 NVS，下次開機會自動帶入，直接按 Enter 即可；`Esc` 可從 PIN 畫面退回修改。這個代號會放進 Beacon 的 callsign，隊友雷達與收到的訊息都用它辨識來源，未設定代號前不會進入群組配對。
3. 輸入相同的 4 位群組 PIN。PIN 會決定 group id、頻道與 sync word。PIN 以明碼顯示，方便當場核對按錯的位數——4 位 PIN 的安全目標本來就只有群組辨識與基本內容遮蔽，遮成 `*` 擋不住任何實際威脅。上次成功配對的 PIN 會像代號一樣自動帶入，直接按 Enter 即可回到同一群組；要換群組就 Backspace 清掉重打。
4. PIN 之後會出現「天線安全檢查」。`Y` 表示天線已鎖上並啟用發射；`N` 或 `Esc` 表示未安裝，此時只接收不發射。主畫面按 `A` 可隨時重新確認，不必重開機。

   發射預設是關閉的，每次開機都必須重新確認——SX1262 沒有 VSWR 或反射功率量測，天線是否安裝在硬體上無法偵測，只能由操作者判定。未確認時文字、罐頭訊息、語音、Beacon 與 Mesh 中繼全部不發射，主畫面下緣會顯示紅色警示列。接收不受限制，無天線接收不會損壞硬體。
5. `T` 開啟罐頭訊息選單；直接按 `↑`／`↓`（不必按 `Fn`）移動光棒、Enter 發送。數字 `1`–`9` 可直接快速發送對應項目。選單最後兩項固定是「分享目前位置」（立即送一次 Beacon，不等 10 分鐘排程，對方雷達會馬上更新）與自由輸入列（可輸入英數並以 Enter 發送）。
6. 主畫面按住 `Space` 錄音，放開或 2.2 秒自動停止；Codec2 700C 壓縮完成後排入發送，剛好是一個 LoRa 封包。
7. 收到訊息或語音時會發出提示音——文字是兩聲上行、語音是三聲上行——並跳出來訊畫面。**語音不會自動播放**：按 `Enter` 才播放，再按一次 `Enter` 標記已讀並前進到下一則；`Esc` 可先返回主畫面，未讀計數會留在下緣的紅色提示列。多則訊息必須逐則確認。
8. `↑` 開啟歷史紀錄。歷史保存在 flash，重開機不會消失；標示 `▶` 的是語音，選到後按 `Enter` 可重播。
9. 主畫面按 `-`／`=` 以 10% 為一級調整喇叭音量，範圍 0–100%，開機預設 100%（對講機用途漏聽比吵更容易出事，需要安靜再自己調低）。每次調整都會用新音量發一聲提示音，用聽的就知道現在多大聲。這兩顆是鍵盤上直接印著的鍵，不需要 `Shift`（`_` 與 `+` 同樣有效）。
10. 主畫面下緣固定顯示快速鍵提示。導覽鍵一律單按即可：`↑` 開歷史、`←`／`→` 切換隊友、`Esc` 返回。唯一的例外是自由訊息輸入頁，因為該頁的 `` ` `` 是可輸入字元，返回鍵是 `Fn` + `Esc`。

SD 卡的 `/message.txt` 可覆寫快捷短語：UTF-8、每行一則，最多 9 行，每行最多 80 bytes。解析失敗會回退內建短語。

### 歷史與語音的保存

訊息與語音存在 flash 上獨立的 4.9 MB `logfs` 分割區，與韌體、NVS 完全分開。可用空間不足時只會自動刪除**最舊的語音**，文字紀錄不會因為空間而被刪除。

分割表在此版本變更，升級必須整片抹除後燒錄；`deploy\flash-dev.cmd` 現在每次都會先完整抹除再寫入，不需要額外參數。若改用其他工具手動燒錄，務必先執行一次完整抹除，否則裝置上不存在 `logfs`，歷史會降級為只存在記憶體。

### 主畫面欄位對照

畫面只有 240×135，狀態列用單字母縮寫。對照如下：

| 位置 | 顯示 | 意義 |
| --- | --- | --- |
| 標題列左 | `ALEX` | 你的使用者代號 |
| 標題列左 | `G:1A2B` | **G**roup，PIN 推導出的群組 id（16 進位） |
| 標題列右 | `B85` | **B**attery，電池電量百分比。讀不到時顯示 `B--` |
| 標題列右 | `L50` | **L**evel，喇叭音量百分比，用 `-`／`=` 調整 |
| 右欄 | `G 7/11 Q0` | **G**NSS：定位使用的衛星數／視野內的衛星數；**Q**ueue，等待發射的封包數。綠＝已定位、黃＝收得到 NMEA 但還沒定位（室內的正常狀態）、紅＝收不到 NMEA，要檢查模組與接線 |
| 右欄 | `920.125M` | 目前工作頻率（MHz） |
| 右欄 | `R:Y S:Y V:Y` | **R**adio／**S**D／**V**oice 是否就緒 |
| 右欄 | `1/2 ALEX 5m` | 目前選取的隊友：序號／總數、代號、多久前收到他的封包。用 `←`／`→` 切換 |
| 右欄 | `NE 1.2km` | 該隊友的方位與距離。`對方無定位`＝他還沒定位；`本機無定位`＝這台還沒定位 |
| 左側方框 | 雷達點 | 以本機為中心，依相對方位/距離標出隊友；選取中的隊友是黃點，其餘是綠點，比例尺依最遠隊友自動縮放並標在右下角 |
| 下緣 | 快速鍵提示 | 有未讀時上排轉為紅色未讀列；否則發射停用時上排轉為紅色警示列 |

## Radio defaults

台灣開發 profile 使用 920.125–922.725 MHz、125 kHz、SF9、CR 4/7、14 dBm，並用 CAD + 50–200 ms 隨機退避。這是工程預設，不是法規或型式認證結論；更換地區、天線、功率或公開使用前，必須重新確認當地規範與硬體認證。

## Security reality

- Wire payload 使用 PIN-derived AES-128-GCM 做群組辨識、基本內容遮蔽與完整性；未通過 tag 的封包不顯示、不重組、不中繼。
- 4 位 PIN 只有 10,000 種組合，可被有意攔截者離線枚舉；本產品不定位為高機密通訊，也不提供成員個別撤銷或 forward secrecy。使用者代號與上次配對的 PIN 明碼存在 NVS，供下次開機自動帶入（ADR-010）；拿到裝置即可讀取，與畫面上原本就明碼顯示的 PIN 屬同一威脅模型。
- 精確位置與語音不寫入 Serial。

詳細架構見 [docs/architecture/FIRMWARE.md](docs/architecture/FIRMWARE.md)，產品邊界見 [docs/PROJECT_BRIEF.md](docs/PROJECT_BRIEF.md)。

## Voice and licensing boundary

- 預設 `cardputer-adv` build 已啟用 `sh123/esp32_codec2@1.0.7`；若音訊初始化失敗，文字、定位與罐頭訊息仍可使用。
- 2.2 秒 Codec2 700C 語音為 193 bytes（55 幀 × 28 bits 跨幀位元打包），加 3-byte 語音 schema 共 196 bytes，剛好放進單一 LoRa 封包。錄音上限由封包預算反推，不是獨立設定的數字。它是「錄完再傳」而非即時對講，空中時間與可懂度必須以實機量測。
- 此 Codec2 Arduino 套件標示為 GPL-3.0。若散布含它的韌體 binary，需先履行對應的 GPL 原始碼與授權義務；詳見 [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)。
