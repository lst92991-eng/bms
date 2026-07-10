# DebugCLI 最终无影响退场规则

`App_DebugCli` 是 AI 上板抓串口、连续采样和执行人工探测的临时入口，不是生产业务协议。当前测试阶段必须保留；只有下面的前置门禁和上板回归都通过后，AI 才能删除。

## 1. 当前能力与依赖

- 实现与头文件：`App/App_DebugCli.c`、`App/App_DebugCli.h`。
- 任务与初始化：`App/App_Main.c` 中的 `debug_cli_task`、`App_DebugCli_Init()` 和 `xTaskCreate()`。
- 构建入口：`CMakeLists.txt` 和 `bms24v_platform/MDK-ARM/bms24v_platform.uvprojx`。
- 串口所有权：DebugCLI 实现了 USART1 的 `HAL_UART_RxCpltCallback()`、`HAL_UART_ErrorCallback()`，并维护中断接收环形缓冲区。
- 周期日志抑制：`App_BatMan_Debug.c`、`App_Power.c`、`App_SC8815.c` 只通过 `App_DebugCli_IsStreaming()` 判断 CLI 是否正在连续占用串口。它只影响日志，不参与充放电判断。
- AI 命令：`diag`、`bq`、`bq on/off`、`bqfast on`、`power`、`sc`、`scprobe`、`vofa on/off`、`charge on/off`、`fault/scd/dsg clear`、`pdsg test/probe/off`、`bq shutdown`。

正常业务路径不应依赖这些命令：BQ 初始化/采样由 `App_BatMan` 运行，SC8815 充电请求由 `App_Power -> App_SC8815_RequestCharge()` 运行，主 FET 由 `App_Power -> App_BatMan_SetMainFets()` 运行。

## 2. 删除前硬门禁

必须同时满足：

1. 冷启动后不发送任何 CLI 命令，充电器插入可自动充电，拔出后可正常放电。
2. BQ 初始化、Data Memory 配置、采样、均衡、SC8815 限流和主 FET 控制不需要人工 `charge on`、`bq` 或 `fault clear` 才能进入正常状态。
3. 已决定 SCD 软件锁存的生产恢复方式：
   - 迁移 `App_Power_ClearDischargeFault()` 到 CAN/产测/生产诊断入口；或
   - 明确产品只允许重新上电恢复，并删除运行期清除能力。
4. `bq shutdown` 若属于产品功能，先迁移到正式电源管理入口；否则随 CLI 测试命令一起删除。
5. 产测需要的日志已有 CAN、正式诊断或独立产测固件替代。

第 3 项当前尚未闭环。未闭环时直接删除 CLI 会失去 `fault clear` 人工恢复路径，不能声称“无影响”。

## 3. AI 的安全删除顺序

### 阶段 A：先断开依赖，不删文件

1. 在 `App_BatMan_Debug.c`、`App_Power.c`、`App_SC8815.c` 中移除 `App_DebugCli_IsStreaming()` 条件和 `#include "App_DebugCli.h"`，保留条件内部原有周期日志代码。CLI 默认未连续输出时该条件本来就成立，因此只恢复正常周期日志，不改变功率逻辑。
2. 在 `App_Main.c` 删除 `debug_cli_task`、`APP_MAIN_DEBUG_CLI_TASK_PERIOD_MS`、`App_DebugCli_Init()`、对应 `xTaskCreate()` 和头文件引用。
3. 此时暂不删除 CLI 源文件，先完成 Debug/Release 构建和一次无 CLI 上板回归。

### 阶段 B：删除源码和仅调试访问面

4. 从 `CMakeLists.txt` 删除 `App/App_DebugCli.c`。
5. 从 `bms24v_platform/MDK-ARM/bms24v_platform.uvprojx` 删除 `App_DebugCli.c/.h` 两个文件项。
6. 删除 `App/App_DebugCli.c`、`App/App_DebugCli.h`。USART1 若仍有生产接收者，必须先把两个 HAL UART 回调迁移给新的唯一所有者；没有接收者时随文件一起删除。
7. 用 `rg` 重新确认调用者，仅在“只剩声明和定义”时删除以下调试访问面：
   - BQ 手动 FET：`App_BatMan_TestPreDischargeOnly()`、`App_BatMan_AllMainFetsOff()`。
   - BQ 连续诊断：`App_BatMan_PrintSnapshot()`、`App_BatMan_PrintMonitor()`、`App_BatMan_PrintFastMonitor()`、`App_BatMan_IsMonitorFaultActive()`、`App_BatMan_PrintMonitorStopReason()`。
   - SC 线序探测：`Int_SC8815_ProbeAddress()`、`Int_SC8815_ReadRegWithLineOrder()`、`Int_SC8815_IsIicLineSwapped()`、`Int_SC8815_GetBusLevels()`。
   - SC CLI 快照 getter：`App_SC8815_IsCommOk()`、`App_SC8815_IsCharging()`、`App_SC8815_GetVbatMv()`、`App_SC8815_GetInputLimitMa()`。
   - 电源诊断：`App_Power_PrintSnapshot()`、`App_Power_PrintStopReason()`、`App_Power_RequestBqShutdown()`。
   - `App_Power_ClearDischargeFault()` 按第 2 节的生产恢复决定迁移或删除，不能机械删除。
8. 必须保留生产调用仍在使用的 `App_SC8815_RequestCharge()`、`App_SC8815_IsAcOk()`、`App_SC8815_HasFault()`、`App_SC8815_GetVbusMv()`、`App_BatMan_SetMainFets()` 和正常状态机接口。

## 4. 静态清零检查

删除后执行：

```powershell
rg -n "App_DebugCli|debug_cli_task|bqfast|scprobe|pdsg probe|vofa" App Com Int CMakeLists.txt bms24v_platform/MDK-ARM/bms24v_platform.uvprojx
cmake --build --preset gcc-debug --clean-first
cmake --build --preset gcc-release --clean-first
arm-none-eabi-nm build/gcc-release/bms24v_platform.elf | rg "App_DebugCli|debug_cli_task"
```

预期：第一条和最后一条无输出；两套 GCC 构建通过。若使用 Keil 发布，还必须执行一次 MDK 全量 Rebuild，确认项目文件没有残留引用。HAL 库自身可能保留弱 `HAL_UART_*Callback` 符号，但应用中不能再有 DebugCLI 的强符号。

## 5. 无 CLI 上板回归

1. 冷启动，全程不发串口命令，确认 BQ/SC 初始化完成且主功率状态符合预期。
2. 插入 24V 充电器，确认 SC8815 自动退出 standby、输入/电池限流值正确、BQ CHG 路径打开并能正常充电。
3. 拔出充电器接负载，确认 DSG 路径、预放电、欠压和温度边界正常。
4. 触发或模拟一次 SCD，按已选生产恢复方式确认锁存与恢复。
5. 确认 OLED 和保留的生产日志正常，USART1 不再需要 CLI 输入即可运行。
6. 对比删除前后的关键串口/波形：PSTOP、#CE、CHG/DSG/PDSG、VBUS、VBAT、IBUS、IBAT。

只有以上步骤全部通过，才能提交“DebugCLI 已干净删除且不影响现有充放电逻辑”的结论。
