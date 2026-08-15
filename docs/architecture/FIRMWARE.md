# Firmware architecture v1

## Runtime layers

```text
UI / Application state machine
        |
Use cases: pairing, text, beacon, PTT, history, track/radar
        |
Core: channel plan, message codec, fragment/reassembly,
      wire protocol, AEAD, sequence gaps, mesh routing, geo math
        |
Ports: radio, GNSS, audio, storage, display/input, clock/random
        |
Cardputer Adv adapters: M5Cardputer/M5Unified, RadioLib SX1262,
                        TinyGPSPlus, SD, ESP32 mbedTLS/Preferences
```

核心層不得 include Arduino/M5/RadioLib，讓封包邏輯可用 native tests 驗證。平台層只負責資源生命週期、驅動錯誤與事件轉換；產品狀態由 `MeshTerminalApp` 統一協調。

## Wire format

所有整數使用 big-endian。Header 是 AES-GCM AAD；payload 加密，tag 為 16 bytes。

| Offset | Bytes | Field |
|---:|---:|---|
| 0 | 2 | Magic `CM` |
| 2 | 1 | Protocol version |
| 3 | 1 | Message type |
| 4 | 1 | Flags |
| 5 | 1 | Remaining TTL |
| 6 | 1 | Hop count |
| 7 | 1 | Fragment index |
| 8 | 1 | Fragment count |
| 9 | 1 | Ciphertext length |
| 10 | 4 | Group id |
| 14 | 4 | Source node id |
| 18 | 4 | Message id |
| 22 | 4 | Daily sequence |
| 26 | 4 | UTC Unix seconds, 0 if unavailable |
| 30 | 12 | GCM nonce |
| 42 | 0..197 | Ciphertext |
| tail | 16 | GCM tag |

LoRa PHY CRC 保留；應用層不再增加無法抵抗惡意修改的 CRC16。Tag 驗證失敗必須靜默丟棄並只增加匿名 counter。

## Message payloads

- Text：UTF-8/ASCII bytes，v1 UI 自由輸入限制 160 bytes；快捷中文由預編碼 UTF-8 發送。
- Beacon v2（ADR-007）：`version(1), flags(1), lat/lon E7(4+4), signed altitude meters(2), battery(1), callsign_len(1), callsign(≤12)`。`flags` bit0 = has_fix，其餘 bit 保留且必須為零。沒有 fix 時照樣廣播，座標欄位必須全零；解碼端遇到「no-fix 帶非零座標」或未定義 flag 一律拒收。
- Voice：8-byte schema 為 `version(1), codec(1), sample_rate(2), samples_per_frame(2), bytes_per_frame(1), frame_count(1)`，後接 Codec2 1300 frames。固定 8 kHz、320 samples/7 bytes per frame、最多 75 frames；完整三秒為 533 bytes，通常切成三片。只有所有片在 timeout 內到齊且 metadata/長度驗證通過才解碼播放。

## Mesh rules

封包 key 是 `(source_id, message_id, fragment_index)`。接收順序：基本格式/group → AEAD → authenticated duplicate cache/relay decision → local reassembly/delivery。中繼節點需使用群組金鑰重新認證修改後的 TTL/hop header；語音初始 TTL 1，不再二次中繼。每個 fragment 最多中繼一次，cache 滿時淘汰最舊項。

Radio adapter 在轉發前執行 CAD。忙碌時以硬體 RNG 取 50–200 ms 退避，有限次重試；不做無上限 busy-loop。傳送與接收使用事件狀態機，避免長時間阻塞 GNSS/鍵盤更新。

## State and storage

- NVS/Preferences：node id、daily sequence、message counter、PIN-derived profile metadata；不保存明文 PIN。
- SD `/message.txt`：每行一個 UTF-8 快捷短語，最多 9 行/每行 80 bytes；解析失敗回退內建短語。
- SD `/tracks/YYYYMMDD.csv`：有效 GNSS fix 依時間/距離節流後附加。v1 畫面只讀當次開機的 bounded point ring，避免整日檔案耗盡 RAM。
- flash `logfs` 分割（LittleFS，4.9 MB，ADR-008）：`/msg.log` 每行一筆 `id|unix|kind|source|dir|clip|sender|text`，`/v/<clip_id>.c2` 為 Codec2 位元組對齊音檔。開機只讀檔尾 12 KB 還原顯示用的 60 筆 ring。可用空間低於 128 KB 時只刪編號最舊的語音；文字有自己的 384 KB 上限才輪替，不因語音空間不足而被刪。這個分割與 SD 上的軌跡互不影響。

## Failure behavior

- Radio/GNSS/SD/Audio 可個別 degraded；開機 diagnostics 顯示狀態。音訊初始化失敗不得阻塞文字、位置與 UI。
- 無有效 GNSS 時不廣播虛假 0/0 座標，但仍必須廣播帶 no-fix 旗標的 Beacon：隊友名單不得依賴 GNSS。主畫面須分辨「沒收到 NMEA」「收到但無 fix」「已定位」三種狀態，`GPS 0` 本身不足以判斷該走到空曠處還是該檢查模組。
- logfs 不可用時降級為 RAM-only 歷史，並在歷史列明確告知本次不保存。
- 語音缺任一 fragment、超時或解碼失敗即整段捨棄，不播放部分資料。
- 喇叭功放只在真的要發聲時啟動，輸出結束 300 ms 後必須關閉；開機時即使 `internal_spk` 已初始化也要立刻關掉，避免靜態耗電與可聽見的底噪。
- sequence gap 是觀測提示，不等同 delivery guarantee；亂序或裝置重啟不得產生大量虛假漏訊。
- 本裝置不是 PLB、衛星信使或認證救援設備；UI/README 必須保留此限制。

## Verification gates

1. Core gate：native tests 覆蓋 wire round-trip/拒絕壞格式、分片邊界、重組亂序/超時、去重/TTL、sequence、geo、profile determinism 與語音 schema/三秒界線。
2. Firmware gate：PlatformIO `cardputer-adv` clean build；static check 若工具可用。
3. Hardware gate：兩機文字/Beacon/語音、三機 relay、GNSS track、SD phrases/key、電池與 airtime 實測。沒有硬體時不得宣稱通過。
