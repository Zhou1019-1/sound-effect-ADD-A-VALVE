// ============================================================================
//  external/engine_adapter.cpp — ValveProcessor 的外接引擎实现
// ----------------------------------------------------------------------------
//  开启 -DADDAVALVE_EXTERNAL_DSP=ON 时：
//    · 不再编译 src/dsp/ValveProcessor.cpp；
//    · src/dsp/ValveProcessor.h 里私有区换成 pimpl（`struct EngineState;`），
//      公开接口一个字符都没动 —— 所以 src/audio、src/model、src/ui、tests 的调用点
//      全部零改动，`tests/ValveProcessorTests.cpp` 也会直接测到这个外接实现；
//    · 本文件实现那 5 个方法 + 析构，内部转调 addavalve.h 的 C ABI。
//
//  语义对齐（与内置实现逐条对应，不是"差不多"）：
//    prepare(sr, block, ch)   → aav_create / aav_prepare。声道数变化必须重建实例，
//                               因为 aav_prepare 只换采样率与块大小。
//    setParameters(params)    → 先按内置实现的规则钳位（drive/character/warmth/mix
//                               到 [0,1]，output 到 [-24,+24] dB）再 aav_set_params；
//                               未 prepare 时只缓存不转发（内置实现同样只是缓存）。
//    reset()                  → aav_reset
//    process(buffer)          → 交织到 aav_scratch 给的中转缓冲，aav_process_interleaved，
//                               再解交织写回。超过 maxBlock 的块自动切片，逐片调用；
//                               20 ms 参数平滑在引擎内部按样本推进，切片不会改变结果。
//    getLatencyInSamples()    → aav_latency（分数样本，宿主用它补偿干路）
//
//  许可：AGPL-3.0-or-later（见仓库 LICENSE / THIRD_PARTY_NOTICES.txt）。
// ============================================================================

#include "../src/dsp/ValveProcessor.h"

#include <addavalve.h>

#include <algorithm>
#include <memory>

// ---------------------------------------------------------------------------
//  每个 ValveProcessor 对应的引擎实例状态。
// ---------------------------------------------------------------------------
struct ValveProcessor::EngineState
{
    int              handle    = -1;     // aav_create() 返回的句柄，-1 = 未创建
    int              channels  = 0;      // 已创建的声道数
    int              block     = 0;      // 交给引擎的最大块（已钳到引擎允许范围）
    float*           scratch   = nullptr; // 交织中转缓冲（引擎所有）
    int              scratchFloats = 0;
    ValveParameters  parameters {};      // 最近一次 setParameters 的钳位结果
    bool             prepared  = false;
};

namespace
{
    // addavalve 的 C ABI 明确要求 16 ≤ maxBlockSize ≤ 8192（越界直接 AAV_INVALID_PARAMS）。
    // 宿主声明的块可能更大（他的测试里就有 prepare(…, 16384, 1)）或更小，所以这里把
    // 交给引擎的块钳进区间，process() 再按这个块切片。逐样本的参数平滑在引擎内部推进，
    // 切片对结果没有影响。
    constexpr int kEngineBlockMin = 16;
    constexpr int kEngineBlockMax = 8192;

    int engineBlockFor (int requested) noexcept
    {
        return std::min (kEngineBlockMax, std::max (kEngineBlockMin, requested));
    }
}

ValveProcessor::ValveProcessor() = default;

ValveProcessor::~ValveProcessor()
{
    if (engine != nullptr && engine->handle >= 0)
        aav_destroy (engine->handle);
}

void ValveProcessor::prepare (double sampleRate, int samplesPerBlock, int numChannels)
{
    if (engine == nullptr)
        engine = std::make_unique<EngineState>();

    auto& state = *engine;

    const int    channels = std::min (2, std::max (1, numChannels));
    const int    block    = engineBlockFor (samplesPerBlock);
    const double rate     = sampleRate > 0.0 ? sampleRate : 48000.0;

    if (state.handle < 0 || channels != state.channels)
    {
        if (state.handle >= 0)
            aav_destroy (state.handle);

        state.handle   = aav_create (channels, rate, block);
        state.channels = channels;
        state.scratch  = nullptr;
        state.scratchFloats = 0;
    }
    else
    {
        aav_prepare (state.handle, rate, block);
    }

    state.block    = block;
    state.prepared = state.handle >= 0;

    if (state.prepared)
    {
        state.scratch = aav_scratch (state.handle, block * channels);
        state.scratchFloats = state.scratch != nullptr ? block * channels : 0;

        // 与内置实现同序：prepare 末尾重新下发一次参数，然后 reset。
        setParameters (state.parameters);
        aav_reset (state.handle);
    }
}

void ValveProcessor::reset()
{
    if (engine != nullptr && engine->handle >= 0)
        aav_reset (engine->handle);
}

void ValveProcessor::setParameters (const ValveParameters& newParameters)
{
    if (engine == nullptr)
        engine = std::make_unique<EngineState>();

    auto& p = engine->parameters;
    p.drive     = std::min (1.0f, std::max (0.0f, newParameters.drive));
    p.character = std::min (1.0f, std::max (0.0f, newParameters.character));
    p.warmth    = std::min (1.0f, std::max (0.0f, newParameters.warmth));
    p.mix       = std::min (1.0f, std::max (0.0f, newParameters.mix));
    p.output    = std::min (24.0f, std::max (-24.0f, newParameters.output));

    if (engine->handle >= 0)
        aav_set_params (engine->handle, p.drive, p.character, p.warmth, p.mix, p.output);
}

float ValveProcessor::getLatencyInSamples() const
{
    return (engine != nullptr && engine->handle >= 0) ? aav_latency (engine->handle) : 0.0f;
}

void ValveProcessor::process (juce::AudioBuffer<float>& buffer)
{
    if (engine == nullptr || ! engine->prepared)
        return;

    auto& state = *engine;

    const int frames = buffer.getNumSamples();
    if (frames <= 0 || buffer.getNumChannels() != state.channels)
        return;

    if (state.scratch == nullptr || state.scratchFloats < state.block * state.channels)
    {
        state.scratch = aav_scratch (state.handle, state.block * state.channels);
        state.scratchFloats = state.scratch != nullptr ? state.block * state.channels : 0;
        if (state.scratch == nullptr)
            return;
    }

    const int channels = state.channels;
    auto* const scratch = state.scratch;

    for (int offset = 0; offset < frames; offset += state.block)
    {
        const int n = std::min (state.block, frames - offset);

        for (int i = 0; i < n; ++i)
            for (int c = 0; c < channels; ++c)
                scratch[i * channels + c] = buffer.getReadPointer (c)[offset + i];

        aav_process_interleaved (state.handle, scratch, n);

        for (int i = 0; i < n; ++i)
            for (int c = 0; c < channels; ++c)
                buffer.getWritePointer (c)[offset + i] = scratch[i * channels + c];
    }
}
