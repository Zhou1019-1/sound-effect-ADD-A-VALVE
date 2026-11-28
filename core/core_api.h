// ============================================================================
//  core/core_api.h — 核心层对界面层的唯一契约
// ----------------------------------------------------------------------------
//  拆分之后：`AddAValveCore` 是库，`AddAValve` 是界面。界面只允许通过下面这几类
//  接口碰核心层；反过来核心层**不许** include 任何 src/app 或 src/ui 的东西
//  （实测核心层 13 个文件里 GUI 符号 0 处，这条是机器可查的，不是口头约定）。
//
//  为什么值得拆：
//    · 无界面也能跑：批量处理、自动化测试、CI 冒烟测试都不需要开窗口；
//    · 依赖面收敛：核心层只需要 juce_audio_{basics,devices,formats} + juce_dsp +
//      juce_data_structures，**不需要** juce_gui_basics / juce_gui_extra / juce_graphics。
//      核心层的 JUCE 聚合头（cmake/JuceHeaderCore.h）就只列这些模块，多一个都进不来。
//
//  接线方式（不做任何代码搬运，只加一个目标）：
//    target_link_libraries(AddAValve PRIVATE AddAValveCore)
//  `src/` 与 `core/` 是 AddAValveCore 的 PUBLIC include 目录，所以界面层原有的
//  include 写法（"../model/AudioDocument.h" 等）保持不变；而本文件里的 include
//  一律写成 <model/...>、<audio/...>，不依赖本文件所在的目录层级 —— 放在仓库外
//  或换一层目录都不会失效。
//
//  自足性由 tests/CoreApiSmoke.cpp 守着：那个目标和这里一样只 include 本文件，
//  不生成自己的 JuceHeader.h、不直接链任何 juce:: 模块，一切依赖都必须来自
//  AddAValveCore 的 PUBLIC 接口。这里少一条路径，冒烟目标就编不过。
//
//  注：把音色处理整体替换成外部引擎（不再编译 src/dsp/ValveProcessor.cpp）是另一
//  件事，单独一个 PR 提交，不混在这里 —— 本 PR 只做「GUI 与核心层分离」，内置 DSP
//  保持原样。
// ============================================================================
#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>

#include <dsp/ValveParameters.h>
#include <dsp/ValveProcessor.h>
#include <model/AudioDocument.h>
#include <model/PresetStore.h>
#include <audio/Metering.h>
#include <audio/PlaybackEngine.h>
#include <audio/AudioExporter.h>

namespace addavalve::core
{

// ---- 1) 文档与编辑（纯内存，可脱离音频设备使用）--------------------------
//   AudioDocument        : 帧/采样率/声道、选区、撤销重做、裁剪/删除/淡变/增益
//   AudioDocument::slice : 导出用的区间副本
//   典型用法：加载 → 编辑 → 交给 AudioExporter 落盘，全程不需要 GUI/设备。

// ---- 2) 音色处理 ---------------------------------------------------------
//   ValveProcessor       : prepare / setParameters / process(AudioBuffer) / reset
//   getLatencyInSamples  : 分数延迟（44.1 kHz 下 59.5 样本），宿主必须用它补偿干路
//   ValveParameters      : drive / character / warmth / mix / output

// ---- 3) 播放（实时路径，只碰 juce_audio_devices）-------------------------
//   PlaybackEngine       : 设备管理、AudioTransportSource、A/B 旁通、延迟补偿
//   注意：它不碰任何界面，所以「无界面渲染」可以完全不启动它。

// ---- 4) 计量 -------------------------------------------------------------
//   Metering             : rms / peak / clip，按 FIFO 交给界面消费
//   MeterPanel（界面层）只负责把 Metering 的结果画出来。

// ---- 5) 导出 -------------------------------------------------------------
//   AudioExporter        : 后台渲染 + 进度 + 取消 + 回读校验；格式由 juce_audio_formats 提供
//   它是「文件到文件」的完整链路，可以在 console 目标里直接调用。

/** 库版本 —— 与 CHANGELOG 同步。 */
inline constexpr const char* coreVersion() noexcept { return "0.1.0"; }

} // namespace addavalve::core
