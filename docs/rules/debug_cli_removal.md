# Debug CLI 退场规则

`App_DebugCli` 是 AI 上板、抓串口和定位问题用的调试入口，不是生产业务协议。正式固件不能依赖串口命令才能完成充电、放电、保护恢复或均衡。

## 当前归属

- 对外头文件：`App/App_DebugCli.h`
- 实现文件：`App/App_DebugCli.c`
- 任务入口：`App/App_Main.c` 里的 `debug_cli_task`
- 初始化入口：`App_Main_Init()` 里的 `App_DebugCli_Init()`
- 运行期日志抑制入口：
  - `App_DebugCli_IsVofaStreaming()`
  - `App_DebugCli_IsBqMonitoring()`

## 删除步骤

1. 删除 `App/App_DebugCli.c` 和 `App/App_DebugCli.h`。
2. 从只用于日志抑制的模块里移除 `#include "App_DebugCli.h"`。
3. 从 `App/App_Main.c` 删除 `debug_cli_task`、对应 `xTaskCreate()` 和 `App_DebugCli_Init()`。
4. 把 `App_DebugCli_IsVofaStreaming()`、`App_DebugCli_IsBqMonitoring()` 替换为生产日志策略；如果没有生产日志策略，先替换为 `false`。
5. 如果没有非 CLI 调用者，删除这些手动探测入口：
   - `App_BatMan_TestPreDischargeOnly()`
   - `App_BatMan_AllMainFetsOff()`
   - `pdsg test`
   - `pdsg probe`
   - `scprobe`
   - `charge on/off`
   - `vofa on/off`
   - `bqfast on`
6. 仍需要给产测、CAN、OLED 或上位机使用的诊断能力，迁移到独立的生产诊断模块，不要继续挂在 CLI 命令解析里。
7. 执行 `cmake --build --preset gcc-debug`，确认没有未解析符号、未使用静态函数或头文件残留。

## 删除前必须确认

- BQ 初始化、采样、保护配置不依赖任何串口命令。
- SC8815 只能由 `App_Power` 的业务状态机请求充电。
- 故障清除/恢复有生产触发方式；如果没有，必须明确依赖重新上电恢复。
- 产测需要的充放电日志已有替代输出路径。
