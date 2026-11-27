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
    std::array<juce::dsp::IIR::Filter<float>, 2> dcFilters;
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
