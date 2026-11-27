#include <JuceHeader.h>
#include "../src/audio/Metering.h"

class MeteringTests : public juce::UnitTest
{
public:
    MeteringTests() : juce::UnitTest("Metering", "Audio") {}

    void runTest() override
    {
        beginTest("统计左右声道均方和与峰值");
        Metering metering;
        juce::AudioBuffer<float> buffer(2, 128);
        buffer.clear();
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            buffer.setSample(0, sample, 0.5f);
            buffer.setSample(1, sample, -0.25f);
        }
        metering.process(buffer);
        const auto result = metering.consume();
        expectEquals(result.numSamples, static_cast<juce::int64>(128));
        expectWithinAbsoluteError(result.peaks[0], 0.5f, 0.0001f);
        expectWithinAbsoluteError(result.peaks[1], 0.25f, 0.0001f);
        expectWithinAbsoluteError(result.sumSquares[0], 32.0, 0.0001);
        expectWithinAbsoluteError(result.sumSquares[1], 8.0, 0.0001);
        expect(! result.clipped);

        beginTest("队列满时合并待发布统计");
        Metering saturated;
        juce::AudioBuffer<float> block(1, 64);
        block.clear();
        for (int sample = 0; sample < 300; ++sample)
        {
            block.setSample(0, sample % 64, 0.1f);
            saturated.process(block);
        }
        const auto queued = saturated.consume();
        saturated.process(block); // 队列腾出空间后，下一次音频块发布之前合并的统计
        const auto merged = saturated.consume();
        expectEquals(queued.numSamples + merged.numSamples, static_cast<juce::int64>(301 * 64));

        beginTest("削波随统计锁存并可清空");
        Metering clipping;
        juce::AudioBuffer<float> clipped(2, 16);
        clipped.clear();
        clipped.setSample(1, 4, 1.25f);
        clipping.process(clipped);
        expect(clipping.consume().clipped);
        clipping.reset();
        expectEquals(clipping.consume().numSamples, static_cast<juce::int64>(0));
    }
};

static MeteringTests meteringTests;
