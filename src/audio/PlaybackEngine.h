#pragma once

#include <JuceHeader.h>
#include "../dsp/ValveProcessor.h"
#include "Metering.h"

class PlaybackEngine : private juce::AudioIODeviceCallback
{
public:
    PlaybackEngine();
    ~PlaybackEngine() override;

    juce::String initialise();
    void shutdown();
    bool loadAudio(const juce::AudioBuffer<float>& audio, double sourceSampleRate);

    void play();
    void stop();
    void setPosition(double seconds);
    void setParameters(const ValveParameters& parameters);
    void setBypassed(bool shouldBypass);

    bool isPlaying() const;
    double getPosition() const;
    double getLength() const;
    Metering& getMetering() noexcept;
    juce::AudioDeviceManager& getDeviceManager() noexcept;

private:
    void audioDeviceAboutToStart(juce::AudioIODevice* device) override;
    void audioDeviceStopped() override;
    void audioDeviceIOCallbackWithContext(const float* const* inputChannelData,
                                          int numInputChannels,
                                          float* const* outputChannelData,
                                          int numOutputChannels,
                                          int numSamples,
                                          const juce::AudioIODeviceCallbackContext& context) override;

    static bool parametersEqual(const ValveParameters& a, const ValveParameters& b) noexcept;
    ValveParameters loadParameters() const noexcept;

    juce::AudioDeviceManager deviceManager;
    juce::AudioTransportSource transport;
    std::unique_ptr<juce::AudioBuffer<float>> playbackBuffer;
    std::unique_ptr<juce::MemoryAudioSource> memorySource;

    ValveProcessor processor;
    ValveParameters currentParameters {};
    std::atomic<float> requestedDrive { 0.25f };
    std::atomic<float> requestedCharacter { 0.35f };
    std::atomic<float> requestedWarmth { 0.25f };
    std::atomic<float> requestedMix { 1.0f };
    std::atomic<float> requestedOutput { 0.0f };
    std::atomic<bool> bypassed { false };
    juce::AudioBuffer<float> effectBuffer;
    juce::dsp::DelayLine<float> dryDelay;
    juce::SmoothedValue<float> effectCrossfade;
    Metering metering;

    double deviceSampleRate = 44100.0;
    int deviceBlockSize = 512;
    int outputChannels = 2;
    int sourceChannels = 0;
    double sourceLengthSeconds = 0.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PlaybackEngine)
};
