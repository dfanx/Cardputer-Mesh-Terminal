# Project brief

狀態：`confirmed`

最後更新：`2026-08-12`

## Problem and outcome

- **要解決的問題**：山區沒有行動網路時，同行隊員缺少可攜、低功耗的文字、短語音與相對位置備援通道。
- **目標使用者與使用情境**：持有 Cardputer Adv + Cap LoRa-1262 的登山隊伍，在視線受阻與行動網路不可用時進行近距離到區域性協作。
- **預期成果**：單一可燒錄韌體提供群組配對、基本內容遮蔽與完整性、去中心化最多兩跳中繼、文字、GNSS Beacon 與隊友相對位置雷達，以及最多 2.2 秒的 Codec2 PTT 語音。
- **成功訊號**：兩台實機能在合法射頻設定下完成文字、Beacon 與短語音往返；三台實機能完成一次文字/Beacon 中繼；損片、重複與認證失敗不造成錯誤顯示或重播。

## Scope

### In

- 4 位 PIN 群組、頻道與相容模式金鑰映射；安全目標限群組辨識，不處理高機密資料。
- AES-128-GCM 認證加密、版本化封包、分片/重組、序號缺口、TTL、去重與 CAD 退避。
- 可視化罐頭訊息選單、數字快速發送與英數文字；MicroSD `message.txt` 自訂短語。
- GNSS 解析、10 分鐘 Beacon、隊友相對距離/方位/高差雷達（ADR-011：不再記錄全軌跡，消費級 GNSS 的原地飄移在小螢幕折線圖上沒有導航價值）。
- Space PTT、2.2 秒限制、Codec2 700C 壓縮、單跳語音與播放；音訊不可用時不阻塞主功能。

### Out / not now

- LoRaWAN、網際網路、雲端帳號、OTA、手機 App、地圖圖磚與中文輸入法。
- 救援級 SLA、已認證無線產品宣稱、端到端 delivery acknowledgement 與自動重傳。
- 跨協定相容 Meshtastic/MeshCore。

## Critical journeys

1. Given 兩台裝置輸入相同 PIN/密鑰，When A 發送文字，Then B 驗證、顯示來源/序號且不同群組不接受。
2. Given B 無法直達 A 但 C 可同時連通，When A 發送 TTL=2 Beacon/文字，Then C 去重、CAD 退避並只中繼一次，B 顯示結果。
3. Given GNSS 已定位，When Beacon 到期，Then 裝置廣播座標/高度/電量並在雷達顯示其他隊員的距離、方位與高差。
4. Given 音訊功能可用且使用者按住 Space，When 放開或滿 3 秒，Then 語音被壓縮、分片、發送且接收端完整重組後播放；缺片時不播放殘缺音訊。

## Constraints and risk

- **團隊能力**：第一版由單一韌體 repo 交付；硬體驗證需至少兩台 Cardputer Mesh Kit。
- **資料敏感度**：使用者已確認不承載高度機密；PIN-derived 認證加密只作群組辨識、基本內容遮蔽與防誤收，不構成強安全。精確位置與語音仍不進日誌。
- **部署環境**：Cardputer Adv ESP32-S3FN8、8 MB flash、無外接 PSRAM 假設；Cap LoRa-1262 SX1262 + ATGM336H/AT6668。
- **可用性**：無基礎設施、best-effort 廣播；RF、GNSS、天候、地形與電池均可能造成失敗，不能取代 PLB/衛星通訊等正式救援設備。

## Decisions and references

- **已採用技術與 ADR**：見 `docs/DECISIONS.md`。
- **架構**：`docs/architecture/FIRMWARE.md`。
- **原始 PRD**：`Cardputer Mesh Terminal-code-1786500374597.md`。

## Unknowns

- Cardputer Adv + Cap 的實際 RF 輸出、接收靈敏度、續航與山區通訊距離，必須實機量測。
- Codec2 1300 在實機上的編碼延遲、可懂度與三秒語音空中時間，必須用目標 LoRa profile 量測。
- 上市或公開使用前的 NCC 型式認證與 duty-cycle/LBT 適用條件需由合規專業人員確認。
