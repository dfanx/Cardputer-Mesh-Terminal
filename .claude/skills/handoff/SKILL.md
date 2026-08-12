---
name: handoff
description: 將未完成工作濃縮成跨 Session 或跨 agent 可接手的快照。
argument-hint: "[optional-note]"
disable-model-invocation: true
---

# Handoff

只有工作確實需要轉交時才更新 `docs/HANDOFF.md`。若可直接用同一 Session 的 `--resume` 延續，不建立重複紀錄。

以目前對話、git status／diff、分支與實際驗證結果為準，覆寫 HANDOFF 的舊快照：

- 目標與明確 non-goals
- 當前分支、基準 commit、working tree 狀態
- 已完成內容與修改檔案
- 關鍵決策、假設與不可破壞的限制
- 已執行的驗證及精確結果
- 未完成內容、阻塞與待使用者決策
- 下一個最小具體動作與可直接執行的指令
- `$ARGUMENTS` 中的補充

保持自包含且短；不要複製聊天紀錄、完整 diff、git history、一般 coding 常識或已過期計畫。不得把密鑰、Token、個資或真實 `.env` 值寫入。
