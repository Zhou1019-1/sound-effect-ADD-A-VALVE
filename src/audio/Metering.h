#pragma once

#include <JuceHeader.h>

struct MeterBlock
{
    double sumSquares[2] {};
    float peaks[2] {};
    juce::int64 numSamples = 0;
    bool clipped = false;
};

class Metering
{
public:
    Metering() = default;

    void reset() noexcept;
    void process(const juce::AudioBuffer<float>& buffer) noexcept;
    MeterBlock consume() noexcept;

private:
    static void merge(MeterBlock& destination, const MeterBlock& source) noexcept;

    static constexpr int queueSize = 256;
    juce::AbstractFifo fifo { queueSize };
    std::array<MeterBlock, queueSize> queue {};
    MeterBlock pending {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Metering)
};
