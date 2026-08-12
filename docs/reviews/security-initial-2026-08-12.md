# Security review — initial protocol design

- **Date**：2026-08-12
- **Scope**：群組配對、key derivation、wire packet、位置/語音資料、relay。
- **Method**：專案 `security-baseline` 後備檢查 + threat-model review；尚無可呼叫的 Claude Code `/security-review` runtime。

## Risk surface registry

| Risk surface | Location | Requirement |
|---|---|---|
| PIN-derived group key | `src/platform` runtime only | 不進 repo/日誌；安全目標僅群組辨識與基本遮蔽，不宣稱強安全 |
| Packet authentication | `include/cmt/core/wire_protocol.h`, crypto adapter | AES-GCM tag 驗證先於顯示/重組/relay；nonce 不得重複 |
| Precise location | Beacon codec, track storage, UI | 僅同組認證封包可見；Serial 不輸出座標；SD track 由持卡者承擔實體保護 |
| Voice | audio adapter/reassembly | 完整認證與重組後才播放；不持久保存原始語音 |
| Public RF protocol | wire format / radio adapter | parser 有長度、版本、type、fragment 上限；無動態無界配置 |

## Baseline results

| Check | Result | Evidence / action |
|---|---|---|
| Input validation | Pass by design | PIN、SD 行長、wire length/type/count 全採 allow-list/bounds；由 native negative tests 證明 |
| Authentication / authorization | N/A | 無帳號或 server；群組 AEAD tag 是資料接受邊界 |
| Secret handling | Pass for stated scope | repo 無真實 PIN；PIN 只在 RAM 進行 PBKDF2，join 後覆寫並清除；Serial 不記錄 PIN/位置/語音 |
| Data in transit | Accepted product risk | AES-128-GCM 提供基本遮蔽/完整性；使用者接受 4 PIN 可被離線枚舉 |
| Dependency audit | Partial | 直接依賴固定 PlatformIO/M5Cardputer/RadioLib/TinyGPSPlus/esp32_codec2 版本並成功 clean build；未接第三方 CVE 資料庫，Codec2 套件有 GPL-3.0 散布義務 |
| Logging / monitoring | Pass by design | 僅匿名 error counters，不記 PIN/key/語音/座標 |
| Replay / duplicate | Pass by design | nonce + bounded duplicate cache + message/fragment identity；跨長期 capture 的 anti-replay 仍有限 |
| Resource exhaustion | Pass for core bounds | 固定 packet/fragment/reassembly/cache/queue 上限；native tests 驗證 oversized 拒絕、重組 timeout 與 duplicate cache |

## Verification evidence

- `tools/test-native.cmd`：9 tests、0 failures，包含壞 header、越界訊息、竄改 tag、分片 timeout、TTL、duplicate，以及語音 schema/三秒界線。
- `tools/pio.cmd run -e cardputer-adv`：啟用 Codec2 與音量控制的 clean build 成功；RAM 靜態配置 24,628 / 327,680 bytes（7.5%），Flash 1,057,265 / 3,342,336 bytes（31.6%）。錄音/播放共用約 48 KB 動態 PCM buffer，此數字不包含在靜態 RAM 報表，仍需實機觀察 heap 最低水位。
- `tools/pio.cmd check -e cardputer-adv --skip-packages`：cppcheck 通過；本專案程式 0 high / 0 medium，low 項為 style/whole-program unused 誤判。總表的 148 medium / 386 low 來自 TinyGPSPlus/M5 等第三方 headers，不視為本專案已修復。

## Residual risk

- 四位 PIN 即使使用 PBKDF2 仍可被離線枚舉；產品只把它視為群組辨識與基本隱私。
- 群組共享對稱 key 無法識別惡意組員，也無個別撤銷或 forward secrecy；這符合目前低安規定位。
- SD 軌跡的 at-rest protection 不在 v1 範圍；遺失 SD 卡會洩漏內容。
- 裝置 nonce 使用 source id + 每次開機硬體隨機值 + counter；硬體 RNG 故障或極低機率 boot random 碰撞仍可能造成 reuse。
- RF jamming、流量分析與位置測向不因加密而消失。
- 語音在完整重組後才配置播放 PCM；schema 限定 75 frames，但實機 codec CPU 峰值、heap 餘量與惡意重播負載仍需量測。
