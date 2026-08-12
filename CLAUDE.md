# Cardputer Mesh Terminal

M5Stack Cardputer Adv + Cap LoRa-1262/GNSS 的山區離網文字、定位與短語音 Mesh 韌體。

## Commands

- Install: `python -m venv .venv; .\.venv\Scripts\python -m pip install -r requirements-dev.txt`
- Dev / serial monitor: `.\tools\pio.cmd device monitor -e cardputer-adv`
- Test targeted: `.\tools\test-native.cmd`
- Test full: `.\tools\test-native.cmd`
- Lint / static check: `.\tools\pio.cmd check -e cardputer-adv`
- Build: `.\tools\pio.cmd run -e cardputer-adv`

## Invariants

- SX1262 實體 payload 上限 255 bytes；wire format 必須逐欄位序列化，不得直接傳送 C/C++ struct。
- 未通過 group、格式與 AES-GCM tag 驗證的封包不得顯示、重組或中繼。
- 文字與 Beacon 初始 TTL=2；語音初始 TTL=1；每次中繼必須遞減 TTL、遞增 hop 並重做認證標記。
- 4 位 PIN 的安全目標只限群組辨識、基本內容遮蔽與防止誤收；不得宣稱能抵抗竊聽者的離線暴力破解。
- 預設射頻設定只適用台灣開發測試基線；頻率、功率與空中時間仍受當地法規及設備認證約束。
- 韌體不得把 PIN、原始語音或精確位置輸出到 Serial 日誌。

產品範圍見 `docs/PROJECT_BRIEF.md`；持久技術決策見 `docs/DECISIONS.md`。不要把這兩份文件匯入常駐 context。

## Boundaries

- 密碼、Token、私鑰與個資不得進入 repo、日誌、測試快照或回覆。
- 未經明確授權，不燒錄裝置、不操作 production、不發布或合併外部變更。
- 認證授權、密鑰、公開協定或持久格式的行為變更，先更新 ADR、安全檢查與回歸測試。
- 同時只推進一個工作流；方向清楚且可逆時直接推進。
- 驗證採最小高訊號原則；回報實際結果與未驗證範圍。

## Codebase gotchas

- Cardputer Adv 的 LoRa 天線開關必須透過 I2C `PI4IOE5V6408` P0 拉高，否則 SX1262 初始化成功也可能無 RF 路徑。
- Cardputer Adv 的麥克風與喇叭共用音訊資源，錄放音切換前必須停止目前工作並切換 `M5.Mic` / `M5.Speaker`。

## Compaction

保留：目標、已修改檔案、ADR、測試指令與結果、未解錯誤、下一個具體動作。
