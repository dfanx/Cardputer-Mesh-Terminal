---
name: release
description: 執行受控 Release、部署與回滾驗證。
argument-hint: "[target-environment-or-version]"
disable-model-invocation: true
---

# Release

處理 `$ARGUMENTS`。若目標環境或版本不明，先釐清；不得自行推定 production。

## Preflight

- 確認 release scope、版本、基準 commit、working tree 與 CI 狀態。
- 執行 repo 定義的 full verification；高風險面另執行 `security-baseline` skill，並確認 `docs/reviews/` 已有對應紀錄。
- 檢查設定／密鑰由安全管道注入，產物不含開發用設定。
- 有 migration 時確認向前相容、dry-run／備份或補償、舊版應用共存界線。
- 更新 CHANGELOG 與必要的 migration／操作說明；內部機械變更不灌入 changelog。
- 指出可執行的 rollback／roll-forward 方法與觸發條件。

## Authorization boundary

Staging 可依使用者明確要求執行。Production、資料變更、對外發布、push、merge 或建立 Tag 只有在本次請求已明確授權該具體動作與目標時才執行；否則停在 ready-to-run，列出精確命令、影響與回滾。

## Execute and observe

1. 優先 staged／canary／feature flag。
2. 執行後驗證 health signal 與至少一條核心 business journey。
3. 觀察 repo 定義的錯誤率、延遲與資料一致性訊號。
4. 超出門檻立即停止擴大並依預案回滾；保留可稽核證據。

回報 release identity、實際動作、驗證、監控、殘餘風險與 rollback readiness。未取得 production 證據不得宣稱「上線穩定」。
