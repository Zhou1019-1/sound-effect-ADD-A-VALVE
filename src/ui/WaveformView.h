#pragma once

#include <JuceHeader.h>

class WaveformView : public juce::Component,
                     private juce::ScrollBar::Listener,
                     private juce::ChangeListener
{
public:
    WaveformView(juce::AudioFormatManager& formatManager, juce::AudioThumbnailCache& cache);
    ~WaveformView() override;

    void setAudio(std::shared_ptr<const juce::AudioBuffer<float>> audio, double sampleRate);
    void clear();
    void setSelection(juce::int64 start, juce::int64 end);
    void setPlayheadSeconds(double seconds);

    std::function<void(juce::int64, juce::int64)> onSelectionChanged;
    std::function<void(double)> onSeek;

    void paint(juce::Graphics& graphics) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& event) override;
    void mouseDrag(const juce::MouseEvent& event) override;
    void mouseDoubleClick(const juce::MouseEvent& event) override;
    void mouseWheelMove(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel) override;

private:
    void scrollBarMoved(juce::ScrollBar* scrollBar, double newRangeStart) override;
    void changeListenerCallback(juce::ChangeBroadcaster* source) override;
    double xToTime(int x) const;
    float timeToX(double seconds, juce::Rectangle<int> bounds) const;
    double visibleDuration() const;
    void updateVisibleRange(double newStart);

    juce::AudioThumbnailCache& thumbnailCache;
    juce::AudioThumbnail thumbnail;
    juce::ScrollBar horizontalScroll { false };
    std::shared_ptr<const juce::AudioBuffer<float>> audio;
    double sampleRate = 44100.0;
    double totalDuration = 0.0;
    double visibleStart = 0.0;
    double zoom = 1.0;
    double playheadSeconds = 0.0;
    juce::int64 selectionStart = 0;
    juce::int64 selectionEnd = 0;
    juce::int64 dragAnchor = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WaveformView)
};
