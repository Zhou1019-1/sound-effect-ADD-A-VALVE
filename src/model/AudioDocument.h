#pragma once

#include <JuceHeader.h>

class AudioDocument
{
public:
    AudioDocument();

    void setAudio(std::shared_ptr<const juce::AudioBuffer<float>> audio,
                  double sampleRate,
                  const juce::File& sourceFile = {});
    std::shared_ptr<const juce::AudioBuffer<float>> getAudio() const;
    double getSampleRate() const;
    juce::int64 getLengthSamples() const;
    juce::File getSourceFile() const;

    void setSelection(juce::int64 start, juce::int64 end);
    juce::int64 getSelectionStart() const;
    juce::int64 getSelectionEnd() const;

    bool trimToSelection();
    bool deleteSelection();
    bool fadeSelection(bool fadeIn);
    bool applyGainDb(float gainDb);

    bool canUndo() const;
    bool canRedo() const;
    bool undo();
    bool redo();
    void setHistoryByteBudget(size_t bytes);
    size_t getHistoryByteBudget() const;

private:
    class EditCommand;
    static constexpr size_t historyUnitBytes = 64 * 1024;
    static constexpr size_t defaultHistoryBytes = 512 * 1024 * 1024;

    bool applyEdit(std::shared_ptr<const juce::AudioBuffer<float>> newAudio,
                   juce::int64 newSelectionStart,
                   juce::int64 newSelectionEnd,
                   const juce::String& name);
    void restoreState(std::shared_ptr<const juce::AudioBuffer<float>> state,
                      juce::int64 start, juce::int64 end);
    std::pair<juce::int64, juce::int64> editRangeOrWholeFile() const;

    std::shared_ptr<const juce::AudioBuffer<float>> audio;
    double audioSampleRate = 44100.0;
    juce::File originalFile;
    juce::int64 selectionStart = 0;
    juce::int64 selectionEnd = 0;
    juce::UndoManager undoManager;
    size_t historyByteBudget = defaultHistoryBytes;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioDocument)
};
