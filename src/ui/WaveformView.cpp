#include "WaveformView.h"

WaveformView::WaveformView(juce::AudioFormatManager& formatManager, juce::AudioThumbnailCache& cache)
    : thumbnailCache(cache), thumbnail(512, formatManager, cache)
{
    setOpaque(true);
    addAndMakeVisible(horizontalScroll);
    horizontalScroll.addListener(this);
    horizontalScroll.setAutoHide(false);
    thumbnail.addChangeListener(this);
}

WaveformView::~WaveformView()
{
    thumbnail.removeChangeListener(this);
    horizontalScroll.removeListener(this);
}

void WaveformView::setAudio(std::shared_ptr<const juce::AudioBuffer<float>> newAudio, double newSampleRate)
{
    audio = std::move(newAudio);
    sampleRate = newSampleRate > 0.0 ? newSampleRate : 44100.0;
    totalDuration = audio != nullptr ? audio->getNumSamples() / sampleRate : 0.0;
    visibleStart = 0.0;
    zoom = 1.0;
    playheadSeconds = 0.0;
    selectionStart = 0;
    selectionEnd = 0;

    thumbnail.clear();
    if (audio != nullptr)
    {
        thumbnail.reset(audio->getNumChannels(), sampleRate, audio->getNumSamples());
        thumbnail.addBlock(0, *audio, 0, audio->getNumSamples());
    }

    updateVisibleRange(0.0);
    repaint();
}

void WaveformView::clear()
{
    setAudio(nullptr, sampleRate);
}

void WaveformView::setSelection(juce::int64 start, juce::int64 end)
{
    selectionStart = juce::jmin(start, end);
    selectionEnd = juce::jmax(start, end);
    repaint();
}

void WaveformView::setPlayheadSeconds(double seconds)
{
    playheadSeconds = juce::jlimit(0.0, totalDuration, seconds);
    repaint();
}

void WaveformView::resized()
{
    horizontalScroll.setBounds(getLocalBounds().removeFromBottom(16).reduced(8, 0));
}

void WaveformView::paint(juce::Graphics& graphics)
{
    graphics.fillAll(juce::Colour(0xff20252a));
    auto area = getLocalBounds().reduced(8);
    area.removeFromBottom(16);

    graphics.setColour(juce::Colour(0xff111519));
    graphics.fillRoundedRectangle(area.toFloat(), 5.0f);
    graphics.setColour(juce::Colour(0xff3a434b));
    graphics.drawRoundedRectangle(area.toFloat(), 5.0f, 1.0f);

    if (audio == nullptr || totalDuration <= 0.0)
    {
        graphics.setColour(juce::Colour(0xff87929b));
        graphics.setFont(16.0f);
        graphics.drawText(L"导入 WAV / FLAC / MP3 后，在此拖动选区；滚轮缩放，双击全选",
                          area, juce::Justification::centred);
        return;
    }

    auto waveformArea = area.reduced(2);
    const auto duration = visibleDuration();
    graphics.setColour(juce::Colour(0xff70b79a));
    thumbnail.drawChannels(graphics, waveformArea, visibleStart, visibleStart + duration, 0.92f);

    const auto selectionStartTime = selectionStart / sampleRate;
    const auto selectionEndTime = selectionEnd / sampleRate;
    const auto selectionX1 = timeToX(selectionStartTime, waveformArea);
    const auto selectionX2 = timeToX(selectionEndTime, waveformArea);
    juce::Rectangle<float> selection(selectionX1, static_cast<float>(waveformArea.getY()),
                                     juce::jmax(1.0f, selectionX2 - selectionX1),
                                     static_cast<float>(waveformArea.getHeight()));
    selection = selection.getIntersection(waveformArea.toFloat());
    graphics.setColour(juce::Colour(0x426bb7e6));
    graphics.fillRect(selection);
    graphics.setColour(juce::Colour(0xff91d5f5));
    graphics.drawVerticalLine(juce::roundToInt(selectionX1), static_cast<float>(waveformArea.getY()),
                              static_cast<float>(waveformArea.getBottom()));
    if (selectionEnd > selectionStart)
        graphics.drawVerticalLine(juce::roundToInt(selectionX2), static_cast<float>(waveformArea.getY()),
                                  static_cast<float>(waveformArea.getBottom()));

    const auto playheadX = timeToX(playheadSeconds, waveformArea);
    graphics.setColour(juce::Colour(0xfff0b55b));
    graphics.drawVerticalLine(juce::roundToInt(playheadX), static_cast<float>(waveformArea.getY()),
                              static_cast<float>(waveformArea.getBottom()));

    graphics.setColour(juce::Colour(0xffaeb7be));
    graphics.setFont(11.0f);
    graphics.drawText(juce::String(visibleStart, 2) + " s", waveformArea.removeFromTop(18),
                      juce::Justification::topLeft);
    graphics.drawText(juce::String(juce::jmin(totalDuration, visibleStart + duration), 2) + " s",
                      waveformArea.removeFromTop(18), juce::Justification::topRight);
}

void WaveformView::mouseDown(const juce::MouseEvent& event)
{
    if (audio == nullptr)
        return;

    dragAnchor = static_cast<juce::int64>(juce::jlimit(0.0, totalDuration, xToTime(event.x)) * sampleRate);
    setSelection(dragAnchor, dragAnchor);
    if (onSeek != nullptr)
        onSeek(dragAnchor / sampleRate);
    if (onSelectionChanged != nullptr)
        onSelectionChanged(dragAnchor, dragAnchor);
}

void WaveformView::mouseDrag(const juce::MouseEvent& event)
{
    if (audio == nullptr)
        return;

    const auto current = static_cast<juce::int64>(juce::jlimit(0.0, totalDuration, xToTime(event.x)) * sampleRate);
    setSelection(dragAnchor, current);
    if (onSelectionChanged != nullptr)
        onSelectionChanged(selectionStart, selectionEnd);
}

void WaveformView::mouseDoubleClick(const juce::MouseEvent&)
{
    if (audio == nullptr)
        return;

    setSelection(0, audio->getNumSamples());
    if (onSelectionChanged != nullptr)
        onSelectionChanged(selectionStart, selectionEnd);
}

void WaveformView::mouseWheelMove(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel)
{
    if (audio == nullptr || wheel.deltaY == 0.0f)
        return;

    const auto anchorTime = xToTime(event.x);
    const auto width = juce::jmax(1, getWidth() - 20);
    const auto proportion = juce::jlimit(0.0, 1.0, (event.x - 10.0) / static_cast<double>(width));
    zoom = juce::jlimit(1.0, 128.0, zoom * (wheel.deltaY > 0.0f ? 1.25 : 0.8));
    updateVisibleRange(anchorTime - proportion * visibleDuration());
}

double WaveformView::xToTime(int x) const
{
    auto width = juce::jmax(1, getWidth() - 20);
    return visibleStart + juce::jlimit(0.0, 1.0, (x - 10.0) / static_cast<double>(width)) * visibleDuration();
}

float WaveformView::timeToX(double seconds, juce::Rectangle<int> bounds) const
{
    return static_cast<float>(bounds.getX() + ((seconds - visibleStart) / visibleDuration()) * bounds.getWidth());
}

double WaveformView::visibleDuration() const
{
    return totalDuration > 0.0 ? totalDuration / zoom : 1.0;
}

void WaveformView::updateVisibleRange(double newStart)
{
    const auto duration = visibleDuration();
    visibleStart = juce::jlimit(0.0, juce::jmax(0.0, totalDuration - duration), newStart);
    horizontalScroll.setRangeLimits(0.0, totalDuration);
    horizontalScroll.setCurrentRange(visibleStart, duration, juce::dontSendNotification);
    repaint();
}

void WaveformView::scrollBarMoved(juce::ScrollBar*, double newRangeStart)
{
    visibleStart = juce::jlimit(0.0, juce::jmax(0.0, totalDuration - visibleDuration()), newRangeStart);
    repaint();
}

void WaveformView::changeListenerCallback(juce::ChangeBroadcaster*)
{
    repaint();
}
