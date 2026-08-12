# Upgrade notes — v4.0 → v4.1

日期：2026-07-29

定位：修補 v4.0 在「防人為／AI 犯錯、安全性單點依賴」上的四個缺陷；保留 v4.0 的高訊號介面架構與 token 紀律不變。常駐 context 淨增約 300 bytes（CLAUDE.md 三行）＋ 一個自動 skill description（約 120 bytes）。

## 缺陷與修補對照

| # | v4.0 缺陷 | v4.1 修補 | 常駐成本 |
|---|---|---|---|
| 1 | 保留 gotchas 區卻刪除餵養機制（v3 的 lessons 二次提升迴路），AI 重複犯錯無累積防線 | gotchas 區內嵌一行提升規則：同類錯誤第二次發生即濃縮入常駐區，修復根因後刪除。不恢復獨立 lessons.md——首次錯誤由 auto-memory 承擔，避免第四份狀態檔 | +1 行 |
| 2 | 高風險面（auth／付款／schema／公開 API）只在驗證階段出現，實作前無授權點；對無力事後審查程式碼的使用者，事前授權是唯一有效人為防線 | Boundaries 新增：高風險面實作先以 discovery decision brief 取得同意；discovery 對應加上「停在授權點」。判準仍為風險面而非行數，不回退 v3 的 20 行 Gate | +2 行 |
| 3 | security-review 整體刪除、押注內建 `/security-review` 存在且不變，形成單點依賴；且無處登記「本專案」風險面與稽核結果 | 新增 `security-baseline` skill：內建指令優先，補三件內建不做的事——專案風險面登記表、後備通用底線、`docs/reviews/` 稽核紀錄。verify-change／acceptance／release 改為引用它 | 僅 description |
| 4 | settings.json 只擋根目錄 .env 變體與 secrets/；子目錄 .env、私鑰檔（*.pem、id_rsa）不設防 | deny 清單擴至 `**/.env`、`**/secrets/**`、`**/*.pem`、`**/*.key`、`**/id_rsa*`、`**/id_ed25519*`。維持逐一列舉 .env 變體而非 `.env.*` 萬用字元，以保留 `.env.example` 可讀 | 0 |

另兩處低成本強化：Boundaries 加入單一工作流原則（一次只推進一個工作流，切換先收斂或 `/handoff`），承接 v3 WIP=1 的意圖但降為原則而非硬規則；README 初始化步驟加入內建指令實測（`/help` 確認 `/verify`、`/security-review` 存在；實測手動 skills 可被斜線指令喚起——`disable-model-invocation` 在部分 Claude Code 版本曾出現使用者明確呼叫仍被拒絕的問題）。

## 刻意不改的部分

- 不恢復 FEATURE_LIST／todo／PROGRESS／lessons 四份狀態檔：v4.0 對狀態真相分裂與同步漂移的診斷正確；跨 Session 狀態由 git、auto-memory 與選用的 HANDOFF 承擔。
- 不恢復固定 coverage 百分比與 L0–L4 分類：verify-change 的四段證據語言（Implemented / Verified / Not verified / Residual risk）強迫陳述事實而非套分類，且門檻應由真實 repo／CI 決定。全新專案的門檻在 discovery 授權點與使用者議定，寫入 CI。
- 不加入通用 CI／hooks 假指令、預設 subagents、無 paths 的 rules：v4.0 的理由成立。

## 已知殘餘風險

- `security-baseline` 為自動觸發 skill，觸發依賴 description 品質；漏觸發時 Boundaries 的高風險面清單是保險（模型在授權點會被迫面對安全考量）。
- `Read(**/*.key)` 可能誤擋合法檔名（如 GraphQL schema.key 之類非密鑰檔）；遇到時在 settings.local.json 以 allow 精確放行單一路徑，不放寬 deny。
- 一切 Read deny 仍非 sandbox；subprocess 可自行開檔。正式專案的密鑰安全下限是 secret manager + pre-commit／CI secret scan，本包不替代。
