# 07 Build / Config Matrix

更新日期：2026-08-22

## Build/Config Matrix

| 配置 | 工具链/生成器 | 优化 | `BMS_ENGINEERING_BUILD` | 输出/用途 | 证据 | 当前验证 |
| --- | --- | --- | ---: | --- | --- | --- |
| `gcc-debug` | arm-none-eabi-gcc + Ninja | `-Og -g3` | ON | 调试 ELF/HEX/BIN/map | `CMakePresets.json:5-13`；`CMakeLists.txt:141-176` | clean build 通过；CTest 4/4 |
| `gcc-release` | arm-none-eabi-gcc + Ninja | `-Os -g0` | OFF | 量产候选；CLI implementation 不可用 | `CMakePresets.json:16-24`；`App/App_DebugCli.c:569-589` | clean build 通过；CTest 4/4；Flash 68,412/122,880 B，RAM 20,704/135,168 B；symbol gate 通过 |
| `gcc-engineering` | arm-none-eabi-gcc + Ninja | `-Os -g0` | ON | 受物理 gate/unlock 限制的工程固件 | `CMakePresets.json:27-35`；`App/App_DebugCli.c:236-264,446-496` | clean build 通过；CTest 4/4 |
| `host-msvc` | MSVC x64 | host Debug | N/A | SOC/SOH/formatter 算法 + 四个 Python 结构/卫生契约 | `CMakePresets.json:38-75`；`CMakeLists.txt:3-16` | Host CTest 5/5 |
| GitHub host job | Ubuntu GCC native + Ninja | Debug | N/A | 5 个 host tests | `.github/workflows/firmware-ci.yml:8-19`；`tests/host/CMakeLists.txt:1-55` | Workflow 配置存在；远端 run Unknown |
| GitHub gcc matrix | Ubuntu arm-none-eabi-gcc | 三 preset | 按 preset | 三固件构建 + 各 4 项 Python 契约 | `.github/workflows/firmware-ci.yml:21-36`；`tests/host/CMakeLists.txt:1-27` | Workflow 配置存在；远端 run Unknown |
| Keil target | MDK-ARM V5.32 / Cortex-M0+ | 工程设置 | 工程宏需核对 | `.axf/.hex` | `bms24v_platform/MDK-ARM/bms24v_platform.uvprojx:10-21,343-851` | 本轮 ARMCC 构建 Unknown |

## 编译与链接门禁

GCC 固件使用 C11、Cortex-M0+、`-Wall -Wextra -Werror`，函数/数据 section + `--gc-sections`；链接生成 map/hex/bin，执行尺寸脚本，并在每次固件链接后用 `nm` 拒绝 heap/stdio 符号、强制要求有界 `Com_FormatV` 和 `Int_Log_Printf`。【事实】

- 证据：`CMakeLists.txt:18-35,134-201`；`tests/host/check_forbidden_symbols.py:10-50`
- 信心：High
- 人工确认：Not needed

尺寸默认门槛：Flash 122,880 B（120 KiB），RAM 135,168 B（132 KiB）；脚本通过 `arm-none-eabi-size` 解析并超限失败。【事实】

- 证据：`CMakeLists.txt:24-27,180-190`；`tests/host/check_size_budget.py:10-34`
- 信心：High
- 人工确认：Not needed

## Host Test Matrix

| CTest 名称 | 类型 | 覆盖点 | 注册证据 | 能否替代 HIL |
| --- | --- | --- | --- | --- |
| `platform_contract` | Python 源码结构契约 | HAL 前 SC safe、静态 RTOS、Fault/Watchdog、Release CLI、NVM mutex/snapshot、构建约束 | `tests/host/CMakeLists.txt:1-6`；`tests/host/test_platform_contract.py:45-226` | 否 |
| `repo_hygiene` | Python Git index 契约 | 拒绝已追踪 build/cache/log/`.log.lock`/binary/pack/IDE-user artifact | `tests/host/CMakeLists.txt:8-13`；`tests/host/check_repo_hygiene.py:11-105` | 否 |
| `bq_safety_contract` | Python 源码结构契约 | BQ transaction/deadline/完整帧/FET epoch、ALERT sequence、PSTOP-first proof 失效、1500 ms 整体重认证与 offline 全关 | `tests/host/CMakeLists.txt:15-20`；`tests/host/test_bq_safety_contract.py:35-242` | 否 |
| `power_soc_contract` | Python 源码结构契约 | Power 唯一授权、wake provenance、temperature/anchor、SC event sequence/ratio/limit 约束 | `tests/host/CMakeLists.txt:22-27`；`tests/host/test_power_soc_contract.py:38-112` | 否 |
| `algorithm_unit` | 原生 C 单元测试 | OCV monotonic、rest seed、coulomb/NVM、anchor guards/timeouts、SOH、有界无堆整数格式化/截断 | `tests/host/CMakeLists.txt:29-55`；`tests/host/test_algorithms.c:71-429` | 否 |

`algorithm_unit` 仅在 `BMS_HOST_TESTS_ONLY=ON` 时编译；三个 cross preset 的 CTest 运行四个 Python 合同/卫生测试，不会交叉运行 native 算法 executable。Host 配置覆盖全部五项。最终本地结果是 Host 5/5、三套 clean ARM 各 4/4。【事实】

- 证据：`CMakeLists.txt:3-16,203-205`；`tests/host/CMakeLists.txt:1-55`；`.github/workflows/firmware-ci.yml:8-36`
- 信心：High
- 人工确认：Not needed

## Keil / GCC 一致性

Keil 工程目前包含 App/Com/Int、Safety/Fault/I2C2Bus/Log/Watchdog 和 FreeRTOS source group；CMake 同样显式列出这些源文件且不含 `heap_4.c`。【事实】

- 证据：`bms24v_platform/MDK-ARM/bms24v_platform.uvprojx:343-851`；`CMakeLists.txt:39-118`
- 信心：High
- 人工确认：Needed，每次新增模块做双清单 diff

存在以下未闭环差异风险：【Unknown】

1. Keil 的宏、优化、链接 scatter 与 GCC Release 是否完全等价。
2. ARMCC/armclang 对 HardFault naked trampoline、static assert、warning 的编译行为。
3. Keil 实际尺寸和 map 符号占用。

因此 GCC 通过不能自动声明 Keil 通过。

## Release Gates

| 门禁 | 状态 | 证据/缺口 |
| --- | --- | --- |
| 所有 GCC preset configure/build | 可自动执行 | `CMakePresets.json:5-65` |
| `-Werror` | Closed in config | `CMakeLists.txt:141-153` |
| Flash/RAM budget | Closed in config | `CMakeLists.txt:180-190` |
| Release heap/stdio symbol gate | Closed in config and current build | `CMakeLists.txt:194-201`；`tests/host/check_forbidden_symbols.py:10-50`；当前 `gcc-release` 输出 `symbol gate: no heap/stdio symbols; bounded log formatter present` |
| Host/ARM CTest | Closed in current workspace | Host 5/5；三套 clean ARM 各 4/4；注册证据 `tests/host/CMakeLists.txt:1-55` |
| Repository hygiene | Software closed | `tests/host/check_repo_hygiene.py:11-105`；tracked forbidden=0；本交付版本已退跟踪 1098 项历史产物 |
| Host tests 纳入 CI | Closed in config | `.github/workflows/firmware-ci.yml:8-19` |
| Release CLI exclusion | Closed in source/config | `CMakePresets.json:16-24`；`App/App_DebugCli.c:569-589` |
| Keil build | Unknown | 无本轮可复核构建日志 |
| Changed-C clang analyzer | Passed in current workspace | 30 个变更 C 文件通过；需归档命令、版本和日志 |
| clang-tidy/MISRA/CERT | Unknown | 未发现适用标准、waiver 或正式报告 |
| 覆盖率/MC-DC | Unknown | 未发现覆盖报告 |
| HIL/故障注入 | HIL blocked | 软件/host 测试不覆盖真实 gate、电流、温升、复位和总线波形 |
| CI 远端绿色 run + commit SHA | Unknown | workflow 文件存在不等于远端已通过 |

## Repository Hygiene

`.gitignore` 覆盖 build、logs、`.log.lock`、对象、elf/axf/hex/bin/map、pack/cache 和临时文件；`check_repo_hygiene.py` 对 Git tracked path 进行 fail-closed 审计并注册到 CTest。最终 tracked forbidden artifact=0；本交付版本共退跟踪 1098 个历史产物，其中需保留的本地二进制/lock 仅从 index 移除并由 ignore 覆盖。【事实】

- 证据：`.gitignore:1-41`；`tests/host/check_repo_hygiene.py:11-105`；`tests/host/CMakeLists.txt:8-13`；`git diff --cached --name-status`
- 信心：High
- 人工确认：Needed
