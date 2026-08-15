#include "cmt/app/mesh_terminal_app.h"

// SET_LOOP_TASK_STACK_SIZE 來自 esp32-hal。先前是靠 app 標頭輾轉帶進 Arduino.h，
// 那條鏈路已經斷開，這裡直接引用。
#include <Arduino.h>

// Arduino 預設的 loopTask stack 是 8 KB，不足以容納 Codec2。實測 MODE_1300
// 在 loopTask 上的峰值用量為 18,184 bytes（create 6,804、encode 與 decode 再
// 疊上去），8 KB 會在 codec2_create() 當場觸發 stack overflow 並無限重啟。
// 32 KB 保留約 14.5 KB 餘裕給 render、mbedTLS GCM 與 RadioLib 路徑。
SET_LOOP_TASK_STACK_SIZE(32768);

cmt::MeshTerminalApp app;

void setup() { app.setup(); }

void loop() { app.loop(); }
