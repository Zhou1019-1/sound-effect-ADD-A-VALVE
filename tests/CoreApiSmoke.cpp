// ============================================================================
//  tests/CoreApiSmoke.cpp — 公共头冒烟测试
// ----------------------------------------------------------------------------
//  本文件**只** include <core_api.h> 一个头。不 include <JuceHeader.h>，也不直接
//  链任何 juce:: 模块 —— 目标 AddAValveCoreApiSmoke 的依赖全部来自 AddAValveCore
//  的 PUBLIC 接口。所以它同时验证三件事：
//
//    1. core/core_api.h 里的 include 路径（<model/...>、<audio/...>、<dsp/...>）
//       在「只有核心库的 include 目录」时也能解析 —— 也就是仓库外使用者能编译；
//    2. 核心层的 JUCE 聚合头（cmake/JuceHeaderCore.h）覆盖了核心层真正用到的模块，
//       不再依赖 GUI 目标生成的那一份；
//    3. 核心层在**没有** juce_gui_basics / juce_graphics 的情况下就能跑起来：下面这段
//       会真的 prepare 一个 ValveProcessor 并处理一块音频，全程不开窗口、不碰设备。
//
//  任何一条不成立，这个目标就编不过或返回非 0，CI 直接红。
// ============================================================================

#include <core_api.h>

#include <cmath>
#include <cstdio>
#include <vector>

int main()
{
    std::printf("core_api.h smoke test\n");
    std::printf("  coreVersion          = %s\n", addavalve::core::coreVersion());

    // ---- 1) 核心层可以在无 GUI 的 console 目标里直接实例化 ----------------
    ValveProcessor processor;
    ValveParameters parameters;                    // drive/character/warmth/mix/output 默认值
    parameters.drive = 0.35f;
    parameters.character = 0.4f;
    parameters.warmth = 0.5f;

    constexpr double sampleRate = 48000.0;
    constexpr int    blockSize  = 512;
    constexpr int    channels   = 2;

    processor.prepare(sampleRate, blockSize, channels);
    processor.setParameters(parameters);

    // ---- 2) 延迟契约（宿主靠它补偿干路）----------------------------------
    const auto latency = processor.getLatencyInSamples();
    std::printf("  getLatencyInSamples  = %.1f\n", latency);
    if (!(latency >= 0.0f))
    {
        std::printf("FAIL: latency 非法\n");
        return 1;
    }

    // ---- 3) 处理一块 440 Hz 正弦，输出必须有限且有能量 --------------------
    juce::AudioBuffer<float> buffer(channels, blockSize);
    for (int ch = 0; ch < channels; ++ch)
    {
        auto* data = buffer.getWritePointer(ch);
        for (int i = 0; i < blockSize; ++i)
            data[i] = 0.5f * std::sin(2.0f * juce::MathConstants<float>::pi * 440.0f
                                      * static_cast<float>(i) / static_cast<float>(sampleRate));
    }

    processor.process(buffer);

    double peak = 0.0;
    for (int ch = 0; ch < channels; ++ch)
    {
        const auto* data = buffer.getReadPointer(ch);
        for (int i = 0; i < blockSize; ++i)
        {
            const auto v = static_cast<double>(data[i]);
            if (!std::isfinite(v))
            {
                std::printf("FAIL: 输出含非有限值 ch=%d i=%d\n", ch, i);
                return 1;
            }
            peak = juce::jmax(peak, std::abs(v));
        }
    }
    std::printf("  processed peak       = %.6f\n", peak);
    if (!(peak > 0.0))
    {
        std::printf("FAIL: 输出全零\n");
        return 1;
    }

    std::printf("OK: 核心层在无 GUI 依赖下可用\n");
    return 0;
}
