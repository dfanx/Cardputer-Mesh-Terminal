---
name: security-baseline
description: 專案安全底線檢查。當 diff 或計畫觸及認證授權、付款、個資、密鑰、DB migration、檔案上傳或公開 API，以及 Release 前與使用者要求安全審查時使用。
---

# Security Baseline

優先執行 Claude Code 內建 `/security-review`；本 skill 補足它不涵蓋的三件事：本專案風險面登記、內建指令缺席時的後備底線、可稽核的結果紀錄。

## 1. 本專案風險面登記（初始化與新增高風險功能時維護）

| 風險面 | 位置（路徑／模組） | 特殊要求 |
|---|---|---|
| <例：多租戶隔離> | <path> | <每條查詢必帶 tenant_id> |

未登記的面不代表安全，代表尚未評估。

## 2. 後備底線（內建 `/security-review` 不可用或結果需交叉驗證時逐項執行）

- 注入：DB 查詢一律參數化／ORM；外部輸入 deny-by-default 驗證；上傳限制型別、大小與路徑。
- 認證：密碼 bcrypt／argon2；登入節流；Session／Token 可過期可撤銷。
- 授權：每個後端端點伺服器側檢查；實測水平與垂直越權；新端點預設拒絕。
- 資料：repo 與日誌無密鑰個資（gitleaks／trufflehog）；全程 HTTPS；生產關閉 debug、錯誤不含堆疊。
- 依賴：npm audit／pip-audit 高危已處理；CORS 與安全 Header（CSP、HSTS）已設定。
- 監控：認證失敗、權限拒絕、異常輸入有結構化日誌；認證與寫入端點有速率限制。

## 3. 紀錄

結果寫入 `docs/reviews/security-<scope-or-date>.md`：檢查範圍、執行方式（內建／後備／兩者）、每項 pass / fail（開修復項）/ N/A（附理由）、殘餘風險。高風險面變更未附本紀錄不得進入 `/release`。
