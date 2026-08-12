# Changelog

## [Unreleased]

### Added

- Cardputer Adv + Cap LoRa-1262/GNSS 第一版 PlatformIO 韌體。
- 4 位 PIN 群組/頻道映射與 PIN-derived AES-GCM 群組辨識。
- 255-byte 版本化分片封包、認證後去重、TTL 兩跳 Mesh 與 CAD 退避。
- 繁中罐頭訊息選單、數字快速發送、英數自由輸入與 SD `message.txt` 自訂。
- GNSS Beacon、SD 軌跡、螢幕軌跡圖、隊友相對距離/方位/高差與漏訊提示。
- 預設啟用 3 秒 Space PTT、Codec2 1300 壓縮、版本化語音 schema、分片發送、完整重組與自動播放。
- 主畫面 `-`／`=` 喇叭音量控制，0–100%、每次 10%、預設 50%。
- 開機第一個畫面改為「使用者代號」輸入（1–8 個大寫英數與 `-` `_`），存於 NVS、下次開機自動帶入，並取代 node id 衍生名稱成為 Beacon callsign 與隊友顯示名稱。
- 主畫面下緣固定顯示兩行快速鍵提示；發射停用時上排轉為紅色警示，下排仍保留快速鍵。
- Native core regression tests 與 Cardputer Adv build workflow。
- 天線安全閘：配對後強制天線確認，未確認時停用文字、罐頭訊息、語音、Beacon 與 Mesh 中繼的所有發射，接收不受影響。主畫面 `A` 可重新確認，並顯示紅色警示列與 LoRa 模組偵測結果。
- `deploy\flash-dev.cmd`：從原始碼建置並燒錄的開發用入口，自動選取 Espressif COM 埠。

### Changed

- 選單、歷史與主畫面的方向鍵與 `Esc` 改為單按即可，不再需要 `Fn` 組合鍵。自由訊息輸入頁維持 `Fn` + `Esc`，因為該頁的 `` ` `` 是可輸入字元。
- 群組 PIN 輸入改為明碼顯示，不再遮成 `*`。4 位 PIN 的安全目標只有群組辨識與基本內容遮蔽，遮蔽擋不住實際威脅，卻讓使用者看不出按錯哪一位。
- README 新增「主畫面欄位對照」表，說明標題列 `B`＝電量、`L`＝音量、`G`＝群組 id，以及右欄 `R`/`S`/`V`/`Q` 等縮寫。

### Fixed

- 畫面持續閃爍。每次更新都直接對面板 `fillScreen()` 再逐項重畫，而且主畫面每 500 ms 無條件重畫一次，肉眼會看到「整頁變黑 → 內容浮現」。改為畫進 240x135x16bpp 的離螢幕緩衝後一次 `pushSprite()`，並讓週期重畫降到 1 s、只在內容變更（`dirty_`）時才提前重畫、最短間隔 50 ms。緩衝在音訊初始化之後才配置，記憶體不足時退回直接繪製，不影響 Codec2。
- 開機即 `loopTask` stack overflow 無限重啟。Codec2 MODE_1300 在 loopTask 上的峰值堆疊用量實測為 18,184 bytes，超過 Arduino 預設的 8 KB，`codec2_create()` 當場觸發 panic。改以 `SET_LOOP_TASK_STACK_SIZE(32768)` 配置。
- `tools\pio.cmd` 在長路徑下會讓編譯命令列超過 Windows 32767 字元上限，造成 `xtensa-esp32s3-elf-g++: error: CreateProcess: No such file or directory`。核心目錄解析改為：顯式 `PLATFORMIO_CORE_DIR` 優先，其次既存的 repo-local `.platformio`，否則使用 PlatformIO 預設位置。
- `tools\test-native.cmd` 在未登記 `VC.Tools.x86.x64` component id 的 Visual Studio 安裝上會誤判找不到 MSVC，改以 `vcvars64.bat` 是否存在為判準。
- 從 repo 根目錄以外的位置執行（例如對 `deploy\flash-dev.cmd` 按「以系統管理員身分執行」，工作目錄會是 `C:\Windows\System32`）時，PlatformIO 以當前目錄尋找 `platformio.ini` 而失敗，出現 `NotPlatformIOProjectError`。`tools\pio.cmd` 改為執行前先 `cd /d` 到 repo 根目錄；`setlocal` 會在腳本結束時還原呼叫端的工作目錄。

### Known limitations

- 尚未以兩台/三台實機驗證 RF、GNSS、SD、音訊與 relay journey。
- 天線是否安裝無法由硬體確認。SX1262 沒有 VSWR 或反射功率量測，天線安全閘依賴操作者判定；自動偵測到的只有 LoRa cap 模組是否在 I2C/SPI 上回應。
- 4 位 PIN 可被離線枚舉，只用於群組辨識與基本內容遮蔽。
- 語音音質、實際空中時間、丟片體驗與錄放音切換尚未經兩台實機驗證。
