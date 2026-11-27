#include "AudioDocument.h"

class AudioDocument::EditCommand : public juce::UndoableAction
{
public:
    EditCommand(AudioDocument& owner,
                std::shared_ptr<const juce::AudioBuffer<float>> before,
                std::shared_ptr<const juce::AudioBuffer<float>> after,
                juce::int64 oldStart, juce::int64 oldEnd,
                juce::int64 newStart, juce::int64 newEnd)
        : document(owner), beforeAudio(std::move(before)), afterAudio(std::move(after)),
          beforeStart(oldStart), beforeEnd(oldEnd), afterStart(newStart), afterEnd(newEnd)
    {
        const auto beforeBytes = beforeAudio != nullptr
            ? static_cast<size_t>(beforeAudio->getNumChannels()) * beforeAudio->getNumSamples() * sizeof(float) : 0;
        const auto afterBytes = afterAudio != nullptr
            ? static_cast<size_t>(afterAudio->getNumChannels()) * afterAudio->getNumSamples() * sizeof(float) : 0;
        sizeInUnits = juce::jmax(1, static_cast<int>(juce::jmin<size_t>(
            std::numeric_limits<int>::max(), (beforeBytes + afterBytes + historyUnitBytes - 1) / historyUnitBytes)));
    }

    bool perform() override
    {
        document.restoreState(afterAudio, afterStart, afterEnd);
        return true;
    }

    bool undo() override
    {
        document.restoreState(beforeAudio, beforeStart, beforeEnd);
        return true;
    }

    int getSizeInUnits() override { return sizeInUnits; }

private:
    AudioDocument& document;
    std::shared_ptr<const juce::AudioBuffer<float>> beforeAudio;
    std::shared_ptr<const juce::AudioBuffer<float>> afterAudio;
    juce::int64 beforeStart, beforeEnd, afterStart, afterEnd;
    int sizeInUnits = 1;
};

AudioDocument::AudioDocument()
    : undoManager(static_cast<int>(defaultHistoryBytes / historyUnitBytes), 1)
{
}

void AudioDocument::setAudio(std::shared_ptr<const juce::AudioBuffer<float>> newAudio,
                             double sampleRate, const juce::File& sourceFile)
{
    audio = std::move(newAudio);
    audioSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
    originalFile = sourceFile;
    selectionStart = 0;
    selectionEnd = 0;
    undoManager.clearUndoHistory();
}

std::shared_ptr<const juce::AudioBuffer<float>> AudioDocument::getAudio() const { return audio; }
double AudioDocument::getSampleRate() const { return audioSampleRate; }
juce::int64 AudioDocument::getLengthSamples() const { return audio != nullptr ? audio->getNumSamples() : 0; }
juce::File AudioDocument::getSourceFile() const { return originalFile; }

void AudioDocument::setSelection(juce::int64 start, juce::int64 end)
{
    const auto length = getLengthSamples();
    selectionStart = juce::jlimit<juce::int64>(0, length, juce::jmin(start, end));
    selectionEnd = juce::jlimit<juce::int64>(0, length, juce::jmax(start, end));
}

juce::int64 AudioDocument::getSelectionStart() const { return selectionStart; }
juce::int64 AudioDocument::getSelectionEnd() const { return selectionEnd; }

std::pair<juce::int64, juce::int64> AudioDocument::editRangeOrWholeFile() const
{
    if (selectionEnd > selectionStart)
        return { selectionStart, selectionEnd };
    return { 0, getLengthSamples() };
}

bool AudioDocument::trimToSelection()
{
    if (audio == nullptr || selectionEnd <= selectionStart
        || (selectionStart == 0 && selectionEnd == getLengthSamples()))
        return false;

    const auto newLength = static_cast<int>(selectionEnd - selectionStart);
    auto result = std::make_shared<juce::AudioBuffer<float>>(audio->getNumChannels(), newLength);
    for (int channel = 0; channel < audio->getNumChannels(); ++channel)
        result->copyFrom(channel, 0, *audio, channel, static_cast<int>(selectionStart), newLength);
    return applyEdit(result, 0, newLength, L"裁剪到选区");
}

bool AudioDocument::deleteSelection()
{
    if (audio == nullptr || selectionEnd <= selectionStart
        || selectionEnd - selectionStart >= getLengthSamples())
        return false;

    const auto removed = selectionEnd - selectionStart;
    const auto newLength = static_cast<int>(getLengthSamples() - removed);
    const auto suffixLength = static_cast<int>(getLengthSamples() - selectionEnd);
    auto result = std::make_shared<juce::AudioBuffer<float>>(audio->getNumChannels(), newLength);

    for (int channel = 0; channel < audio->getNumChannels(); ++channel)
    {
        if (selectionStart > 0)
            result->copyFrom(channel, 0, *audio, channel, 0, static_cast<int>(selectionStart));
        if (suffixLength > 0)
            result->copyFrom(channel, static_cast<int>(selectionStart), *audio, channel,
                             static_cast<int>(selectionEnd), suffixLength);
    }
    return applyEdit(result, selectionStart, selectionStart, L"删除选区");
}

bool AudioDocument::fadeSelection(bool fadeIn)
{
    if (audio == nullptr || getLengthSamples() < 2)
        return false;

    const auto range = editRangeOrWholeFile();
    const auto count = static_cast<int>(range.second - range.first);
    if (count < 2)
        return false;

    auto result = std::make_shared<juce::AudioBuffer<float>>(*audio);
    for (int channel = 0; channel < result->getNumChannels(); ++channel)
    {
        auto* samples = result->getWritePointer(channel) + range.first;
        for (int sample = 0; sample < count; ++sample)
        {
            const auto position = static_cast<float>(sample) / static_cast<float>(count - 1);
            samples[sample] *= fadeIn ? position : 1.0f - position;
        }
    }
    return applyEdit(result, selectionStart, selectionEnd, fadeIn ? L"淡入" : L"淡出");
}

bool AudioDocument::applyGainDb(float gainDb)
{
    if (audio == nullptr || ! std::isfinite(gainDb) || std::abs(gainDb) > 96.0f || gainDb == 0.0f)
        return false;

    const auto range = editRangeOrWholeFile();
    const auto count = static_cast<int>(range.second - range.first);
    if (count <= 0)
        return false;

    auto result = std::make_shared<juce::AudioBuffer<float>>(*audio);
    const auto gain = juce::Decibels::decibelsToGain(gainDb);
    for (int channel = 0; channel < result->getNumChannels(); ++channel)
        juce::FloatVectorOperations::multiply(result->getWritePointer(channel) + range.first, gain, count);
    return applyEdit(result, selectionStart, selectionEnd, L"增益");
}

bool AudioDocument::applyEdit(std::shared_ptr<const juce::AudioBuffer<float>> newAudio,
                              juce::int64 newSelectionStart, juce::int64 newSelectionEnd,
                              const juce::String& name)
{
    undoManager.beginNewTransaction(name);
    return undoManager.perform(new EditCommand(*this, audio, std::move(newAudio),
                                               selectionStart, selectionEnd,
                                               newSelectionStart, newSelectionEnd), name);
}

void AudioDocument::restoreState(std::shared_ptr<const juce::AudioBuffer<float>> state,
                                 juce::int64 start, juce::int64 end)
{
    audio = std::move(state);
    setSelection(start, end);
}

bool AudioDocument::canUndo() const { return undoManager.canUndo(); }
bool AudioDocument::canRedo() const { return undoManager.canRedo(); }
bool AudioDocument::undo() { return undoManager.undo(); }
bool AudioDocument::redo() { return undoManager.redo(); }

void AudioDocument::setHistoryByteBudget(size_t bytes)
{
    historyByteBudget = juce::jmax(historyUnitBytes, bytes);
    undoManager.setMaxNumberOfStoredUnits(static_cast<int>(juce::jmin<size_t>(
        std::numeric_limits<int>::max(), historyByteBudget / historyUnitBytes)), 1);
}

size_t AudioDocument::getHistoryByteBudget() const { return historyByteBudget; }
