#include <JuceHeader.h>
#include "../src/dsp/ValveProcessor.h"

namespace
{
    double spectralMagnitude(const juce::AudioBuffer<float>& buffer, int start, int count,
                             double frequency, double sampleRate)
    {
        double real = 0.0, imaginary = 0.0;
        for (int sample = 0; sample < count; ++sample)
        {
            const auto phase = -juce::MathConstants<double>::twoPi * frequency * sample / sampleRate;
            const auto value = buffer.getSample(0, start + sample);
            real += value * std::cos(phase);
            imaginary += value * std::sin(phase);
        }
        return 2.0 * std::sqrt(real * real + imaginary * imaginary) / count;
    }
}

class ValveProcessorTests : public juce::UnitTest
{
public:
    ValveProcessorTests() : juce::UnitTest("Valve processor", "DSP") {}

    void runTest() override
    {
        for (const auto sampleRate : { 44100.0, 48000.0, 96000.0 })
        {
            for (const int channels : { 1, 2 })
            {
                beginTest("有限值 " + juce::String(sampleRate) + " Hz / " + juce::String(channels) + "ch");
                ValveProcessor processor;
                processor.prepare(sampleRate, 512, channels);
                ValveParameters parameters;
                parameters.drive = 1.0f;
                parameters.character = 1.0f;
                parameters.warmth = 1.0f;
                processor.setParameters(parameters);
                processor.reset();

                juce::Random random(42);
                for (const int blockSize : { 32, 64, 127, 512 })
                {
                    juce::AudioBuffer<float> buffer(channels, blockSize);
                    for (int channel = 0; channel < channels; ++channel)
                        for (int sample = 0; sample < blockSize; ++sample)
                            buffer.setSample(channel, sample, random.nextFloat() * 0.4f - 0.2f);
                    processor.process(buffer);
                    for (int channel = 0; channel < channels; ++channel)
                        for (int sample = 0; sample < blockSize; ++sample)
                            expect(std::isfinite(buffer.getSample(channel, sample)));
                }
            }
        }

        beginTest("零输入无直流输出");
        ValveProcessor zeroProcessor;
        zeroProcessor.prepare(48000.0, 256, 2);
        juce::AudioBuffer<float> zeros(2, 256);
        zeros.clear();
        zeroProcessor.process(zeros);
        expectWithinAbsoluteError(zeros.getMagnitude(0, 256), 0.0f, 0.000001f);

        beginTest("Mix=0 时干声延迟对齐");
        constexpr int length = 4096;
        constexpr double sampleRate = 48000.0;
        juce::AudioBuffer<float> input(1, length);
        for (int sample = 0; sample < length; ++sample)
            input.setSample(0, sample, 0.4f * std::sin(juce::MathConstants<float>::twoPi * 997.0f * sample / sampleRate));
        auto output = input;
        ValveProcessor dryProcessor;
        dryProcessor.prepare(sampleRate, length, 1);
        ValveParameters dryParameters;
        dryParameters.mix = 0.0f;
        dryProcessor.setParameters(dryParameters);
        dryProcessor.reset();
        dryProcessor.process(output);

        const auto latency = dryProcessor.getLatencyInSamples();
        const auto whole = static_cast<int>(std::floor(latency));
        const auto fraction = latency - whole;
        double squaredError = 0.0;
        int comparisons = 0;
        for (int sample = whole + 257; sample < length; ++sample)
        {
            const auto expected = (1.0f - fraction) * input.getSample(0, sample - whole)
                + fraction * input.getSample(0, sample - whole - 1);
            const auto error = output.getSample(0, sample) - expected;
            squaredError += error * error;
            ++comparisons;
        }
        expectWithinAbsoluteError(std::sqrt(squaredError / comparisons), 0.0, 0.002);

        beginTest("非对称饱和产生低阶谐波且混叠受控");
        constexpr int harmonicLength = 16384;
        juce::AudioBuffer<float> harmonic(1, harmonicLength);
        for (int sample = 0; sample < harmonicLength; ++sample)
            harmonic.setSample(0, sample, 0.25f * std::sin(juce::MathConstants<float>::twoPi * 1500.0f * sample / sampleRate));
        ValveProcessor harmonicProcessor;
        harmonicProcessor.prepare(sampleRate, harmonicLength, 1);
        ValveParameters harmonicParameters;
        harmonicParameters.drive = 0.8f;
        harmonicParameters.character = 0.95f;
        harmonicParameters.warmth = 0.0f;
        harmonicProcessor.setParameters(harmonicParameters);
        harmonicProcessor.reset();
        harmonicProcessor.process(harmonic);
        const auto fundamental = spectralMagnitude(harmonic, 4096, 8192, 1500.0, sampleRate);
        const auto second = spectralMagnitude(harmonic, 4096, 8192, 3000.0, sampleRate);
        expect(fundamental > 0.02);
        expect(second > 0.0003);

        juce::AudioBuffer<float> highFrequency(1, harmonicLength);
        for (int sample = 0; sample < harmonicLength; ++sample)
            highFrequency.setSample(0, sample, 0.2f * std::sin(juce::MathConstants<float>::twoPi * 18000.0f * sample / sampleRate));
        ValveProcessor aliasProcessor;
        aliasProcessor.prepare(sampleRate, harmonicLength, 1);
        aliasProcessor.setParameters(harmonicParameters);
        aliasProcessor.reset();
        aliasProcessor.process(highFrequency);
        const auto highFundamental = spectralMagnitude(highFrequency, 4096, 8192, 18000.0, sampleRate);
        const auto foldedSecond = spectralMagnitude(highFrequency, 4096, 8192, 12000.0, sampleRate);
        expect(foldedSecond < highFundamental * 0.02);

        beginTest("reset 后结果可重复且立体声通道一致");
        juce::AudioBuffer<float> stereo(2, 512);
        for (int sample = 0; sample < 512; ++sample)
        {
            const auto value = 0.3f * std::sin(juce::MathConstants<float>::twoPi * 440.0f * sample / sampleRate);
            stereo.setSample(0, sample, value);
            stereo.setSample(1, sample, value);
        }
        ValveProcessor repeatProcessor;
        repeatProcessor.prepare(sampleRate, 512, 2);
        repeatProcessor.setParameters(harmonicParameters);
        repeatProcessor.reset();
        auto first = stereo;
        repeatProcessor.process(first);
        repeatProcessor.reset();
        auto secondRun = stereo;
        repeatProcessor.process(secondRun);
        for (int sample = 0; sample < 512; ++sample)
        {
            expectWithinAbsoluteError(first.getSample(0, sample), secondRun.getSample(0, sample), 0.000001f);
            expectWithinAbsoluteError(first.getSample(0, sample), first.getSample(1, sample), 0.00001f);
        }
    }
};

static ValveProcessorTests valveProcessorTests;
