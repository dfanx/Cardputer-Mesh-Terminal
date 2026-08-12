---
name: acceptance
description: 對指定 scope 做完整、證據導向的 Release readiness 驗收。
argument-hint: "[scope-or-version]"
disable-model-invocation: true
context: fork
agent: general-purpose
---

# Acceptance

驗收 `$ARGUMENTS`；若未指定 scope，以目前分支相對基準分支的變更為範圍。這是審查工作：除非呼叫參數明確要求修復，否則不要修改產品程式碼。

## 1. Ground

- 讀取 `CLAUDE.md` 的真實指令、相關 spec／測試／ADR，以及實際 diff。
- 建立「需求或 acceptance criterion → 證據」矩陣；沒有需求依據的偏好不得列為缺陷。
- 先確認環境與依賴可用，分開記錄產品失敗與環境阻塞。

## 2. Inspect and exercise

依產品型態選擇高訊號證據，不為填表而執行低價值檢查：

- 靜態：diff、型別、lint、依賴與邊界條件。
- 自動化：repo 既有的 targeted、integration、full suite 與 CI gate。
- Runtime：用 `/verify`、`/run` 或專案 E2E 觀察真實行為；UI 同時檢查錯誤、空白、loading、權限與窄螢幕。
- 高風險：auth、授權、付款、個資、密鑰、公開 API 或 migration 觸發 `security-baseline` skill 與對應失敗路徑測試。
- 回滾：驗證回滾步驟確實可執行；資料變更需確認向前相容與恢復界線。

沿用 repo／CI 已定義的 coverage 與品質門檻；模板不得自行發明全域百分比。

## 3. Report

將報告寫入 `docs/reviews/acceptance-<scope-or-date>.md`：

1. 結論：Ready / Conditional / Not ready。
2. 驗收矩陣：criterion、證據、結果、缺口。
3. Findings：P0 阻擋、P1 上線前、P2 後續；每項含位置、可重現證據、影響、建議修法。
4. 執行過的指令與結果摘要。
5. 未驗證範圍、環境限制、殘餘風險與回滾狀態。

沒有可重現證據的疑慮標為「待確認」，不得包裝成確定缺陷。
