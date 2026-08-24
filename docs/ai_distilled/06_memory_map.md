# 06 Memory Map

更新日期：2026-08-22

## MCU Memory Layout

| 区域 | GCC linker | Keil/startup | 结论 | 信心 | 人工确认 |
| --- | --- | --- | --- | --- | --- |
| Flash | origin `0x08000000`, length 128 KiB | target `STM32G0B1CBTx`；scatter/target 元数据对应 128 KiB | 一致 | High | Not needed |
| SRAM | origin `0x20000000`, length 144 KiB | target 元数据对应 144 KiB | 一致 | High | Not needed |
| 初始 MSP stack reservation | `_Min_Stack_Size=0x400` | startup `Stack_Size EQU 0x400` | 1 KiB 启动/异常栈保留 | High | Needed，map/水位 |
| C heap reservation | `_Min_Heap_Size=0x200` | startup `Heap_Size EQU 0x200` | 链接层保留 512 B；不表示 FreeRTOS 动态堆启用 | High | Not needed |

证据：`bms24v_platform/gcc/STM32G0B1CBTx_FLASH.ld:3-9`；`bms24v_platform/MDK-ARM/startup_stm32g0b1xx.s:31-60`；`bms24v_platform/MDK-ARM/bms24v_platform.uvprojx:10-21`。

## RTOS 静态内存

`configSUPPORT_STATIC_ALLOCATION=1`、`configSUPPORT_DYNAMIC_ALLOCATION=0`。所有任务 TCB/stack 和 idle task 内存为静态对象；没有运行期 FreeRTOS heap 分配路径。【事实】

| 任务 | stack words | 约字节数（32-bit StackType_t 推断） | 证据 |
| --- | ---: | ---: | --- |
| Safety | 320 | 1280 | `App/App_Main.c:25-78` |
| BatMan | 1024 | 4096 | `App/App_Main.c:25-78` |
| SC8815 | 512 | 2048 | `App/App_Main.c:25-78` |
| CAN | 512 | 2048 | `App/App_Main.c:25-78` |
| CLI（Engineering） | 512 | 2048 | `App/App_Main.c:25-78` |
| Maintenance | 768 | 3072 | `App/App_Main.c:25-78` |
| Buzzer | 256 | 1024 | `App/App_Main.c:25-78` |

“约字节数”是根据 Cortex-M0+ FreeRTOS `StackType_t` 为 32 位的推断；最终占用以 map 文件为准。Safety 周期检查 critical task 的 high-watermark，但非关键任务仍需要压力测试确认。【推断】

- 证据：`App/App_Safety.c:155-211`；`bms24v_platform/MDK-ARM/FreeRTOS/include/FreeRTOSConfig.h:40-49`
- 信心：High
- 人工确认：Needed

`configTOTAL_HEAP_SIZE=32 KiB` 仍保留在配置头中，但动态分配关闭且构建未链接 heap implementation，因此它不是实际可用/已占用的 RTOS heap。后续可删除该失效配置以减少误读，但不能据此声称 RAM 节省了精确 32 KiB，最终仍以 map 为准。【事实 + 推断】

- 证据：`bms24v_platform/MDK-ARM/FreeRTOS/include/FreeRTOSConfig.h:24,40-49`；`CMakeLists.txt:39-118`
- 信心：High
- 人工确认：Not needed

`Int_Log` 静态占用包含 1024 B UART ring 及管理状态；每次 `Int_Log_Printf()` 还在当前任务栈上创建 384 B 固定格式化缓冲，由 `Com_FormatV` 执行无堆限界格式化。这个栈开销已可由源码界定，但与各调用链其他局部变量叠加后的最坏 high-watermark 仍需 HIL 实测。【事实 + Unknown】

- 证据：`Com/Com_Format.c:13-269`；`Int/Int_Log.c:11-24,138-173`；`Int/Int_Log.h:16-41`
- 信心：High
- 人工确认：Needed，task stack high-watermark

## 固件尺寸门禁

CMake 的默认预算为 Flash 120 KiB、RAM 132 KiB；链接后脚本读取 ELF 并在超预算时失败。最终冻结工作区 `gcc-release` 结果为 Flash 68,412/122,880 B、RAM 20,704/135,168 B，并通过 heap/stdio symbol gate。这是当前工作区验证结果，不是已归档的冻结 commit artifact；尺寸余量也不是 stack/WCET 或安全裕量证明。【事实】

- 证据：`CMakeLists.txt:24-27,174-201`；`tests/host/check_size_budget.py:10-34`；`tests/host/check_forbidden_symbols.py:10-50`；验证命令 `cmake --build --preset gcc-release`
- 信心：High
- 人工确认：Not needed

当前 Release 总 Flash/RAM 已知，但本地 build 目录/map 不作为可追溯仓库证据；精确 `.text/.data/.bss`、符号级占用、各任务栈实际 watermark、Engineering 尺寸及冻结 SHA artifact 仍为 Unknown。应由冻结 commit 的 CI artifact 生成并归档。【事实 + Unknown】

## 非易失数据布局

### EEPROM M24C64 逻辑槽

| 数据 | Slot A | Slot B | 完整性 | 证据 |
| --- | ---: | ---: | --- | --- |
| SOH | `0x0000` | `0x0040` | magic `SOH2`、sequence、CRC、写后读回 | `App/App_BatMan_Nvm.c:17-28,37-38,204-306,459-539` |
| SOC | `0x0080` | `0x00C0` | magic `SOC3`、sequence、CRC、写后读回 | `App/App_BatMan_Nvm.c:29-38,300-458` |

新旧槽通过 wrap-safe sequence 选择；无效/CRC 错误槽不被采用。NVM 使用独立静态 recursive mutex 串行化 slot/sequence/重连/flush 状态，并以短 scheduler pause 获取同一时刻的 SOC/SOH/provenance 内存快照；EEPROM 访问不在 scheduler pause 内。【事实】
证据：`App/App_BatMan_Nvm.c:49-175,204-539,713-894`。信心：High（软件结构）。人工确认：Needed，竞争、掉电注入/磨损。

没有发现 RTC 时间戳或关机持续时间随 SOC record 持久化，因此长时间断电后是否仍可把持久 SOC 当作新鲜值为 Unknown。必须定义 freshness 策略或通过可信静置重新收敛。【事实 + Unknown】

### RTC Backup Register Fault Record

`Int_Fault` 使用 TAMP backup register BKP0..4 保存 magic/故障、reset flags、PC、LR 和 checksum，并在启动读取上一条记录。【事实】

- 证据：`Int/Int_Fault.c:73-129,151-187`
- 信心：High
- 人工确认：Needed，IWDG/HardFault/掉电保持测试

## Memory Unknowns

1. 冻结 commit 的 Release/Engineering map 与符号级占用：Unknown；当前冻结工作区 Release 总尺寸为 68,412 B Flash / 20,704 B RAM。
2. ISR/MSP 最坏嵌套深度及 0x400 启动栈裕量：Unknown。
3. critical/non-critical task 在 HIL 压力下的最小 high-watermark：Unknown。
4. EEPROM 实际器件地址、页大小、写周期和 endurance 与 driver 假设一致性：Unknown。
5. RTC backup domain 在目标上电/掉电拓扑中的保持行为：Unknown。
