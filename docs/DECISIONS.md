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
