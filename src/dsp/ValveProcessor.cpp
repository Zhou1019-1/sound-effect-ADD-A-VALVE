#include "ValveProcessor.h"

void ValveProcessor::prepare(double sampleRate, int samplesPerBlock, int numChannels)
{
    jassert(sampleRate > 0.0 && samplesPerBlock > 0 && numChannels > 0 && numChannels <= 2);
    baseSampleRate = juce::jmax(1.0, sampleRate);
    maximumBlockSize = juce::jmax(1, samplesPerBlock);
    channelCount = juce::jlimit(1, 2, numChannels);

    oversampling = std::make_unique<juce::dsp::Oversampling<float>>(
        static_cast<size_t>(channelCount), 2,
        juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple, true);
    oversampling->initProcessing(static_cast<size_t>(maximumBlockSize));

    const auto oversampledRate = baseSampleRate * 4.0;
    const juce::dsp::ProcessSpec oversampledSpec {
        oversampledRate,
        static_cast<juce::uint32>(maximumBlockSize * 4),
        static_cast<juce::uint32>(channelCount)
    };
    const auto dcCoefficients = juce::dsp::IIR::Coefficients<float>::makeHighPass(oversampledRate, 5.0f, 0.7071f);
    for (auto& filter : dcFilters)
    {
        filter.prepare(oversampledSpec);
        filter.coefficients = dcCoefficients;
    }

    dryStorage.setSize(channelCount, maximumBlockSize);
    dryDelay.setMaximumDelayInSamples(juce::jmax(1, static_cast<int>(std::ceil(getLatencyInSamples())) + 4));
    dryDelay.prepare({ baseSampleRate,
                       static_cast<juce::uint32>(maximumBlockSize),
                       static_cast<juce::uint32>(channelCount) });
    dryDelay.setDelay(getLatencyInSamples());

    driveGain.reset(baseSampleRate, 0.020);
    characterAmount.reset(oversampledRate, 0.020);
    warmthAmount.reset(baseSampleRate, 0.020);
    mixAmount.reset(baseSampleRate, 0.020);
    outputGain.reset(baseSampleRate, 0.020);
    prepared = true;
    setParameters(parameters);
    reset();
}

void ValveProcessor::reset()
{
    if (! prepared)
        return;

    oversampling->reset();
    for (auto& filter : dcFilters)
        filter.reset();
    dryDelay.reset();
    warmthStates.fill(0.0f);
    driveGain.setCurrentAndTargetValue(driveGain.getTargetValue());
    characterAmount.setCurrentAndTargetValue(characterAmount.getTargetValue());
    warmthAmount.setCurrentAndTargetValue(warmthAmount.getTargetValue());
    mixAmount.setCurrentAndTargetValue(mixAmount.getTargetValue());
    outputGain.setCurrentAndTargetValue(outputGain.getTargetValue());
}

void ValveProcessor::setParameters(const ValveParameters& newParameters)
{
    parameters.drive = juce::jlimit(0.0f, 1.0f, newParameters.drive);
    parameters.character = juce::jlimit(0.0f, 1.0f, newParameters.character);
    parameters.warmth = juce::jlimit(0.0f, 1.0f, newParameters.warmth);
    parameters.mix = juce::jlimit(0.0f, 1.0f, newParameters.mix);
    parameters.output = juce::jlimit(-24.0f, 24.0f, newParameters.output);

    if (! prepared)
        return;

    driveGain.setTargetValue(1.0f + 11.0f * std::pow(parameters.drive, 1.35f));
    characterAmount.setTargetValue(parameters.character);
    warmthAmount.setTargetValue(parameters.warmth);
    mixAmount.setTargetValue(parameters.mix);
    outputGain.setTargetValue(juce::Decibels::decibelsToGain(parameters.output));
}

float ValveProcessor::getLatencyInSamples() const
{
    return oversampling != nullptr ? oversampling->getLatencyInSamples() : 0.0f;
}

float ValveProcessor::saturate(float input, float character) noexcept
{
    const auto symmetric = std::tanh(input);
    constexpr float bias = 0.42f;
    const auto biasShape = std::tanh(bias);
    const auto derivativeNormalisation = 1.0f - biasShape * biasShape;
    const auto asymmetric = (std::tanh(input + bias) - biasShape) / derivativeNormalisation;
    return 0.82f * ((1.0f - character) * symmetric + character * asymmetric);
}

void ValveProcessor::process(juce::AudioBuffer<float>& buffer)
{
    if (! prepared || buffer.getNumSamples() == 0)
        return;

    jassert(buffer.getNumSamples() <= maximumBlockSize);
    jassert(buffer.getNumChannels() == channelCount);
    if (buffer.getNumSamples() > maximumBlockSize || buffer.getNumChannels() != channelCount)
        return;

    juce::ScopedNoDenormals noDenormals;
    const auto numSamples = buffer.getNumSamples();
    juce::AudioBuffer<float> dry(dryStorage.getArrayOfWritePointers(), channelCount, numSamples);

    for (int channel = 0; channel < channelCount; ++channel)
        dry.copyFrom(channel, 0, buffer, channel, 0, numSamples);

    for (int sample = 0; sample < numSamples; ++sample)
    {
        const auto gain = driveGain.getNextValue();
        for (int channel = 0; channel < channelCount; ++channel)
            buffer.getWritePointer(channel)[sample] *= gain;
    }

    juce::dsp::AudioBlock<float> block(buffer);
    auto oversampledBlock = oversampling->processSamplesUp(block);
    for (size_t sample = 0; sample < oversampledBlock.getNumSamples(); ++sample)
    {
        const auto character = characterAmount.getNextValue();
        for (size_t channel = 0; channel < oversampledBlock.getNumChannels(); ++channel)
        {
            auto* samples = oversampledBlock.getChannelPointer(channel);
            samples[sample] = saturate(samples[sample], character);
        }
    }

    for (size_t channel = 0; channel < oversampledBlock.getNumChannels(); ++channel)
    {
        auto channelBlock = oversampledBlock.getSingleChannelBlock(channel);
        juce::dsp::ProcessContextReplacing<float> context(channelBlock);
        dcFilters[channel].process(context);
    }
    oversampling->processSamplesDown(block);

    for (int sample = 0; sample < numSamples; ++sample)
    {
        const auto warmth = warmthAmount.getNextValue();
        const auto cutoff = 18000.0f - 12500.0f * warmth;
        const auto alpha = 1.0f - std::exp(-juce::MathConstants<float>::twoPi * cutoff
                                           / static_cast<float>(baseSampleRate));
        const auto mix = mixAmount.getNextValue();
        const auto gain = outputGain.getNextValue();

        for (int channel = 0; channel < channelCount; ++channel)
        {
            auto* samples = buffer.getWritePointer(channel);
            const auto original = dry.getSample(channel, sample);
            auto& state = warmthStates[static_cast<size_t>(channel)];
            state += alpha * (samples[sample] - state);
            const auto warmed = samples[sample] + 0.38f * warmth * (state - samples[sample]);

            dryDelay.pushSample(channel, original);
            const auto delayedDry = dryDelay.popSample(channel);
            samples[sample] = gain * (delayedDry + mix * (warmed - delayedDry));
        }
    }
}
