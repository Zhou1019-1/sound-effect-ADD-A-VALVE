#pragma once

#include <JuceHeader.h>
#include "ValveParameters.h"

class ValveProcessor
{
public:
    ValveProcessor() = default;

    void prepare(double sampleRate, int samplesPerBlock, int numChannels);
    void reset();
    void setParameters(const ValveParameters& parameters);
    void process(juce::AudioBuffer<float>& buffer);
    float getLatencyInSamples() const;

private:
    static float saturate(float input, float character) noexcept;

    ValveParameters parameters {};
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampling;
    /** 直流阻断器：内部用 double 运算。

        改动原因（可复现）：这个 5 Hz 二阶高通跑在 4x 过采样率上（44.1k → 176.4 kHz），
        极点距单位圆只有 ~1.3e-4，float32 的舍入会被这个近极点放大到无法收敛：
        在真实音乐素材上实测，44.1 kHz 与 96 kHz 会在几秒内指数发散（幅值涨到 1e30，
        随后变 NaN / 整段静音），48 kHz 只是「恰好」稳定。

        频率响应、截止频率、Q 值与原实现完全一致，只把运算精度提到 double。 */
    struct DcBlocker
    {
        double b0 = 1.0, b1 = 0.0, b2 = 0.0, a1 = 0.0, a2 = 0.0;
        double x1 = 0.0, x2 = 0.0, y1 = 0.0, y2 = 0.0;

        void setCoefficients (const juce::dsp::IIR::Coefficients<double>& c)
        {
            b0 = c.coefficients[0]; b1 = c.coefficients[1]; b2 = c.coefficients[2];
            a1 = c.coefficients[3]; a2 = c.coefficients[4];
            reset();
        }

        void reset() noexcept { x1 = x2 = y1 = y2 = 0.0; }

        float process (float x) noexcept
        {
            const double xd = static_cast<double> (x);
            const double y = b0 * xd + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
            x2 = x1; x1 = xd;
            y2 = y1; y1 = y;
            return static_cast<float> (y);
        }
    };

    std::array<DcBlocker, 2> dcFilters;
    juce::dsp::DelayLine<float> dryDelay;
    juce::AudioBuffer<float> dryStorage;
    std::array<float, 2> warmthStates {};

    juce::SmoothedValue<float> driveGain;
    juce::SmoothedValue<float> characterAmount;
    juce::SmoothedValue<float> warmthAmount;
    juce::SmoothedValue<float> mixAmount;
    juce::SmoothedValue<float> outputGain;
    double baseSampleRate = 44100.0;
    int maximumBlockSize = 0;
    int channelCount = 0;
    bool prepared = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ValveProcessor)
};
