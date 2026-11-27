#pragma once

#include <JuceHeader.h>
#include "../dsp/ValveProcessor.h"

enum class AudioExportFormat
{
    wav24,
    wav32Float,
    flac24,
    mp3
};

struct AudioExportSettings
{
    AudioExportFormat format = AudioExportFormat::wav24;
    int mp3Bitrate = 256;
};

class AudioExporter : private juce::Thread, private juce::AsyncUpdater
{
public:
    using CompletionCallback = std::function<void(bool success, bool cancelled, const juce::String& error)>;

    AudioExporter();
    ~AudioExporter() override;

    bool start(std::shared_ptr<const juce::AudioBuffer<float>> audio,
               double sampleRate,
               const ValveParameters& parameters,
               juce::File targetFile,
               AudioExportSettings settings,
               CompletionCallback callback);
    void cancel();
    float getProgress() const noexcept;
    bool isRunning() const;

    static juce::Result writeAudio(std::shared_ptr<const juce::AudioBuffer<float>> audio,
                                   double sampleRate,
                                   const ValveParameters& parameters,
                                   juce::File targetFile,
                                   AudioExportSettings settings,
                                   std::atomic<bool>* cancelRequested = nullptr,
                                   std::atomic<float>* progress = nullptr);
    static juce::String getFileExtension(AudioExportFormat format);

private:
    void run() override;
    void handleAsyncUpdate() override;

    static juce::Result writePcm(std::shared_ptr<const juce::AudioBuffer<float>> audio,
                                 double sampleRate,
                                 const ValveParameters& parameters,
                                 juce::File targetFile,
                                 AudioExportSettings settings,
                                 std::atomic<bool>* cancelRequested,
                                 std::atomic<float>* progress,
                                 float progressStart,
                                 float progressEnd);
    static juce::Result encodeMp3(juce::File wavFile,
                                  juce::File targetFile,
                                  double sampleRate,
                                  int channels,
                                  juce::int64 expectedSamples,
                                  int bitrate,
                                  std::atomic<bool>* cancelRequested,
                                  std::atomic<float>* progress);
    static juce::File getMp3Encoder();

    std::shared_ptr<const juce::AudioBuffer<float>> sourceAudio;
    double sourceSampleRate = 44100.0;
    ValveParameters exportParameters {};
    juce::File destination;
    AudioExportSettings exportSettings {};
    CompletionCallback completionCallback;
    std::atomic<bool> cancelRequested { false };
    std::atomic<float> progress { 0.0f };
    juce::String resultError;
    bool resultSuccess = false;
    bool resultCancelled = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioExporter)
};
