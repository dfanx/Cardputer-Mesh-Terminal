#include "cmt/app/mesh_terminal_app.h"

// Arduino 預設的 loopTask stack 是 8 KB，不足以容納 Codec2。實測 MODE_1300
// 在 loopTask 上的峰值用量為 18,184 bytes（create 6,804、encode 與 decode 再
// 疊上去），8 KB 會在 codec2_create() 當場觸發 stack overflow 並無限重啟。
// 32 KB 保留約 14.5 KB 餘裕給 render、mbedTLS GCM 與 RadioLib 路徑。
SET_LOOP_TASK_STACK_SIZE(32768);

cmt::MeshTerminalApp app;

void setup() { app.setup(); }

void loop() { app.loop(); }
