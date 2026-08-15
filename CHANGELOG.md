# Changelog

## [Unreleased]

### Added

- Cardputer Adv + Cap LoRa-1262/GNSS 第一版 PlatformIO 韌體。
- 4 位 PIN 群組/頻道映射與 PIN-derived AES-GCM 群組辨識。
- 255-byte 版本化分片封包、認證後去重、TTL 兩跳 Mesh 與 CAD 退避。
- 繁中罐頭訊息選單、數字快速發送、英數自由輸入與 SD `message.txt` 自訂。
- GNSS Beacon、SD 軌跡、螢幕軌跡圖、隊友相對距離/方位/高差與漏訊提示。
- 預設啟用 2.2 秒 Space PTT、Codec2 700C 壓縮、版本化語音 schema 與自動播放。錄音上限由單一 fragment 的 197-byte 預算反推為 55 幀，語音因此不分片。
- 主畫面 `-`／`=` 喇叭音量控制，0–100%、每次 10%、預設 50%。
- 開機第一個畫面改為「使用者代號」輸入（1–8 個大寫英數與 `-` `_`），存於 NVS、下次開機自動帶入，並取代 node id 衍生名稱成為 Beacon callsign 與隊友顯示名稱。
- 主畫面下緣固定顯示兩行快速鍵提示；發射停用時上排轉為紅色警示，下排仍保留快速鍵。
- Native core regression tests 與 Cardputer Adv build workflow。
- 天線安全閘：配對後強制天線確認，未確認時停用文字、罐頭訊息、語音、Beacon 與 Mesh 中繼的所有發射，接收不受影響。主畫面 `A` 可重新確認，並顯示紅色警示列與 LoRa 模組偵測結果。
- `deploy\flash-dev.cmd`：從原始碼建置並燒錄的開發用入口，自動選取 Espressif COM 埠。

- 來訊提示：收到文字或語音會發出可分辨的提示音（文字兩聲上行、語音三聲上行），停在主畫面時直接跳出來訊畫面，主畫面下緣顯示紅色未讀計數。多則訊息必須逐則按 Enter 確認（ADR-009）。
- 訊息與語音歷史持久化：新增 4.9 MB 的 `logfs` flash 分割（`partitions/cmt-adv-8mb.csv`），文字寫 `/msg.log`、語音寫 `/v/<clip>.c2`，重開機後歷史仍在，語音可在歷史紀錄按 Enter 重播（ADR-008）。
- GNSS 診斷：主畫面右欄改為 `G <定位數>/<可見數>`，並以綠／黃／紅區分「已定位」「收到 NMEA 但無 fix」「收不到 NMEA」。沒有隊友時直接顯示 GNSS 狀態。
- 罐頭訊息選單新增「分享目前位置」項目：立即送出一次 Beacon（不等 10 分鐘排程），對方的雷達會馬上更新這筆位置；沒有 fix 時仍會分享身分與電量。
- 群組 PIN 隨使用者代號一併存進 NVS，下次開機自動帶入配對畫面，直接 Enter 即可回到同一群組（ADR-010）。
- 歷史畫面新增 `D` 清除全部：先進入獨立的 Y/N 確認狀態（顯示「將刪除 N 則訊息與全部語音音檔，此操作無法復原」），確定後同時清空 `logfs` 的 `/msg.log`、全部 `/v/*.c2` 語音音檔與畫面上的歷史清單；取消或按錯鍵都不會誤觸發。

### Changed

- **Beacon 升到 v2，與 v1 不相容，兩台裝置都必須重新燒錄**（ADR-007）。新增 `flags` 欄位（bit0 = has_fix），沒有 GNSS fix 時照樣廣播身分與電量，座標欄位為零。
- 隊友名單改由任何通過認證的封包建立，不再只靠 Beacon；收到沒見過的隊友時把自己的下一次 Beacon 提前 4–10 s，位置不必等滿十分鐘才對上。主畫面顯示隊友代號、序號與「多久前聽到」，並分辨「對方無定位」與「本機無定位」。
- 收到語音改為**不自動播放**：先存檔並發提示音，使用者按 Enter 才播放，之後可從歷史重播（ADR-009）。
- 分割表由 `default_8MB.csv` 改為 `partitions/cmt-adv-8mb.csv`，移除未使用的 OTA 第二 app 槽；app 分割由 3.3 MB 縮為 3 MB（目前用量 1.19 MB）。
- `deploy\flash-dev.cmd`／`flash-dev.ps1` 移除 `-EraseAll` 選用旗標，改為每次都先完整抹除再寫入。分割表已綁定固定 offset 的 `logfs`，不先抹除可能在分割表變動後留下與新版面不一致的殘留內容；完整抹除會清掉 NVS，使用者代號與配對狀態需在下次開機重新輸入。
- 選單、歷史與主畫面的方向鍵與 `Esc` 改為單按即可，不再需要 `Fn` 組合鍵。自由訊息輸入頁維持 `Fn` + `Esc`，因為該頁的 `` ` `` 是可輸入字元。
- 群組 PIN 輸入改為明碼顯示，不再遮成 `*`。4 位 PIN 的安全目標只有群組辨識與基本內容遮蔽，遮蔽擋不住實際威脅，卻讓使用者看不出按錯哪一位。
- README 新增「主畫面欄位對照」表，說明標題列 `B`＝電量、`L`＝音量、`G`＝群組 id，以及右欄 `R`/`S`/`V`/`Q` 等縮寫。
- **主畫面拿掉全軌跡折線圖，SD 不再寫入 `/tracks/*.csv`，改為以本機為中心的隊友相對位置雷達**（ADR-011）：比例尺依目前最遠的隊友自動縮放，選取中的隊友是黃點、其餘綠點，沒有可用相對位置時顯示提示文字。消費級 GNSS 原地飄移在 145×77 像素的折線圖上沒有實用價值。隊友之間原有的 10 分鐘互相 Beacon 更新頻率不變。
- 喇叭音量預設由 50% 提高到 100%（對講機用途漏聽比吵更容易出事）；調整音量時會用調整後的音量播一聲提示音。

### Fixed

- 主畫面永遠顯示「尚無隊友」，即使兩台已配對且互相收得到封包。`sendBeacon()` 在沒有 GNSS fix 時直接放棄，而隊友名單只由 Beacon 建立——室內、峽谷與冷啟動這些最需要確認隊友在不在的情境，正好都沒有 fix。改為沒有 fix 也廣播帶旗標的 Beacon，並讓任何通過認證的封包都能建立隊友。
- 收到訊息完全沒有任何提示，必須自己去翻歷史紀錄才會發現。改為提示音加畫面來訊提示與未讀計數。
- 喇叭播過一次之後持續開啟並發出電流聲。`M5.begin()` 的 `internal_spk` 會讓功放常駐，而先前只有語音播放結束的那一條路徑會 `Speaker.end()`。改為開機即關閉功放，統一由 `speakerOn()`／`speakerOff()` 管理生命週期，輸出停止 300 ms 後自動關閉。
- 收到的語音無法再次播放：播完即丟，沒有任何保存。改為先寫入 `logfs` 再等使用者確認播放，歷史紀錄中以 `▶` 標示可重播。
- 歷史紀錄在重開機後全部消失，且只保留 RAM 中的 32 筆。
- 歷史與來訊畫面以位元組截斷中文字串，會截在 UTF-8 多位元組序列中間而顯示亂碼方塊。改為以字元邊界與顯示寬度截斷／換行。
- 語音即使把喇叭音量調到最大仍明顯偏小聲。M5Unified 的 master volume 在 100% 時已經打滿（255/255），瓶頸在 Codec2 700C 解碼後的 PCM 本身振幅偏低；改為在解碼後對 PCM 做 4 倍（+12 dB）數位增益並硬限幅，只影響語音播放路徑，不影響提示音。
- GNSS 右欄的「已用衛星數」可能大於「可見衛星數」（例如 `G 11/6`），因為「可見」只加總單一 talker（如 ATGM336H 這類 GPS+北斗雙模模組只讀了 GPGSV，用的卻是跨系統合計的 GGA 已用數）。改為加總 GP/BD/GB/GL/GA/GQ 各 talker 的 GSV 可見數。
- **喇叭用過一次語音功能後持續開啟、發出電流聲，即使先前已修過「播完自動關閉」的 I2S 層邏輯**。根因在第三方函式庫：M5Unified 0.2.19 對 Cardputer Adv 的喇叭 enable callback 在停用時送出空的暫存器寫入序列，`M5Cardputer.Speaker.end()` 因此只停掉 I2S 任務，從未真的把 ES8311 audio codec 斷電。改為在 `speakerOff()` 額外直接對 ES8311（I2C `0x18`）補發斷電暫存器（`0x0D=0xFC` 關類比電路、`0x00=0x00` CSM power down），鏡射同一份原始碼中其他板子既有的斷電序列。
- 畫面持續閃爍。每次更新都直接對面板 `fillScreen()` 再逐項重畫，而且主畫面每 500 ms 無條件重畫一次，肉眼會看到「整頁變黑 → 內容浮現」。改為畫進 240x135x16bpp 的離螢幕緩衝後一次 `pushSprite()`，並讓週期重畫降到 1 s、只在內容變更（`dirty_`）時才提前重畫、最短間隔 50 ms。緩衝在音訊初始化之後才配置，記憶體不足時退回直接繪製，不影響 Codec2。
- 開機即 `loopTask` stack overflow 無限重啟。Codec2 MODE_1300 在 loopTask 上的峰值堆疊用量實測為 18,184 bytes，超過 Arduino 預設的 8 KB，`codec2_create()` 當場觸發 panic。改以 `SET_LOOP_TASK_STACK_SIZE(32768)` 配置。
- `tools\pio.cmd` 在長路徑下會讓編譯命令列超過 Windows 32767 字元上限，造成 `xtensa-esp32s3-elf-g++: error: CreateProcess: No such file or directory`。核心目錄解析改為：顯式 `PLATFORMIO_CORE_DIR` 優先，其次既存的 repo-local `.platformio`，否則使用 PlatformIO 預設位置。
- `tools\test-native.cmd` 在未登記 `VC.Tools.x86.x64` component id 的 Visual Studio 安裝上會誤判找不到 MSVC，改以 `vcvars64.bat` 是否存在為判準。
- 從 repo 根目錄以外的位置執行（例如對 `deploy\flash-dev.cmd` 按「以系統管理員身分執行」，工作目錄會是 `C:\Windows\System32`）時，PlatformIO 以當前目錄尋找 `platformio.ini` 而失敗，出現 `NotPlatformIOProjectError`。`tools\pio.cmd` 改為執行前先 `cd /d` 到 repo 根目錄；`setlocal` 會在腳本結束時還原呼叫端的工作目錄。

### Verification status

- **最新一輪修正（ES8311 手動斷電、歷史清除確認）僅通過 native core tests（10/10）與 `cardputer-adv` clean build，尚未在實機驗證。** 這輪的 ES8311 修正是對前一輪「喇叭底噪消失」問題的追蹤：前一輪的 I2S 層 `speakerOn()`/`speakerOff()` 邏輯本身沒有問題，實機仍回報底噪持續，追查後發現根因在 M5Unified 函式庫本身的 disable callback 是 no-op，這次改為直接補發 ES8311 斷電暫存器；需要重新在實機確認底噪是否真的消失。歷史清除需確認：確認畫面不會被誤觸發、Y 之後 `/msg.log` 與 `/v/*.c2` 確實清空、清空後重開機歷史仍是空的。
- 上一輪修正（PIN 持久化、罐頭訊息分享位置、雷達視圖、音量預設/提示音、語音增益、GNSS 多 talker 加總）同樣僅通過 build/test，尚未在實機驗證。需要兩台裝置實測的項目：重開機後 PIN 是否正確帶入、分享位置後對方雷達是否即時更新、雷達點位置與比例尺是否合理、100% 音量下語音增益是否明顯改善且不失真、GNSS 已用/可見數是否不再逆反。
- 再前一輪修正（Beacon v2、來訊提示、logfs 歷史、GNSS 診斷）已燒錄到一台裝置：隊友名單成功出現對方（兩機互見），雷達相對方位/距離計算正確（同機測試顯示 `S 0.0km`，兩機緊鄰、方位角落在南向誤差範圍內）。仍需兩台裝置實測：提示音與逐則確認、語音重播。
- 分割表已變更，升級必須整片抹除後燒錄；`deploy\flash-dev.cmd`／`flash-dev.ps1` 已改為每次都先完整抹除再寫入（不再是 `-EraseAll` 選用旗標），避免舊分割表殘留造成 `logfs` 不存在或內容不一致。
- 已在單機實測：完整抹除後建置、燒錄（COM3、hash verified）與開機流程；離螢幕緩衝配置成功，畫面閃爍經實機確認已消除。
- **語音錄音鏈路已在實機驗證**：`codec2_create(CODEC2_MODE_700C)` 配置成功（狀態列 `V:Y`），未重演 MODE_1300 的 loopTask 堆疊溢位；連續兩次 PTT 分別擷取 46 與 55 幀（後者錄滿 2.2 秒緩衝上限），`M5Cardputer.Mic.record()` 失敗 0 次，麥克風／喇叭資源切換正常。
- 語音編碼與 wire format 已用真實錄音（2.82 秒人聲）在主機端驗證：走過 `encodeVoiceMessage()`／`decodeVoiceMessage()` 後位元流完全一致，2.2 秒段落為 196 bytes、單一 fragment。
- **未驗證：所有發射與接收鏈路**（文字、罐頭訊息、Beacon、語音、Mesh 中繼）。測試機沒有 LoRa cap 模組，`radio_.begin()` 失敗、狀態列顯示 `R:N`，`sendPayload()` 的前置檢查一律擋下。GNSS 同樣來自該 cap，`GPS 0`。
- 語音**播放**路徑（`codec2_decode` 加 `Speaker.playRaw()`）未在實機執行過：它只在收到語音封包時觸發，沒有 LoRa 就沒有觸發來源，也沒有本機回放路徑。
- `pio check` 通過（專案原始碼 0 high／0 medium）。native 測試 9/9 通過：開發機的 Visual Studio 缺 C++ 工作負載，改由 `tools/test-native.cmd` 的 WSL g++ 路徑執行。

### Known limitations

- 尚未以兩台/三台實機驗證 RF、GNSS、SD、音訊與 relay journey。
- 天線是否安裝無法由硬體確認。SX1262 沒有 VSWR 或反射功率量測，天線安全閘依賴操作者判定；自動偵測到的只有 LoRa cap 模組是否在 I2C/SPI 上回應。
- 4 位 PIN 可被離線枚舉，只用於群組辨識與基本內容遮蔽。
- 語音音質與實際空中時間尚未經兩台實機驗證。700C 的中文可懂度是這次改用低位元率的核心風險，主機端試聽只是音質上限——實機用的是 Cardputer Adv 的麥克風，表現會更差。不滿意時的退路見 ADR-006 的 revisit trigger。
