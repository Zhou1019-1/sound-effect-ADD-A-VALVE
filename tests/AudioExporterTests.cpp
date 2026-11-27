#include <JuceHeader.h>
#include "../src/audio/AudioExporter.h"

class AudioExporterTests : public juce::UnitTest
{
public:
    AudioExporterTests() : juce::UnitTest("Audio export", "Audio") {}

    void runTest() override
    {
        constexpr int sampleRate = 48000;
        constexpr int length = 48000;
        auto audio = std::make_shared<juce::AudioBuffer<float>>(2, length);
        for (int channel = 0; channel < 2; ++channel)
        {
            for (int sample = 0; sample < length; ++sample)
                audio->setSample(channel, sample,
                                 0.35f * std::sin(juce::MathConstants<float>::twoPi * 440.0f * sample / sampleRate));
        }

        const auto directory = juce::File::getSpecialLocation(juce::File::tempDirectory)
            .getChildFile("AddAValveTests-" + juce::String(juce::Random::getSystemRandom().nextInt()));
        expect(directory.createDirectory());
        ValveParameters parameters;
        parameters.mix = 0.0f;
        juce::AudioFormatManager formatManager;
        formatManager.registerBasicFormats();

        for (const bool float32 : { false, true })
        {
            beginTest(float32 ? "32-bit float WAV 回读" : "24-bit PCM WAV 回读");
            const auto file = directory.getChildFile(float32 ? "float.wav" : "pcm.wav");
            AudioExportSettings settings;
            settings.format = float32 ? AudioExportFormat::wav32Float : AudioExportFormat::wav24;
            const auto result = AudioExporter::writeAudio(audio, sampleRate, parameters, file, settings);
            expect(result.wasOk(), result.getErrorMessage());
            expect(file.existsAsFile());

            std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(file));
            expect(reader != nullptr);
            if (reader == nullptr)
                continue;

            expectEquals(reader->sampleRate, static_cast<double>(sampleRate));
            expectEquals(reader->lengthInSamples, static_cast<juce::int64>(length));
            expectEquals(static_cast<int>(reader->numChannels), 2);
            expectEquals(static_cast<int>(reader->bitsPerSample), float32 ? 32 : 24);
            expect(reader->usesFloatingPointData == float32);

            juce::AudioBuffer<float> readBack(2, length);
            expect(reader->read(&readBack, 0, length, 0, true, true));
            expectWithinAbsoluteError(readBack.getSample(0, 100), audio->getSample(0, 100),
                                      float32 ? 0.0002f : 0.0003f);
        }

        beginTest("24-bit FLAC 无损回读");
        const auto flacFile = directory.getChildFile("lossless.flac");
        AudioExportSettings flacSettings;
        flacSettings.format = AudioExportFormat::flac24;
        const auto flacResult = AudioExporter::writeAudio(audio, sampleRate, parameters, flacFile, flacSettings);
        expect(flacResult.wasOk(), flacResult.getErrorMessage());
        std::unique_ptr<juce::AudioFormatReader> flacReader(formatManager.createReaderFor(flacFile));
        expect(flacReader != nullptr);
        if (flacResult.wasOk() && flacReader != nullptr)
        {
            expectEquals(flacReader->sampleRate, static_cast<double>(sampleRate));
            expectEquals(flacReader->lengthInSamples, static_cast<juce::int64>(length));
            expectEquals(static_cast<int>(flacReader->numChannels), 2);
            expectEquals(static_cast<int>(flacReader->bitsPerSample), 24);
            juce::AudioBuffer<float> readBack(2, length);
            expect(flacReader->read(&readBack, 0, length, 0, true, true));
            expectWithinAbsoluteError(readBack.getSample(0, 100), audio->getSample(0, 100), 0.0003f);
        }
        flacReader.reset();

        beginTest("320 kbps MP3 回读并补偿编码延迟");
        const auto mp3File = directory.getChildFile("lossy.mp3");
        AudioExportSettings mp3Settings;
        mp3Settings.format = AudioExportFormat::mp3;
        mp3Settings.mp3Bitrate = 320;
        const auto mp3Result = AudioExporter::writeAudio(audio, sampleRate, parameters, mp3File, mp3Settings);
        expect(mp3Result.wasOk(), mp3Result.getErrorMessage());
        std::unique_ptr<juce::AudioFormatReader> mp3Reader(formatManager.createReaderFor(mp3File));
        expect(mp3Reader != nullptr);
        if (mp3Result.wasOk() && mp3Reader != nullptr)
        {
            expectEquals(mp3Reader->sampleRate, static_cast<double>(sampleRate));
            expectEquals(static_cast<int>(mp3Reader->numChannels), 2);
            expect(mp3Reader->lengthInSamples >= length);
            expect(mp3Reader->lengthInSamples <= length + 4 * 1152);

            const auto decodedLength = static_cast<int>(mp3Reader->lengthInSamples);
            juce::AudioBuffer<float> decoded(2, decodedLength);
            const auto decodedOk = mp3Reader->read(&decoded, 0, decodedLength, 0, true, true);
            expect(decodedOk);
            if (decodedOk)
            {
                constexpr int compareStart = 4000;
                constexpr int compareLength = 16000;
                const auto maxOffset = juce::jmin(4096, decodedLength - compareStart - compareLength);
                auto bestError = std::numeric_limits<float>::max();
                auto bestOffset = -1;
                for (int offset = 0; offset <= maxOffset; ++offset)
                {
                    auto squaredError = 0.0;
                    for (int sample = 0; sample < compareLength; ++sample)
                    {
                        const auto difference = decoded.getSample(0, compareStart + offset + sample)
                            - audio->getSample(0, compareStart + sample);
                        squaredError += difference * difference;
                    }
                    const auto meanError = static_cast<float>(squaredError / compareLength);
                    if (meanError < bestError)
                    {
                        bestError = meanError;
                        bestOffset = offset;
                    }
                }
                expect(bestOffset >= 0);
                expect(bestError < 0.02f,
                       "MP3 均方误差 " + juce::String(bestError) + "，延迟 " + juce::String(bestOffset));
            }
        }
        mp3Reader.reset();

        for (const int bitrate : { 128, 192, 256 })
        {
            beginTest(juce::String(bitrate) + " kbps MP3 编码回读");
            const auto bitrateFile = directory.getChildFile(juce::String(bitrate) + ".mp3");
            auto bitrateSettings = mp3Settings;
            bitrateSettings.mp3Bitrate = bitrate;
            const auto result = AudioExporter::writeAudio(audio, sampleRate, parameters,
                                                          bitrateFile, bitrateSettings);
            expect(result.wasOk(), result.getErrorMessage());
            std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(bitrateFile));
            expect(reader != nullptr);
            if (reader != nullptr)
            {
                expectEquals(reader->sampleRate, static_cast<double>(sampleRate));
                expectEquals(static_cast<int>(reader->numChannels), 2);
                expect(reader->lengthInSamples >= length);
                expect(reader->lengthInSamples <= length + 4 * 1152);
                juce::AudioBuffer<float> check(2, 4096);
                expect(reader->read(&check, 0, check.getNumSamples(), 0, true, true));
            }
        }

        beginTest("MP3 不支持的采样率会明确失败");
        const auto unsupportedFile = directory.getChildFile("unsupported.mp3");
        const auto unsupportedResult = AudioExporter::writeAudio(audio, 96000.0, parameters,
                                                                 unsupportedFile, mp3Settings);
        expect(unsupportedResult.failed());
        expect(! unsupportedFile.existsAsFile());

        beginTest("预先取消不会留下成品");
        const auto cancelledFile = directory.getChildFile("cancelled.wav");
        std::atomic<bool> cancelled { true };
        const auto result = AudioExporter::writeAudio(audio, sampleRate, parameters, cancelledFile, {}, &cancelled);
        expect(result.failed());
        expect(! cancelledFile.existsAsFile());
        expect(directory.deleteRecursively());
    }
};

static AudioExporterTests audioExporterTests;
