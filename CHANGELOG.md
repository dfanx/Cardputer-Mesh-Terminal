# Changelog

## [Unreleased]

### Added

- Cardputer Adv + Cap LoRa-1262/GNSS 第一版 PlatformIO 韌體。
- 4 位 PIN 群組/頻道映射與 PIN-derived AES-GCM 群組辨識。
- 255-byte 版本化分片封包、認證後去重、TTL 兩跳 Mesh 與 CAD 退避。
- 繁中罐頭訊息選單、數字快速發送、英數自由輸入與 SD `message.txt` 自訂。
- GNSS Beacon、SD 軌跡、螢幕軌跡圖、隊友相對距離/方位/高差與漏訊提示。
- 預設啟用 3 秒 Space PTT、Codec2 1300 壓縮、版本化語音 schema、分片發送、完整重組與自動播放。
- 主畫面 `+`／`-` 喇叭音量控制，0–100%、每次 10%、預設 50%。
- Native core regression tests 與 Cardputer Adv build workflow。

### Known limitations

- 尚未以兩台/三台實機驗證 RF、GNSS、SD、音訊與 relay journey。
- 4 位 PIN 可被離線枚舉，只用於群組辨識與基本內容遮蔽。
- 語音音質、實際空中時間、丟片體驗與錄放音切換尚未經兩台實機驗證。
