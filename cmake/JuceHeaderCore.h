// ============================================================================
//  cmake/JuceHeaderCore.h
// ----------------------------------------------------------------------------
//  `AddAValveCore`（静态库）专用的 JUCE 聚合头。CMake 在 configure 阶段把它原样
//  复制到 <build>/AddAValveCore_artefacts/JuceLibraryCode/JuceHeader.h。
//
//  为什么不用 juce_generate_juce_header(AddAValveCore)：
//    · 那个函数只对 juce_add_* 创建的二进制目标成立，而且它生成的 JuceHeader.h 依赖
//      juceaide 宿主工具，在只构建核心库（无 GUI）的场景里没必要引入这层依赖；
//    · 核心层的模块集合是固定的（下面 7 个），写成静态文件可 diff、可审阅。
//
//  为什么把高级模块排除在外：
//    核心层源码里 GUI 符号 0 处（Component / Graphics / Colour / Timer 一个都没有）。
//    这里只列下面 7 个模块，等于从编译期就堵住「核心层偷偷用 GUI 类型」这条路——
//    一旦有人在 src/model 里写了 juce::Component，这里立刻编不过。
//
//  为什么不会遮蔽 GUI / 测试目标的 JuceHeader.h：
//    那两个目标仍然各自 juce_generate_juce_header()，生成在自己的 PRIVATE include
//    目录里；CMake 排编译命令行时，目标自身的 include 目录排在依赖接口目录之前，
//    所以它们永远命中自己那份。核心层这份只在核心库自身、以及只链接核心库的
//    纯粹消费者（tests/CoreApiSmoke.cpp）里生效。
// ============================================================================
#pragma once

#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>
#include <juce_data_structures/juce_data_structures.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_dsp/juce_dsp.h>
