#include "AudioExporter.h"

AudioExporter::AudioExporter() : juce::Thread("ADD A VALVE export")
{
}

AudioExporter::~AudioExporter()
{
    cancel();
    waitForThreadToExit(10000);
}

bool AudioExporter::start(std::shared_ptr<const juce::AudioBuffer<float>> audio,
                          double sampleRate,
                          const ValveParameters& parameters,
                          juce::File targetFile,
                          AudioExportSettings settings,
                          CompletionCallback callback)
{
    if (isThreadRunning() || audio == nullptr || audio->getNumSamples() == 0
        || sampleRate <= 0.0 || ! targetFile.getFullPathName().isNotEmpty())
        return false;

    sourceAudio = std::move(audio);
    sourceSampleRate = sampleRate;
    exportParameters = parameters;
    destination = std::move(targetFile);
    exportSettings = settings;
    completionCallback = std::move(callback);
    resultError.clear();
    resultSuccess = false;
    resultCancelled = false;
    progress.store(0.0f);
    cancelRequested.store(false);
    return startThread();
}

void AudioExporter::cancel()
{
    cancelRequested.store(true);
    signalThreadShouldExit();
}

float AudioExporter::getProgress() const noexcept
{
    return progress.load();
}

bool AudioExporter::isRunning() const
{
    return isThreadRunning();
}

void AudioExporter::run()
{
    const auto result = writeAudio(sourceAudio, sourceSampleRate, exportParameters,
                                   destination, exportSettings, &cancelRequested, &progress);
    resultCancelled = cancelRequested.load() || threadShouldExit();
    resultSuccess = result.wasOk() && ! resultCancelled;
    resultError = resultSuccess ? juce::String() : result.getErrorMessage();
    triggerAsyncUpdate();
}

void AudioExporter::handleAsyncUpdate()
{
    sourceAudio.reset();
    if (completionCallback != nullptr)
        completionCallback(resultSuccess, resultCancelled, resultError);
}

juce::String AudioExporter::getFileExtension(AudioExportFormat format)
{
    switch (format)
    {
        case AudioExportFormat::wav24:
        case AudioExportFormat::wav32Float: return "wav";
        case AudioExportFormat::flac24: return "flac";
        case AudioExportFormat::mp3: return "mp3";
    }
    return "wav";
}

juce::File AudioExporter::getMp3Encoder()
{
    return juce::File::getSpecialLocation(juce::File::currentExecutableFile).getSiblingFile("lame.exe");
}

juce::Result AudioExporter::writeAudio(std::shared_ptr<const juce::AudioBuffer<float>> audio,
                                       double sampleRate,
                                       const ValveParameters& parameters,
                                       juce::File targetFile,
                                       AudioExportSettings settings,
                                       std::atomic<bool>* cancelRequested,
                                       std::atomic<float>* progress)
{
    if (settings.format != AudioExportFormat::mp3)
        return writePcm(std::move(audio), sampleRate, parameters, std::move(targetFile), settings,
                        cancelRequested, progress, 0.0f, 1.0f);

    if (audio == nullptr || audio->getNumSamples() <= 0
        || audio->getNumChannels() < 1 || audio->getNumChannels() > 2 || sampleRate <= 0.0)
        return juce::Result::fail(L"没有可导出的单声道或立体声音频");
    if (! targetFile.getFullPathName().isNotEmpty())
        return juce::Result::fail(L"导出路径无效");
    if (cancelRequested != nullptr && cancelRequested->load())
        return juce::Result::fail(L"导出已取消");

    const auto supportedSampleRates = { 8000.0, 11025.0, 12000.0, 16000.0, 22050.0,
                                        24000.0, 32000.0, 44100.0, 48000.0 };
    if (std::find(supportedSampleRates.begin(), supportedSampleRates.end(), sampleRate) == supportedSampleRates.end())
        return juce::Result::fail(L"MP3 不支持当前采样率；请改用 WAV 或 FLAC 保留原采样率");

    const auto supportedBitrates = { 128, 192, 256, 320 };
    if (std::find(supportedBitrates.begin(), supportedBitrates.end(), settings.mp3Bitrate) == supportedBitrates.end())
        return juce::Result::fail(L"MP3 码率无效");
    if (! getMp3Encoder().existsAsFile())
        return juce::Result::fail(L"找不到随程序附带的 lame.exe，请重新安装完整程序包");

    const auto randomPart = juce::String(juce::Random::getSystemRandom().nextInt());
    const auto temporaryWav = targetFile.getSiblingFile(
        "." + targetFile.getFileNameWithoutExtension() + "-" + randomPart + ".render.wav");
    const auto temporaryMp3 = targetFile.getSiblingFile(
        "." + targetFile.getFileNameWithoutExtension() + "-" + randomPart + ".partial.mp3");
    temporaryWav.deleteFile();
    temporaryMp3.deleteFile();

    auto cleanup = [&temporaryWav, &temporaryMp3]
    {
        if (temporaryWav.existsAsFile())
            temporaryWav.deleteFile();
        if (temporaryMp3.existsAsFile())
            temporaryMp3.deleteFile();
    };

    AudioExportSettings intermediateSettings;
    intermediateSettings.format = AudioExportFormat::wav24;
    const auto renderResult = writePcm(audio, sampleRate, parameters, temporaryWav, intermediateSettings,
                                       cancelRequested, progress, 0.0f, 0.80f);
    if (renderResult.failed())
    {
        cleanup();
        return renderResult;
    }

    const auto encodeResult = encodeMp3(temporaryWav, temporaryMp3, sampleRate, audio->getNumChannels(),
                                        audio->getNumSamples(), settings.mp3Bitrate, cancelRequested, progress);
    if (encodeResult.failed())
    {
        cleanup();
        return encodeResult;
    }

    if (temporaryWav.existsAsFile())
        temporaryWav.deleteFile();
    if (targetFile.existsAsFile() && ! targetFile.deleteFile())
    {
        cleanup();
        return juce::Result::fail(L"无法替换目标文件");
    }
    if (! temporaryMp3.moveFileTo(targetFile))
    {
        cleanup();
        return juce::Result::fail(L"无法将临时文件移动为最终 MP3");
    }

    if (progress != nullptr)
        progress->store(1.0f);
    return juce::Result::ok();
}

juce::Result AudioExporter::encodeMp3(juce::File wavFile,
                                      juce::File targetFile,
                                      double sampleRate,
                                      int channels,
                                      juce::int64 expectedSamples,
                                      int bitrate,
                                      std::atomic<bool>* cancelRequested,
                                      std::atomic<float>* progress)
{
    juce::StringArray arguments;
    arguments.add(getMp3Encoder().getFullPathName());
    arguments.add("--quiet");
    arguments.add("--cbr");
    arguments.add("-b");
    arguments.add(juce::String(bitrate));
    arguments.add("--noreplaygain");
    arguments.add(wavFile.getFullPathName());
    arguments.add(targetFile.getFullPathName());

    juce::ChildProcess process;
    if (! process.start(arguments, juce::ChildProcess::wantStdErr))
        return juce::Result::fail(L"无法启动 MP3 编码器");

    while (process.isRunning())
    {
        if (cancelRequested != nullptr && cancelRequested->load())
        {
            process.kill();
            process.waitForProcessToFinish(5000);
            return juce::Result::fail(L"导出已取消");
        }
        juce::Thread::sleep(20);
    }

    if (process.getExitCode() != 0 || ! targetFile.existsAsFile() || targetFile.getSize() <= 0)
    {
        const auto encoderError = process.readAllProcessOutput().trim();
        return juce::Result::fail(encoderError.isNotEmpty()
                                      ? L"MP3 编码失败：" + encoderError
                                      : L"MP3 编码失败，请检查采样率和可用磁盘空间");
    }

    if (progress != nullptr)
        progress->store(0.92f);

    juce::MP3AudioFormat format;
    auto input = targetFile.createInputStream();
    std::unique_ptr<juce::AudioFormatReader> reader(
        input != nullptr ? format.createReaderFor(input.release(), true) : nullptr);
    if (reader == nullptr || reader->sampleRate != sampleRate
        || static_cast<int>(reader->numChannels) != channels
        || reader->lengthInSamples < expectedSamples
        || reader->lengthInSamples > expectedSamples + 4 * 1152)
        return juce::Result::fail(L"MP3 回读校验失败：格式或编码帧长度异常");

    juce::AudioBuffer<float> check(channels, 4096);
    const auto samplesToCheck = static_cast<int>(juce::jmin<juce::int64>(check.getNumSamples(), reader->lengthInSamples));
    if (samplesToCheck <= 0 || ! reader->read(&check, 0, samplesToCheck, 0, true, true))
        return juce::Result::fail(L"MP3 回读校验失败：无法解码音频");

    if (progress != nullptr)
        progress->store(0.96f);
    return juce::Result::ok();
}

juce::Result AudioExporter::writePcm(std::shared_ptr<const juce::AudioBuffer<float>> audio,
                                     double sampleRate,
                                     const ValveParameters& parameters,
                                     juce::File targetFile,
                                     AudioExportSettings settings,
                                     std::atomic<bool>* cancelRequested,
                                     std::atomic<float>* progress,
                                     float progressStart,
                                     float progressEnd)
{
    const auto cancelled = [cancelRequested]
    {
        return cancelRequested != nullptr && cancelRequested->load();
    };

    if (audio == nullptr || audio->getNumSamples() <= 0
        || audio->getNumChannels() < 1 || audio->getNumChannels() > 2 || sampleRate <= 0.0)
        return juce::Result::fail(L"没有可导出的单声道或立体声音频");
    if (! targetFile.getFullPathName().isNotEmpty())
        return juce::Result::fail(L"导出路径无效");
    if (cancelled())
        return juce::Result::fail(L"导出已取消");

    const auto isFlac = settings.format == AudioExportFormat::flac24;
    const auto isFloat = settings.format == AudioExportFormat::wav32Float;
    const auto bitsPerSample = isFlac || ! isFloat ? 24 : 32;
    const auto formatName = isFlac ? juce::String("FLAC") : juce::String("WAV");
    const auto temporaryFile = targetFile.getSiblingFile(
        "." + targetFile.getFileNameWithoutExtension() + "-"
        + juce::String(juce::Random::getSystemRandom().nextInt()) + ".partial." + getFileExtension(settings.format));
    temporaryFile.deleteFile();

    auto cleanupTemporary = [&temporaryFile]
    {
        if (temporaryFile.existsAsFile())
            temporaryFile.deleteFile();
    };

    auto fileStream = temporaryFile.createOutputStream();
    if (fileStream == nullptr || ! fileStream->openedOk())
        return juce::Result::fail(L"无法创建临时导出文件，请检查目录权限和磁盘空间");
    std::unique_ptr<juce::OutputStream> outputStream = std::move(fileStream);

    std::unique_ptr<juce::AudioFormat> format = isFlac
        ? std::unique_ptr<juce::AudioFormat>(std::make_unique<juce::FlacAudioFormat>())
        : std::unique_ptr<juce::AudioFormat>(std::make_unique<juce::WavAudioFormat>());
    std::unique_ptr<juce::AudioFormatWriter> writer(format->createWriterFor(
        outputStream.get(), sampleRate, static_cast<unsigned int>(audio->getNumChannels()),
        bitsPerSample, {}, isFlac ? 5 : 0));
    if (writer == nullptr)
    {
        outputStream.reset();
        cleanupTemporary();
        return juce::Result::fail(L"无法创建 " + formatName + L" 写入器，请检查采样率");
    }
    outputStream.release();

    constexpr int blockSize = 4096;
    const auto channels = audio->getNumChannels();
    const auto totalSamples = static_cast<juce::int64>(audio->getNumSamples());
    ValveProcessor processor;
    processor.prepare(sampleRate, blockSize, channels);
    processor.setParameters(parameters);
    processor.reset();
    const auto latency = juce::jmax(0.0f, processor.getLatencyInSamples());
    auto samplesToSkip = static_cast<int>(std::ceil(latency));
    const auto fractionalDelay = latency - std::floor(latency);

    juce::AudioBuffer<float> block(channels, blockSize);
    juce::AudioBuffer<float> outputBlock(channels, blockSize);
    std::array<float, 2> previousSamples {};
    juce::int64 readPosition = 0;
    juce::int64 writtenSamples = 0;

    while (writtenSamples < totalSamples)
    {
        if (cancelled())
        {
            writer.reset();
            cleanupTemporary();
            return juce::Result::fail(L"导出已取消");
        }

        block.clear();
        outputBlock.clear();
        const auto inputSamples = readPosition < totalSamples
            ? static_cast<int>(juce::jmin<juce::int64>(blockSize, totalSamples - readPosition)) : 0;
        for (int channel = 0; channel < channels; ++channel)
        {
            if (inputSamples > 0)
                block.copyFrom(channel, 0, *audio, channel, static_cast<int>(readPosition), inputSamples);
        }
        readPosition += inputSamples;

        processor.process(block);
        auto outputSamples = 0;
        for (int sample = 0; sample < blockSize; ++sample)
        {
            if (samplesToSkip > 0)
            {
                for (int channel = 0; channel < channels; ++channel)
                    previousSamples[static_cast<size_t>(channel)] = block.getSample(channel, sample);
                --samplesToSkip;
                continue;
            }

            if (writtenSamples + outputSamples < totalSamples)
            {
                for (int channel = 0; channel < channels; ++channel)
                {
                    const auto current = block.getSample(channel, sample);
                    const auto corrected = fractionalDelay == 0.0f ? current
                        : (1.0f - fractionalDelay) * previousSamples[static_cast<size_t>(channel)]
                            + fractionalDelay * current;
                    outputBlock.setSample(channel, outputSamples, corrected);
                }
                ++outputSamples;
            }

            for (int channel = 0; channel < channels; ++channel)
                previousSamples[static_cast<size_t>(channel)] = block.getSample(channel, sample);
        }

        if (outputSamples > 0
            && ! writer->writeFromAudioSampleBuffer(outputBlock, 0, outputSamples))
        {
            writer.reset();
            cleanupTemporary();
            return juce::Result::fail(L"写入 " + formatName + L" 失败，请检查磁盘空间");
        }

        writtenSamples += outputSamples;
        if (progress != nullptr)
        {
            const auto renderProgress = static_cast<float>(writtenSamples) / static_cast<float>(totalSamples);
            progress->store(progressStart + (progressEnd - progressStart) * renderProgress);
        }
    }

    writer->flush();
    writer.reset();

    if (cancelled())
    {
        cleanupTemporary();
        return juce::Result::fail(L"导出已取消");
    }

    auto input = temporaryFile.createInputStream();
    std::unique_ptr<juce::AudioFormatReader> reader(
        input != nullptr ? format->createReaderFor(input.release(), true) : nullptr);
    if (reader == nullptr || reader->sampleRate != sampleRate
        || static_cast<int>(reader->numChannels) != channels
        || reader->lengthInSamples != totalSamples
        || static_cast<int>(reader->bitsPerSample) != bitsPerSample
        || reader->usesFloatingPointData != isFloat)
    {
        cleanupTemporary();
        return juce::Result::fail(formatName + L" 导出回读校验失败");
    }

    if (targetFile.existsAsFile() && ! targetFile.deleteFile())
    {
        cleanupTemporary();
        return juce::Result::fail(L"无法替换目标文件");
    }

    if (! temporaryFile.moveFileTo(targetFile))
    {
        cleanupTemporary();
        return juce::Result::fail(L"无法将临时文件移动为最终 " + formatName);
    }

    if (progress != nullptr)
        progress->store(progressEnd);
    return juce::Result::ok();
}
