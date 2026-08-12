# Upgrade notes — v3.0 → v4.0

日期：2026-07-25

定位：Claude Code 新專案部署基線，優先支援 Claude Fable 5 / Opus 5。

## 結論

v3.0 已採用 progressive disclosure，但仍用舊模型思路管理新模型：依行數強制 Gate、固定 WIP=1、固定 coverage 百分比、每次收尾同步多份狀態文件，並複製 Claude Code 已內建的通用安全與驗證能力。

v4.0 將框架從「流程控制器」改為「高訊號介面」：

- 常駐 context 只保留真實指令、不可推知的 invariants、授權邊界、gotchas 與 compaction 保留項。
- 只有 `discovery`、`verify-change` 兩個自動 Skills 的描述會常駐；完整驗收、交接、Release 均為手動。
- 用不確定性、可逆性、blast radius 與外部 side effect 決定治理強度，不再用行數或檔案數假裝量化風險。
- 暫時工作狀態回歸 Session、auto-memory 與 git；只有跨 Session／agent 的未完工作才寫 HANDOFF。
- 通用能力優先使用 Claude Code 內建 `/verify`、`/code-review`、`/security-review`，不在 repo 重複維護。

## Context 預算差異

以下以 UTF-8 bytes 計算，並非 Anthropic tokenizer 的精確 token 數；它用來比較同一批中文內容的相對負擔。實際載入仍應在 Claude Code 用 `/context` 與 `/doctor` 檢查。

| 項目 | v3.0 | v4.0 | 差異 |
|---|---:|---:|---:|
| `CLAUDE.md` 原始行數 | 50 | 38 | -24.0% |
| `CLAUDE.md` 注入 bytes | 2,999 | 1,335 | -55.5% |
| 自動可觸發 Skill descriptions | 1,698 bytes / 6 個 | 431 bytes / 2 個 | -74.6% |
| 固定啟動 context 估算 | 4,697 bytes | 1,766 bytes | **-62.4%** |
| v3.0 再加其規定接手必讀 HANDOFF | 5,788 bytes | 1,766 bytes | **-69.5%** |

v4.0 的 HTML 維護註解不會被 Claude Code 注入，因此不計入 `CLAUDE.md` 注入 bytes。手動 Skills 設有 `disable-model-invocation: true`，其 description 與 body 在未呼叫時不進入 context。

## 逐項變更

| v3.0 | v4.0 | 本質差異 |
|---|---|---|
| `CLAUDE.md` 含 5 條紅線、狀態檔表、Skills 路由與固定收尾流程 | 只留 commands、invariants、必要 boundaries、gotchas、compaction | 刪除模型可自行判斷或由 Skill description 已表達的內容 |
| `>20 行`、`>2 檔` 一律 Gate 並等待授權 | `discovery` 只處理會改變結果的未知項與高代價決策 | 行數不是風險代理；可逆的跨檔變更可能低風險，單行 prod 變更可能高風險 |
| `project-intake` 逐題訪談 + `plan-gate` | 合併為 `discovery` | 先查 repo／一手資料／reference，再只問 1–3 個決策題 |
| `verification` 固定 L0–L4、coverage 80%/60%、每次更新三份文件 | `verify-change` 依 diff 與風險選最小高訊號 proof | coverage 門檻由真實 repo／CI 決定；證明行為，不追求模板儀式 |
| `security-review` 複製通用 OWASP checklist | 移除，改用 Claude Code 內建 `/security-review` | Skills 應編碼專案或團隊特有知識，不複製模型已有的一般知識 |
| `acceptance` 可自動觸發且內容留在主 context | `/acceptance` 手動觸發並用 `context: fork` | 完整稽核成本高，隔離大量讀檔與測試輸出 |
| `release-ops` 可由模型觸發 | `/release` 僅手動觸發 | 部署、Tag、push、merge 與 production side effect 不可由「看起來準備好了」觸發 |
| 每個 Session 寫 FEATURE_LIST、todo、HANDOFF；另有 PROGRESS、lessons | 移除 FEATURE_LIST、todo、PROGRESS、lessons；HANDOFF 改選用 | 消除四份狀態真相、同步 I/O 與漂移 |
| 每個 Release／完成狀態皆要求 CHANGELOG | 只記使用者可見或 Release 變更 | Git history 管內部變更，CHANGELOG 服務使用者 |
| `.env.example` 宣稱已有 `.gitignore`，實際缺檔 | 新增 `.gitignore` + `.claude/settings.json` | Git 降低誤提交；Claude Code `Read(path)` 規則阻擋內建檔案工具讀取常見真實環境檔 |
| 模型選擇未制度化 | README 加入 Opus／Fable 與 effort 判斷 | 推理錯誤升模型；漏做工具工作升 effort |

## 新 Skills 結構

### 自動、低常駐成本

- `discovery`：發現 decision-changing unknowns；清楚、局部、可逆工作不觸發。
- `verify-change`：根據實際 diff 建立驗證迴圈，保留精確證據與殘餘風險。

### 手動、未呼叫零啟動 context

- `/acceptance [scope]`：完整 Release readiness audit，隔離執行。
- `/handoff [note]`：只有未完工作需轉交時更新快照。
- `/release [target]`：受控發布；production 與外部 side effect 必須有具體授權。

## 刻意沒有加入的東西

- **通用 CI 與 hooks**：模板不知道語言、package manager、部署平台與風險面；放入假指令只會製造虛假安全。專案初始化後，把真正常態的 test、lint、secret scan 與 migration policy 寫成 CI／hook。
- **預設 subagents**：subagent 應對應可獨立驗證的實際工作面。模板只建議將大型調查與完整驗收隔離，不預先發明角色。
- **大量 `.claude/rules/`**：沒有 `paths` 的 rules 仍會每次載入。只有 codebase 出現真正不同的模組規則後才建立 path-scoped rule。
- **把 README／規格 import 到 CLAUDE.md**：import 只是組織方式，不是 lazy loading。

`Read(path)` deny 不等於作業系統 sandbox；任意 subprocess 仍可能自行讀檔。正式專案要再搭配 secret manager、sandbox、secret scan 與最小權限。

## 部署

1. 合併到 repo 根目錄並完成 `CLAUDE.md` 尖括號欄位；刪除不適用列。
2. 將真實的 install、targeted test、full test、lint／typecheck、build、dev 指令填入。
3. 執行 `/doctor` 與 `/context`；確認只有預期的 memory files 與兩個自動 Skill descriptions。
4. 應用程式能啟動後執行 `/run-skill-generator`，建立專案真實 runtime recipe。
5. 用一組實際任務觀察 `discovery` 是否過度／不足觸發；先改 description，不把整套流程搬回 `CLAUDE.md`。
6. Claude Code 版本過舊時先更新；本包的 `Read(path)` 權限規則需要支援新式 file permission matching 的版本。

## 依據

- Anthropic：[The new rules of context engineering for Claude 5 generation models](https://claude.com/blog/the-new-rules-of-context-engineering-for-claude-5-generation-models)
- Anthropic：[A field guide to Claude Fable 5: Finding your unknowns](https://claude.com/blog/a-field-guide-to-claude-fable-finding-your-unknowns)
- Claude Code Docs：[Best practices](https://code.claude.com/docs/en/best-practices)
- Claude Code Docs：[How Claude remembers your project](https://code.claude.com/docs/en/memory)
- Claude Code Docs：[Extend Claude with skills](https://code.claude.com/docs/en/skills)
- Anthropic：[Building verification loops in Claude Code with skills](https://claude.com/blog/building-verification-loops-in-claude-code-with-skills)
- Anthropic：[Choosing a Claude model and effort level in Claude Code](https://claude.com/blog/claude-model-and-effort-level-in-claude-code)
- Anthropic：[Claude models explained: choosing the best model for your use case](https://claude.com/blog/claude-models-explained-choosing-the-best-model-for-your-use-case)
