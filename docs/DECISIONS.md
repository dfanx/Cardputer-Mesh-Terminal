# Architecture decision records

## ADR-001 — PlatformIO + Arduino/M5Unified

- **Date**：2026-08-12
- **Status**：accepted
- **Context / constraints**：目標硬體是 Cardputer Adv；官方文件提供 Arduino、ESP-IDF 與 PlatformIO，第一版需快速整合鍵盤、LCD、ES8311 音訊與既有 SX1262 生態。
- **Options considered**：純 ESP-IDF；Arduino + PlatformIO；沿用 Meshtastic/MeshCore。
- **Decision**：PlatformIO 管理建置；ESP32 Arduino 為 runtime；M5Cardputer/M5Unified、RadioLib、TinyGPSPlus 與 Codec2 為硬體函式庫。核心協定保持標準 C++，可在 native 環境測試。
- **Rationale**：官方有相同硬體範例，整合風險最低；協定與硬體隔離可避免只能靠實機驗證。
- **Consequences**：需管理 Arduino core 與函式庫相容性；不取得 Meshtastic/MeshCore wire compatibility。
- **Revisit trigger**：Arduino 音訊/並行限制無法滿足，或產品需要 secure boot、flash encryption、RTOS 資源保證時評估 ESP-IDF。

## ADR-002 — 版本化 255-byte AEAD 分片封包

- **Date**：2026-08-12
- **Status**：accepted
- **Context / constraints**：SX1262 payload 上限 255 bytes；原 PRD struct 已接近上限且沒有版本、nonce、認證 tag 或語音分片能力。
- **Options considered**：直接傳 packed struct；CBOR/Protobuf；固定欄位 header + opaque payload。
- **Decision**：採固定 42-byte network-order header、最多 197-byte ciphertext 與 16-byte GCM tag；所有多位元組欄位明確序列化。大 payload 依 message/fragment id 重組。
- **Rationale**：固定成本與可預測 airtime，避免 ABI/endian/padding 問題，保留協定版本與分片空間。
- **Consequences**：最大 32 片；接收端需有超時與記憶體上限；公開格式改動必須升版。
- **Revisit trigger**：實測 airtime/丟包率顯示需要 FEC、ACK/ARQ 或更小 header。

## ADR-003 — PIN-derived AES-GCM 群組辨識

- **Date**：2026-08-12
- **Status**：accepted
- **Context / constraints**：PRD 要求 4 位 PIN；使用者確認資料不具高度機密，目標只需分辨群組、基本內容遮蔽與避免錯誤封包進入 UI。
- **Options considered**：只比對明文 group id + CRC；PIN-derived HMAC；PIN-derived AES-GCM；另行分發高熵密鑰。
- **Decision**：PIN 經 PBKDF2-HMAC-SHA256 產生 AES-128-GCM key、channel/group id；每片使用 96-bit nonce，header 為 AAD。不增加 SD group key 或複雜配對流程。
- **Rationale**：GCM 在 ESP32 由既有 mbedTLS 提供，額外產品複雜度低，能同時完成基本遮蔽、完整性與群組驗證；不把低熵 PIN 包裝成高安全。
- **Consequences**：擷取封包者可枚舉 10,000 組 PIN；不得承載高度機密或宣稱抗專業竊聽。安全性不是 v1 驗收門檻，格式健全與錯組隔離才是。
- **Revisit trigger**：未來要承載敏感位置、需要成員撤銷或明確對抗竊聽者。

## ADR-004 — 台灣保守 RF profile

- **Date**：2026-08-12
- **Status**：accepted
- **Context / constraints**：硬體支援約 868–923 MHz；預設使用地為台灣，NCC 對 920 MHz 頻段、功率及避免干擾有規範。
- **Options considered**：固定 868 MHz；完整 AS923；由 PIN 映射 920–923 MHz 子頻道。
- **Decision**：第一版預設在 920.125–922.725 MHz 的 14 個 125 kHz 子頻道中映射，SF9/CR4/7、14 dBm、CAD + 50–200 ms 退避。設定集中於 `RadioProfile`，未來可替換地區方案。
- **Rationale**：落在硬體與台灣物聯網頻段交集內，功率保守且保留 PRD 的自動對頻。
- **Consequences**：不是認證結論；不得據此推定任何地區皆合法；語音 airtime 仍需實測與節流。
- **Revisit trigger**：目標市場、認證報告、天線、法規或實測鏈路預算改變。

## ADR-005 — 預設納入三秒 Codec2 1300 PTT

- **Date**：2026-08-12
- **Status**：superseded by ADR-006
- **Context / constraints**：語音先前只有 adapter，預設 build 未綁定 codec；產品要求把語音實際做進韌體，同時限制為三秒並避免影響文字主流程。
- **Options considered**：不壓縮 PCM/ADPCM；Codec2 3200；Codec2 1300；保留未啟用 adapter。
- **Decision**：固定 `sh123/esp32_codec2@1.0.7`，只編入 Codec2 1300 mode、8 kHz mono。最多 75 個 40 ms frame；8-byte 版本化 metadata 加約 525-byte codec data，語音 TTL=1，完整重組與 schema 驗證後才播放。
- **Rationale**：原始 PCM/ADPCM 的空中資料量不適合目前 LoRa profile；1300 mode 可將三秒控制在約三個 LoRa 封包，且現有 ESP32 Arduino package 能重現建置。
- **Consequences**：這是錄完再傳，不是即時全雙工；音質、編解碼延遲、約五秒級理論空中時間及丟片體驗需實機驗證。套件標示 GPL-3.0，散布 binary 前需完成授權合規。
- **Revisit trigger**：實機可懂度不足、空中時間不符法規/操作需求，或產品不能接受 GPL 散布義務。

## ADR-006 — 語音改為單封包 Codec2 700C

- **Date**：2026-08-12
- **Status**：accepted
- **Context / constraints**：ADR-005 的 1300 mode 三秒約 525 bytes，跨三個 fragment。以現行 SF9/BW125/CR4/7 估算，光是送出就要約 4.2 s 空中時間，比錄音本身還久；語音 TTL=1 且無 ACK/重傳，三片全到才可播放，單包到達率 0.85 時整段只剩約 0.61。產品需求是對講機等級可懂度，不是高傳真。
- **Options considered**：維持 1300 三秒三片；1300 縮短到單片（約 1.2 s，太短）；700C 三秒兩片；700C 壓到單片；450 三秒單片。
- **Decision**：改用 Codec2 700C（28 bits/frame、40 ms、8 kHz），錄音上限由 fragment 預算反推為 55 幀 = 2.2 s。wire format 升到 v2：3-byte header（version、codec、frame_count）加跨幀位元打包的 193 bytes，合計 196 bytes，保證單一 fragment。`kMaxVoiceFrames` 於 `message_codec.h` 由 `kMaxFragmentPayloadBytes` 推導並以 static_assert 鎖住。
- **Rationale**：700C 是 FreeDV 700D 使用的位元率，屬「吵雜環境仍可辨識」的設計點；450 雖能塞進三秒，但 newamp2 的音高量化誤差對中文聲調傷害明顯，不值得為省 90 bytes 賭可懂度。位元打包是必要的：700C 每幀 28 bits 若沿用位元組對齊會浪費 14%。移除可由 codec id 推得的 metadata 欄位，換得多一幀並減少受攻擊者控制的輸入。
- **Consequences**：單段語音從 3 s 縮短為 2.2 s，且與 v1 不相容（codec id、版本、打包方式全變）；混版部署會互相判為格式錯誤而捨棄。音質低於 1300 mode，中文可懂度必須實機驗證。仍是錄完再傳，不是即時全雙工。GPL-3.0 散布義務不變。
- **Revisit trigger**：實機中文可懂度不足（則往 1300 單片 1.2 s 或雙片評估）；改用其他 SF/BW 使 airtime 不再是瓶頸；或加入 FEC/ARQ 後多片成本可接受。

## ADR-007 — Beacon v2：沒有 GNSS fix 也要廣播身分

- **Date**：2026-08-15
- **Status**：accepted
- **Context / constraints**：v1 的 `encodeBeaconMessage()` 在 `point.valid == false` 時直接失敗，`sendBeacon()` 也在沒有 fix 時提前 return，而隊友名單是**只**由 Beacon 建立的。實機結果是兩台已配對、彼此收得到封包的機器，主畫面永遠顯示「尚無隊友」——室內、峽谷、剛開機冷啟動這些最需要確認「隊友還在不在」的情境，正好都是沒有 fix 的情境。FIRMWARE.md 原本就寫「Beacon 顯示 stale/no-fix」，實作沒有做到。
- **Options considered**：沒有 fix 時送 0/0 座標（會被誤讀成幾內亞灣外海的真實位置，且與「不廣播虛假座標」的失效行為衝突）；只靠文字/語音封包建立隊友名單（安靜的隊友就消失了）；Beacon 加 has_fix 旗標。
- **Decision**：Beacon 升到 v2：`version(1), flags(1), lat_e7(4), lon_e7(4), alt(2), battery(1), callsign_len(1), callsign`，`flags` bit0 = has_fix。沒有 fix 時座標欄位必須為零，解碼端看到「no-fix 卻帶非零座標」或未定義的 flag bit 一律拒收。另外，任何通過認證的封包（文字、語音、Beacon）都會建立/更新隊友紀錄，名單不再單靠 Beacon；收到沒見過的隊友時把自己的下一次 Beacon 提前 4–10 s，雙方位置不必等滿十分鐘。
- **Rationale**：Beacon 真正在回答的是「我在這個群組、我還活著、電量多少」，座標只是其中一欄。把整則訊息綁在最容易失效的那一欄上，等於讓最不可靠的感測器決定整個隊友名單能不能運作。旗標化讓「沒有位置」成為可表達的狀態，而不是靜默失敗。
- **Consequences**：與 v1 不相容，混版部署會互相判為格式錯誤而捨棄，兩台都必須重新燒錄。Beacon 從 13+n 變成 14+n bytes。隊友可能在畫面上只有代號與電量而沒有方位，UI 必須分辨「對方無定位」與「本機無定位」。
- **Revisit trigger**：需要在 Beacon 帶更多狀態（求救旗標、行進速度、電量趨勢）時一併重排欄位。

## ADR-008 — logfs 分割區保存訊息與語音歷史

- **Date**：2026-08-15
- **Status**：accepted
- **Context / constraints**：歷史只存在 RAM 的 32 筆 ring，語音收到即播、播完即丟，重開機全部消失，收到的語音無法重聽。登山情境下「剛剛那則語音說什麼」與「今天講過哪些事」都要能回頭查。SD 卡已用於軌跡，但 SD 可能沒插、可能被拔走，而軌跡與 GPS 紀錄不能被訊息歷史擠掉。
- **Options considered**：全部寫 SD（沒卡就沒歷史，且與軌跡搶空間）；NVS（設計給小型鍵值，不適合語音）；沿用 `default_8MB.csv` 的 960 KB SPIFFS 分割（要與 OTA 共存，空間也偏小）；自訂分割表切出專用區。
- **Decision**：改用 `partitions/cmt-adv-8mb.csv`：拿掉不使用的 OTA 第二 app 槽，切出 4.9 MB 的 `logfs`（LittleFS）。文字紀錄寫 `/msg.log`（每行一筆的分隔欄位格式），語音寫 `/v/<clip_id>.c2`。可用空間低於 128 KB 時**只**淘汰編號最舊的語音，文字紀錄永遠不因空間不足被刪；文字另有自己的 384 KB 上限（約三千餘筆）才輪替。SD 上的軌跡與 `message.txt` 完全不受影響。
- **Rationale**：文字是「講過什麼」的證據，一筆約 80 bytes，保存整趟行程的成本可以忽略；語音一段 193 bytes 但可重錄可重問，是唯一適合當作快取犧牲的資料。把兩者放在專屬分割區，才能保證訊息歷史不會吃掉軌跡或韌體空間。
- **Consequences**：分割表改變，燒錄必須整片更新（`-EraseAll` 或完整 flash），舊裝置升級會清掉既有 NVS 以外的內容；放棄 OTA 能力（本專案本來就只用 USB 燒錄）。app 分割由 3.3 MB 縮到 3 MB，目前用量 1.19 MB，仍有兩倍以上餘裕。
- **Revisit trigger**：需要 OTA；語音長度或保存策略改變使 4.9 MB 不足；或改用外部 flash/SD 作為主要歷史儲存。

## ADR-009 — 來訊改為提示音加逐則確認，語音不自動播放

- **Date**：2026-08-15
- **Status**：accepted
- **Context / constraints**：v1 收到文字只是靜靜寫進歷史，畫面與聲音都沒有任何變化，使用者必須自己去翻歷史才知道有新訊息；語音則相反，收到就立刻播放。兩者都不符合實際使用：背包上的機器沒人在盯著，訊息會被錯過；而語音在沒人注意時自動播完就沒了。
- **Options considered**：只加提示音（訊息仍要自己去找）；訊息與語音都自動顯示/播放（會蓋掉使用者當下的操作，語音也仍可能沒人聽到）；未讀佇列加確認後播放。
- **Decision**：文字與語音進入未讀佇列，收到時發出可分辨的提示音（文字兩聲上行、語音三聲上行）。使用者停在主畫面時直接跳出來訊畫面；正在輸入或操作選單時不打斷，改以主畫面紅色未讀列提示。語音**不自動播放**：第一次 Enter 才播，再按 Enter 標記已讀並前進到下一則，多則訊息必須逐則確認。所有來訊都同時寫入 logfs，語音之後可在歷史紀錄以 Enter 重播。
- **Rationale**：提示音負責「叫人來看」，確認鍵負責「人真的在聽了才播」。自動播放把一次性的音訊押在使用者剛好在場，逐則確認則讓每一則都至少被看到一次。不打斷輸入中的操作，是因為誤觸送出錯誤訊息的代價高於延遲數秒看到來訊。
- **Consequences**：語音從「收到即聽到」變成「收到聽到提示音，按鍵才聽到內容」，多了一個操作步驟。未讀佇列上限 16 則，超過會丟棄最舊的未讀（紀錄仍在歷史裡）。喇叭在提示音與播放之外必須保持關閉，否則提示音頻繁反而更耗電。
- **Revisit trigger**：實機顯示提示音在背包中聽不見（則評估震動或更長的提示序列）；或使用者回報逐則確認在大量訊息時太繁瑣。
