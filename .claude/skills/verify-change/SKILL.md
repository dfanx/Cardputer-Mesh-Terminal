---
name: verify-change
description: 對已實作或已修復的變更建立最小高訊號驗證迴圈。當準備宣稱完成、修 bug、改行為、回報測試狀態或 diff 觸及高風險面時使用；純分析且未改檔時不觸發。
---

# Verify Change

驗證目標是證明使用者要的行為成立，不是追求儀式性全綠。

## 1. Determine the proof

1. 讀實際 diff、acceptance criteria、鄰近測試與 `CLAUDE.md` 指令。
2. 列出受影響行為與最可能回歸點。
3. 選擇最窄且足以失敗的檢查，再依風險擴大：
   - 文案／文件：格式、連結或既有 docs check。
   - 局部邏輯：targeted regression test + typecheck／lint。
   - 跨層行為：integration test + 實際 runtime 觀察。
   - UI／CLI：用 `/verify`、`/run` 或既有 E2E 操作真實 journey。
   - auth、權限、付款、個資、公開 API、migration：加入負向案例、隔離／相容性，並執行 `security-baseline` skill（內含內建 `/security-review` 與後備底線）。

沿用 repo／CI 的品質與 coverage 門檻；不要從模板發明固定百分比。

## 2. Close the loop

- 執行檢查；若失敗由本次變更造成，定位根因、修正並重跑。
- 不得刪測試、放寬 assertion、吞錯或關閉規則來製造綠燈。
- 既存或環境失敗要證明與本次 diff 的關係，分開回報，不假稱通過。
- bug 或明確行為變更通常補回歸測試；若成本不合理，保留可重現腳本並說明缺口。
- 無法執行的關鍵驗證要明示原因與最短補驗路徑。

## 3. Evidence language

最後回報：

- **Implemented**：改了什麼。
- **Verified**：實際指令／runtime 操作與結果。
- **Not verified**：未執行項目及原因。
- **Residual risk**：仍可能失敗的邊界。

只有足以覆蓋 acceptance criteria 的證據存在時，才使用「完成／已修復／驗收通過」。
