---
name: discovery
description: 在昂貴實作前找出會改變方向的未知項。新專案、模糊大型功能、陌生領域、架構／資料模型／UX 決策或適合先做 prototype 時使用；清楚、局部、可逆的變更不要觸發。
---

# Discovery

目標是降低錯方向成本，不是增加文件或會議。

## 1. Ground before asking

- 先讀使用者提供的內容、相關程式碼、測試、歷史決策與高保真參考。
- 分開列出：已知事實、暫定假設、已知未知、可能的盲點。
- 只保留會改變產品行為、架構、資料、成本、時程、安全或驗收方式的未知項。

## 2. Use the cheapest uncertainty reducer

依成本由低到高選擇：搜尋 repo → 查一手文件 → 比對既有模式 → 最小 spike／HTML prototype → 詢問使用者。

- 可由證據回答的問題，不轉嫁給使用者。
- 方向清楚且可逆：明示假設後直接推進。
- 只有使用者的產品偏好、商業取捨或不可逆選擇才能回答時，集中詢問 1–3 個決策題，附建議預設值與影響。
- 視覺或互動需求若「看到才知道」，先做便宜的多方向 prototype，不先接完整後端。

## 3. Decision brief

對高代價或長鏈路工作，實作前輸出短版：

- Outcome 與 non-goals
- 會改變結果的假設／未知項
- 主要介面、資料或 UX 決策
- 最小可驗證切片
- 風險、驗證與回滾

需要使用者決策時停在該決策；觸及 `CLAUDE.md` Boundaries 所列高風險面時，brief 必含回滾方式與安全考量，並停在授權點等待同意。其餘機械性細節由 agent 判斷。

## 4. Durable output

- 新專案或方向改變時才更新 `docs/PROJECT_BRIEF.md`。
- 只把長期、昂貴、難逆轉的決策寫入 `docs/DECISIONS.md`。
- 複雜功能若需跨 Session，再建立 `docs/features/<name>.md`；優先引用測試、程式碼或 prototype。
- 長任務可暫記 `.claude/tmp/implementation-notes.md`。結束時只把仍有價值的內容濃縮到 ADR、spec 或 handoff，避免永久累積。
