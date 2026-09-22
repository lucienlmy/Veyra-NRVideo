# 统一修复计划执行记录（2026-09-22）

分支 `codex/fg-independent-repair-20260922`，实施提交 `e11aa56`，标签
`checkpoint/plan-b1-pre-20260922`（开工）与 `checkpoint/plan-b6-done-20260922`（完成）。
计划见 [统一修复计划](UNIFIED_REPAIR_PLAN_2026-09-22.md)。本机 RTX 5070、100 Hz
显示器；所有数字为 30 s 短测或单元/合同测试，长测、实卡、多显示器、HDR 屏、肉眼画质
由用户验收。运行入口 `E:/项目/Veyra/tests/fg-independent-repair-20260922/app/veyra.exe`
（第 7 批后 SHA256 前 16 位 `703B9D073AD28197`，staging，运行库为 junction，非便携包）。
产物：`E:/项目/Veyra/tests/fg-independent-repair-20260922/{b12,b3456}/`，构建日志
`E:/项目/Veyra/logs/fg-independent-repair-20260922/build-b*.log`。

## 1. 逐项状态

| 编号 | 状态 | 备注 |
| --- | --- | --- |
| **第 1 批** | | |
| D1 | 已修 | `statusBar` 删除；字幕快捷键/自动对齐/关闭进度走 `showToast` 到底栏 `ColourStatus`（3 s 后恢复设置消息）；日常模式无文件或失败时 `EmptyTitle/EmptyHint` 显示并含失败原因 |
| D3 | 已修 | 状态宽度 `min(440,max(120,tw/3))`，标题宽度相应减去 |
| D4 | 已修（改方案） | 窄于 960 px 时截图按钮移入左侧竖栏 y=372，抽屉按钮保持 `w-384`；原"右移抽屉"方案在 720 px 下仍与模式切换重叠 |
| D5 | 已修 | `Master` 分支检查 `requestSettings` 返回值并还原 |
| D7 | 已修 | `windowTimer` 首行在弹窗打开时返回 |
| D11 | 已修 | 状态 sink 走 `setText` 去重；`message()` 记上次文本 |
| D15a | 已修 | 最大化按钮真最大化；Ctrl+V 不再触发 |
| B12 | 已修 | 删除 4 行空 "skipped" 日志 |
| **第 2 批** | | |
| D2 | 已修 | 诊断面板不可见时不刷新；刷新前后恢复首行与选区 |
| D6 | 已修 | `alignSubtitleToAudio` 增加 `stop_token`；jthread lambda 接 token；`WM_DESTROY` `request_stop()` |
| D8 | 已修 | `WM_ENTERSIZEMOVE` 起 16 ms 合并 layout；`SetWindowRgn` 仅尺寸/模式变化时重建；去 `RDW_UPDATENOW` |
| D9 | 部分 | `seekPreview.tick` 复用 tick 快照；CapturePanel 三个 getter 只在面板创建时调用，非每 tick，无需改 |
| D10 | 已修 | 音量滑块判等 |
| D12 | 已修 | 呈现设置保存 1 s 合并（`PreferenceSaveTimer`），关窗时立即保存；折叠状态掩码不变不写 |
| D13 | 已修 | `cachedChromeFont` 按 (dpi,size,weight) 缓存 |
| D14 | 已修 | 字幕 DIB 线程局部缓存，仅尺寸变化时重建 |
| D15b | 部分 | CapturePanel 未完成的枚举 future 在销毁时交给 detached 线程；RemotePlayPanel 的 worker 已接 `stop_token`，未改 |
| E4 | 已修 | 导出管理器析构等待 1 s |
| **第 3 批** | | |
| C1 | 已修 | 4 个 `static const std::regex`；提取在锁外，锁内只更新 `latestProblem_`/历史 |
| C3 | **未做** | 采集回调三缓冲锁外复制涉及 mailbox 所有权重构，本轮未动；见 §4 |
| C4 | 未做 | 同上 |
| C5 | 已修 | `PcmQueue` 连续存储 + 头游标，`pull` 用 `memcpy` |
| E1 | 已修 | NVENC 成功日志前 24 次，之后仅失败 |
| E5 | 未做 | 每秒日志合并未做（分析脚本依赖现有行格式） |
| 延迟 4 | 已修 | owner 线程 `MmcssScope("Pro Audio")`；采集回调线程 thread_local 注册 |
| **第 4 批** | | |
| B4 | 已修 | `pts` → `best_effort_timestamp` → `pkt_dts`，每 300 次记一条日志 |
| B5 | 未做 | 中途硬解→软解回退需要 seek 回位与历史重置，本轮未动 |
| C2 | 已修 | 软解线程 `clamp(cores/2,2,4)` |
| C6 | 已修 | `IMMNotificationClient`，默认渲染端点变化置 `AUDCLNT_E_DEVICE_INVALIDATED` 走现有恢复 |
| C7 | 未做 | 格式变化重建 resampler 未动 |
| E3 | 已修 | 2 s GPU 超时设置具体原因 |
| **第 5 批** | | |
| A1 | 已修 | HDR 查询缓存 `IDXGIFactory1`，`IsCurrent()` 失效时重建 |
| 延迟 2 | 未做 | owner 唤醒改事件未动（`waitLive` 仍 1 ms 定时器切片） |
| 延迟 3b | 未做 | 依赖 C3 |
| A2 | **已撤回** | upload fence 前移到 FG 之前后，`veyra_fg_presentation_tests` 报 49 条跨队列 barrier 错误（`uploadFences_` 同时是 DLSS 呈现队列的 handoff fence，必须覆盖 FG 列表把 `videoFrame_` 转回 COMMON）。恢复原位并加注释 |
| 延迟 5 | 已修 | `LivePairLatency` 余量 1 ms→0.5 ms，步长 0.25→0.5 ms；单测常数同步 |
| A6+B3 | 已修 | `snapshot()` 拷贝后释放引擎互斥再取 flow；flow 快照 50 ms 缓存 |
| A4+A5 | 未做 | 每帧诊断上下文与直方图缓冲未动 |
| A7 | 已修 | 命令槽时间戳回读仅 `VEYRA_PROFILE_SLOTS` 或详细日志下 |
| B7 | 已修 | `fgDisableReadback_` 常驻 Map |
| A8 | 已修 | 暂停轮询 50 ms |
| 延迟 1 | 未做 | 专业模式圆角是否 Composed 的测量需要 PresentMon 权限或用户实屏，未做 |
| **第 6 批** | | |
| B2 | 已修 | `SetUnhandledExceptionFilter` 写 minidump 并 flush；`VEYRA_TEST_RAISE_SEH=1` 验证：日志末尾完整，dmp 255 KB |
| B1 | 未做 | step lambda 状态收敛属重构，本轮未动 |
| B6 | 撤回 | `recovering()` 有单测使用，保留 |
| C8/C9/E6 | 未做 | 注释级项未动 |

## 2. 门槛测试结果

| 测试 | 结果 | 日志 |
| --- | --- | --- |
| `veyra_ui_contract_tests` | PASS（384 布局用例） | `b12/` |
| `veyra_popup_selector_tests` | 19 PASS | `b12/` |
| `veyra_subtitle_panel_tests` | 31 PASS | `b12/` |
| `veyra_subtitle_overlay_tests` | 18 PASS | `b12/` |
| `ui-fg-backends.py` | exit 0 | `b12/ui-fg-backends/` |
| `ui-layout-dpi.py` | **exit 1，"status detail cannot scroll"** | 基线 exe（`bounded-repair` app）同样失败同一断言，属本机既有状态，非本轮引入；`header controls overlap` 断言在 D4 改方案后通过 |
| `veyra_presentation_worker_tests` | 127 PASS | `b3456/` |
| `veyra_repair_contract_tests` | 205 PASS | `b3456/` |
| `veyra_capture_audio_tests` | 18 PASS | `b3456/` |
| `veyra_fg_presentation_tests` | exit 0，D3D12 errors=0（A2 撤回后） | `b3456/fg-presentation.out` |
| `veyra_fg_settings_tests backend-switch` | 16 PASS（最终 exe） | `b3456/settings2.log` |
| `veyra_fg_admission_tests dlss 4` | pass=1，debugErrors=0 | `b3456/admission.log` |
| SEH 注入 | minidump 写出，日志 flush | `b3456/seh.log`，`veyra-crash-*.dmp` |
| 8 s 文件音频 smoke | exit 0，0 ERROR | `b3456/audio-smoke.log` |
| `veyra_source_tests` | **未运行**：corpus 需 `veyra_clip_gen`，本机 FFmpeg 无 libopenh264 | `b3456/clipgen.log` |
| `veyra_audio_timeline_tests` 等 3 个 | 未运行（需参数，本轮未查） | — |

## 3. FG 短测（30 s，全屏，`b3456/ab-final/`，与本日早些 `ab4`/`ab3` 同构建族对照）

| 工况 | 指标 | 本轮 | 之前（第 0 批后） |
| --- | --- | ---: | ---: |
| SR+NR XeSS 4X | 源/s | 49.9 | 45.4 |
| | 生成/s | 149.7 | 136.1 |
| | 原帧间隔 p95 / max | 23.0 / 24.8 | 26.5 / 27.5 |
| | `gpuReadyP95` | 20.8 | 23.2 |
| 原生 XeSS 4X | 源/s | 60.0 | 58.5 |
| | Present 阻塞 p50 | **0.73** | 11.5 |
| | `gpuReadyP95` | 15.8 | 18.0 |
| 原生 DLSS 6X | 提交/s | 269.9 | 237.1 |
| | 间隔 >16.67 ms | **0** | 3 |
| | 间隔 p95 / max | 8.44 / 8.96 | 8.53 / 16.94 |
| | reduced 对 | 574 | 814 |
| SR+NR DLSS 6X | 提交/s | 140.3 | 115.7 |
| | 间隔 >16.67 ms | 118 | 155 |
| | reduced 对 | 623 | 269 |
| 原生 DLSS 2X | 提交/s | 120.0 | 120.0 |
| | 间隔 p95 | 8.76 | 8.78 |

所有工况 `errorLines=0`，`notDisplayed` 分布与刷新率一致。原生 XeSS 4X 的 Present 阻塞
从 11.5 ms 降到 0.73 ms 是本轮最大的意外收益，最可能来自 MMCSS 让提供方节奏线程与
owner 不再互相抢占；DLSS 6X 提交率 237→270 与 SR 6X 116→140 同理。这些是单次 30 s
短测，用户长测确认前不计入。

## 3b. 第 7 批（补做未完成项，提交 `6d019dc`，标签 `checkpoint/plan-b7-done-20260922`）

| 编号 | 状态 | 备注 |
| --- | --- | --- |
| C3 | 已修 | 原生采集回调第三帧 staging：锁内认领、锁外复制、锁内指针交换发布；`tryRead` 不再被复制阻塞 |
| 延迟 2 | 已修 | 采集源暴露自动重置事件；owner `waitLive` 改 `WaitForMultipleObjects{事件,定时器}` |
| B5 | 已修 | 中途硬解失败：重开软解、`seekToUs` 回上次 PTS、置 Seek 标志与新 epoch |
| C7 | 已修 | 采样率/采样格式变化重建 resampler；仅声道布局变化才停流 |
| A4/A5 | 已修 | 每帧诊断上下文复用成员，字符串仅变化时更新；亮度采样与直方图缓冲复用 |
| E6 | 已修 | 命令槽 GPU 时长记录满 16384 后环形覆盖 |
| C9 | 已修 | 硬解路径 EAGAIN 缓冲上限 8（原 16），避免占满解码池 |
| C8 | 注释 | Receive 锁范围说明 |
| C4、延迟 3b、E5、B1 | 未做 | 压缩 payload 池、直写 upload 堆、每秒日志合并、step lambda 收敛 |

门槛：scheduler 127、修复合同 205、采集音频 18、FG 呈现 D3D12 errors=0、backend-switch 16、
文件 NR+DLSS2 10 s smoke 463 帧 0 错误；实卡 MCS 4K--T800 4K30 rate test PASS（callback
29.97）；25 s 采集 NR+DLSS4X 新/旧 exe 对照：`callbackToPresentReturnP95` 42.7 vs 42.5 ms，
`gpuReadyP95` 10.6 vs 10.6，dropped 0/0，expired 2/2。**C3 与事件唤醒在本工况没有可测收益**：
FG 下延迟由相位等待主导，且复制本就与 GPU 工作重叠；收益（若有）要在无 FG 或 1440p60
工况用 `test-capture-version-comparison.ps1` 120 s 测，由用户执行。注意 `capture-callback`
日志的 `entryToLockMs` 现在包含锁外复制时间，与旧版数值不可直接比较。

## 4. 未做项的原因与下一步

- **C3/C4/延迟 3b（采集三缓冲、锁外复制、直写 upload 堆）**：需要重构 mailbox 所有权
  （驱动写/复制/owner 读三方），并与 D3D12VA 硬件路径共存；改动面大，需实卡回归，
  本轮不冒进。建议单独一批，实卡 A/B 为门槛。
- **B5（中途硬解回退）**：需要保存位置、重开软解、seek 回位并触发历史重置；单独一批。
- **延迟 2（owner 唤醒改事件）**：`waitLive` 与 `LiveGpuScheduler` 的 wakeAt 绑定，改为
  `WaitForMultipleObjects` 需要采集源暴露事件句柄；与 C3 一起做。
- **延迟 1（专业模式 Composed 测量）**：需 PresentMon 权限或用户实屏，请用户在长测时
  对比全屏与专业模式的 `display-stats`。
- **E5、A4/A5、B1、C7、C8/C9/E6**：低收益或重构性质，保留在清扫文档待办。

## 5. 撤回记录

- A2（upload fence 前移）：跨队列 barrier 错误 49 条，撤回，原因已写入代码注释。
- B6（删 `recovering()`）：单测使用，撤回。
- D4 原方案（抽屉右移）：720 px 仍重叠，改为截图按钮入竖栏。

## 6. 用户长测请看

1. `b3456/ab-final` 的收益是否在 120 s 与实卡上复现；重点 XeSS 原生 4X 的 Present 阻塞
   和 DLSS 6X 的零长空档。
2. 字幕快捷键、失败原因 toast、窄窗口截图按钮位置、弹出下拉不再自动关闭。
3. 拖边框、中键拖动缩放（字幕开启时）、诊断面板滚动。
4. 播放中拔插耳机是否在 1 s 内恢复。
5. 长测中若崩溃，`logs/` 下应有 `veyra-crash-*.dmp` 且日志末尾完整。
