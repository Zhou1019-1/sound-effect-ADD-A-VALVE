#include "PlaybackEngine.h"

PlaybackEngine::PlaybackEngine() = default;

PlaybackEngine::~PlaybackEngine()
{
    shutdown();
}

juce::String PlaybackEngine::initialise()
{
    const auto error = deviceManager.initialiseWithDefaultDevices(0, 2);
    if (error.isNotEmpty())
        return error;

    deviceManager.addAudioCallback(this);
    return {};
}

void PlaybackEngine::shutdown()
{
    deviceManager.removeAudioCallback(this);
    transport.stop();
    transport.setSource(nullptr);
    memorySource.reset();
    playbackBuffer.reset();
    deviceManager.closeAudioDevice();
}

bool PlaybackEngine::loadAudio(const juce::AudioBuffer<float>& audio, double sourceSampleRate)
{
    if (audio.getNumSamples() == 0 || audio.getNumChannels() < 1 || audio.getNumChannels() > 2
        || sourceSampleRate <= 0.0)
        return false;

    const auto oldPosition = transport.getCurrentPosition();
    const auto shouldResume = transport.isPlaying();

    deviceManager.removeAudioCallback(this);
    transport.stop();
    transport.setSource(nullptr);
    memorySource.reset();

    playbackBuffer = std::make_unique<juce::AudioBuffer<float>>(audio);
    sourceChannels = audio.getNumChannels();
    sourceLengthSeconds = audio.getNumSamples() / sourceSampleRate;
    memorySource = std::make_unique<juce::MemoryAudioSource>(*playbackBuffer, false, false);
    transport.setSource(memorySource.get(), 0, nullptr, sourceSampleRate);
    transport.prepareToPlay(deviceBlockSize, deviceSampleRate);
    transport.setPosition(juce::jlimit(0.0, sourceLengthSeconds, oldPosition));

    deviceManager.addAudioCallback(this);

    if (shouldResume)
        transport.start();

    return true;
}

void PlaybackEngine::play()
{
    if (memorySource == nullptr)
        return;

    if (transport.getCurrentPosition() >= sourceLengthSeconds)
        transport.setPosition(0.0);

    transport.start();
}

void PlaybackEngine::stop()
{
    transport.stop();
}

void PlaybackEngine::setPosition(double seconds)
{
    transport.setPosition(juce::jlimit(0.0, sourceLengthSeconds, seconds));
}

void PlaybackEngine::setParameters(const ValveParameters& parameters)
{
    requestedDrive.store(parameters.drive, std::memory_order_release);
    requestedCharacter.store(parameters.character, std::memory_order_release);
    requestedWarmth.store(parameters.warmth, std::memory_order_release);
    requestedMix.store(parameters.mix, std::memory_order_release);
    requestedOutput.store(parameters.output, std::memory_order_release);
}

void PlaybackEngine::setBypassed(bool shouldBypass)
{
    bypassed.store(shouldBypass, std::memory_order_release);
}

bool PlaybackEngine::isPlaying() const
{
    return transport.isPlaying();
}

double PlaybackEngine::getPosition() const
{
    return transport.getCurrentPosition();
}

double PlaybackEngine::getLength() const
{
    return sourceLengthSeconds;
}

Metering& PlaybackEngine::getMetering() noexcept
{
    return metering;
}

juce::AudioDeviceManager& PlaybackEngine::getDeviceManager() noexcept
{
    return deviceManager;
}

bool PlaybackEngine::parametersEqual(const ValveParameters& a, const ValveParameters& b) noexcept
{
    return a.drive == b.drive && a.character == b.character && a.warmth == b.warmth
        && a.mix == b.mix && a.output == b.output;
}

ValveParameters PlaybackEngine::loadParameters() const noexcept
{
    ValveParameters parameters;
    parameters.drive = requestedDrive.load(std::memory_order_acquire);
    parameters.character = requestedCharacter.load(std::memory_order_acquire);
    parameters.warmth = requestedWarmth.load(std::memory_order_acquire);
    parameters.mix = requestedMix.load(std::memory_order_acquire);
    parameters.output = requestedOutput.load(std::memory_order_acquire);
    return parameters;
}

void PlaybackEngine::audioDeviceAboutToStart(juce::AudioIODevice* device)
{
    deviceSampleRate = device != nullptr ? device->getCurrentSampleRate() : 44100.0;
    deviceBlockSize = device != nullptr ? device->getCurrentBufferSizeSamples() : 512;
    outputChannels = device != nullptr ? juce::jlimit(1, 2, device->getOutputChannelNames().size()) : 2;

    transport.prepareToPlay(deviceBlockSize, deviceSampleRate);
    processor.prepare(deviceSampleRate, deviceBlockSize, outputChannels);
    processor.reset();
    currentParameters = loadParameters();
    processor.setParameters(currentParameters);

    effectBuffer.setSize(outputChannels, deviceBlockSize);
    dryDelay.setMaximumDelayInSamples(juce::jmax(1, static_cast<int>(std::ceil(processor.getLatencyInSamples())) + 2));
    dryDelay.prepare({ deviceSampleRate, static_cast<juce::uint32>(deviceBlockSize), static_cast<juce::uint32>(outputChannels) });
    dryDelay.setDelay(juce::jmax(0.0f, static_cast<float>(processor.getLatencyInSamples())));
    dryDelay.reset();

    effectCrossfade.reset(deviceSampleRate, 0.015);
    effectCrossfade.setCurrentAndTargetValue(bypassed.load(std::memory_order_acquire) ? 0.0f : 1.0f);
    metering.reset();
}

void PlaybackEngine::audioDeviceStopped()
{
}

void PlaybackEngine::audioDeviceIOCallbackWithContext(const float* const* inputChannelData,
                                                       int numInputChannels,
                                                       float* const* outputChannelData,
                                                       int numOutputChannels,
                                                       int numSamples,
                                                       const juce::AudioIODeviceCallbackContext& context)
{
    juce::ignoreUnused(inputChannelData, numInputChannels, context);
    juce::ScopedNoDenormals noDenormals;

    for (int channel = 0; channel < numOutputChannels; ++channel)
        juce::FloatVectorOperations::clear(outputChannelData[channel], numSamples);

    if (numOutputChannels <= 0 || numSamples <= 0)
        return;

    const auto channels = juce::jmin(outputChannels, numOutputChannels);
    juce::AudioBuffer<float> dryBuffer(outputChannelData, channels, numSamples);
    juce::AudioSourceChannelInfo info(&dryBuffer, 0, numSamples);
    transport.getNextAudioBlock(info);

    if (sourceChannels == 1 && channels == 2)
        dryBuffer.copyFrom(1, 0, dryBuffer, 0, 0, numSamples);

    juce::AudioBuffer<float> wetBuffer(effectBuffer.getArrayOfWritePointers(), channels, numSamples);
    for (int channel = 0; channel < channels; ++channel)
        wetBuffer.copyFrom(channel, 0, dryBuffer, channel, 0, numSamples);

    const auto latestParameters = loadParameters();
    if (! parametersEqual(currentParameters, latestParameters))
    {
        currentParameters = latestParameters;
        processor.setParameters(currentParameters);
    }

    processor.process(wetBuffer);
    effectCrossfade.setTargetValue(bypassed.load(std::memory_order_acquire) ? 0.0f : 1.0f);

    for (int sample = 0; sample < numSamples; ++sample)
    {
        const auto amount = effectCrossfade.getNextValue();
        for (int channel = 0; channel < channels; ++channel)
        {
            auto* output = dryBuffer.getWritePointer(channel);
            const auto* wet = wetBuffer.getReadPointer(channel);
            dryDelay.pushSample(channel, output[sample]);
            const auto alignedDry = dryDelay.popSample(channel);
            output[sample] = alignedDry + amount * (wet[sample] - alignedDry);
        }
    }

    metering.process(dryBuffer);
}
