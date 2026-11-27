#include "MeterPanel.h"

namespace
{
    float toDb(float linear)
    {
        return linear > 0.0f ? juce::jmax(-60.0f, juce::Decibels::gainToDecibels(linear)) : -60.0f;
    }
}

MeterPanel::MeterPanel(Metering& metering) : source(metering)
{
    setOpaque(true);
    addAndMakeVisible(clipButton);
    clipButton.onClick = [this] { resetClip(); };
    updateClipButton();
    lastUpdateMs = juce::Time::getMillisecondCounterHiRes();
    startTimerHz(30);
}

MeterPanel::~MeterPanel()
{
    stopTimer();
}

void MeterPanel::resetClip()
{
    clipLatched = false;
    updateClipButton();
    repaint(clipBounds);
}

void MeterPanel::timerCallback()
{
    const auto now = juce::Time::getMillisecondCounterHiRes();
    const auto elapsedSeconds = juce::jlimit(0.0, 0.25, (now - lastUpdateMs) * 0.001);
    lastUpdateMs = now;
    const auto block = source.consume();
    const auto hasSamples = block.numSamples > 0;
    const auto vuAlpha = 1.0f - std::exp(-static_cast<float>(elapsedSeconds) / 0.30f);

    for (int channel = 0; channel < 2; ++channel)
    {
        const auto rms = hasSamples
            ? static_cast<float>(std::sqrt(juce::jmax(0.0, block.sumSquares[channel] / static_cast<double>(block.numSamples))))
            : 0.0f;
        const auto targetVu = juce::jlimit(-20.0f, 3.0f, toDb(rms) + 18.0f);
        vuValues[channel] += vuAlpha * (targetVu - vuValues[channel]);

        const auto targetPeak = hasSamples ? toDb(block.peaks[channel]) : -60.0f;
        peakDb[channel] = juce::jmax(targetPeak, peakDb[channel] - 26.0f * static_cast<float>(elapsedSeconds));

        if (targetPeak >= heldPeakDb[channel])
        {
            heldPeakDb[channel] = targetPeak;
            holdUntilMs[channel] = now + 2000.0;
        }
        else if (now >= holdUntilMs[channel])
        {
            heldPeakDb[channel] = juce::jmax(targetPeak,
                                              heldPeakDb[channel] - 12.0f * static_cast<float>(elapsedSeconds));
        }
    }

    const auto newClipState = clipLatched || block.clipped;
    if (newClipState != clipLatched)
    {
        clipLatched = newClipState;
        updateClipButton();
    }
    repaint();
}

void MeterPanel::resized()
{
    auto area = getLocalBounds().reduced(12);
    area.removeFromTop(26);
    clipBounds = area.removeFromBottom(24);
    clipButton.setBounds(clipBounds.reduced(1, 3));
}

void MeterPanel::paint(juce::Graphics& graphics)
{
    graphics.fillAll(juce::Colour(0xff171a1e));
    auto area = getLocalBounds().reduced(12);

    graphics.setColour(juce::Colour(0xffe8e2d6));
    graphics.setFont(juce::Font(16.0f, juce::Font::bold));
    graphics.drawText(L"动态仪表", area.removeFromTop(26), juce::Justification::centredLeft);

    area.removeFromBottom(24);
    auto levels = area.removeFromBottom(104);
    for (int channel = 0; channel < 2; ++channel)
        drawLevelBar(graphics, levels.removeFromTop(48).reduced(2, 4).toFloat(), channel);

    const auto gap = 8;
    const auto gaugeWidth = (area.getWidth() - gap) / 2;
    drawVuMeter(graphics, juce::Rectangle<int>(area.getX(), area.getY(), gaugeWidth, area.getHeight()).toFloat(), 0);
    drawVuMeter(graphics, juce::Rectangle<int>(area.getX() + gaugeWidth + gap, area.getY(), gaugeWidth, area.getHeight()).toFloat(), 1);
}

void MeterPanel::drawVuMeter(juce::Graphics& graphics, juce::Rectangle<float> bounds, int channel) const
{
    bounds = bounds.withHeight(juce::jmin(205.0f, bounds.getHeight())).withCentre(bounds.getCentre());
    graphics.setColour(juce::Colour(0xffe7ddc5));
    graphics.fillRoundedRectangle(bounds.reduced(1.0f), 7.0f);
    graphics.setColour(juce::Colour(0xff5c5549));
    graphics.drawRoundedRectangle(bounds.reduced(1.0f), 7.0f, 1.0f);

    auto face = bounds.reduced(8.0f);
    const auto centre = juce::Point<float>(face.getCentreX(), face.getBottom() - 15.0f);
    const auto radius = juce::jmin(face.getWidth() * 0.43f, face.getHeight() * 0.73f);
    constexpr float startDegrees = -132.0f;
    constexpr float endDegrees = -48.0f;

    auto angleForValue = [=](float value)
    {
        return juce::MathConstants<float>::pi
            * juce::jmap(juce::jlimit(-20.0f, 3.0f, value), -20.0f, 3.0f, startDegrees, endDegrees) / 180.0f;
    };

    juce::Path scaleArc;
    scaleArc.addCentredArc(centre.x, centre.y, radius, radius, 0.0f,
                           angleForValue(-20.0f), angleForValue(3.0f), true);
    graphics.setColour(juce::Colour(0xff554e43));
    graphics.strokePath(scaleArc, juce::PathStrokeType(1.4f));

    juce::Path overloadArc;
    overloadArc.addCentredArc(centre.x, centre.y, radius, radius, 0.0f,
                              angleForValue(0.0f), angleForValue(3.0f), true);
    graphics.setColour(juce::Colour(0xffb64b3d));
    graphics.strokePath(overloadArc, juce::PathStrokeType(2.8f));

    const std::array<float, 6> ticks { -20.0f, -10.0f, -5.0f, -3.0f, 0.0f, 3.0f };
    graphics.setFont(juce::Font(10.0f, juce::Font::bold));
    for (const auto tick : ticks)
    {
        const auto angle = angleForValue(tick);
        const auto outer = centre.getPointOnCircumference(radius, angle);
        const auto inner = centre.getPointOnCircumference(radius * 0.90f, angle);
        graphics.setColour(tick >= 0.0f ? juce::Colour(0xffa23e34) : juce::Colour(0xff4b463e));
        graphics.drawLine(inner.x, inner.y, outer.x, outer.y, 1.3f);

        const auto labelPosition = centre.getPointOnCircumference(radius * 0.74f, angle);
        const auto text = tick > 0.0f ? L"+" + juce::String(static_cast<int>(tick))
                                      : juce::String(static_cast<int>(tick));
        graphics.drawText(text,
                          juce::Rectangle<float>(26.0f, 14.0f).withCentre(labelPosition).toNearestInt(),
                          juce::Justification::centred);
    }

    const auto needleAngle = angleForValue(vuValues[channel]);
    const auto needleTip = centre.getPointOnCircumference(radius * 0.87f, needleAngle);
    graphics.setColour(juce::Colour(0xffc34d38));
    graphics.drawLine(juce::Line<float>(centre, needleTip), 2.0f);
    graphics.fillEllipse(juce::Rectangle<float>(10.0f, 10.0f).withCentre(centre));

    graphics.setColour(juce::Colour(0xff3d3932));
    graphics.setFont(juce::Font(13.0f, juce::Font::bold));
    graphics.drawText(juce::String(channel == 0 ? L"L" : L"R") + L"  VU",
                      face.removeFromTop(20).toNearestInt(), juce::Justification::centred);
    graphics.setFont(juce::Font(20.0f, juce::Font::bold));
    graphics.drawText(juce::String(vuValues[channel], 1), face.removeFromTop(30).toNearestInt(),
                      juce::Justification::centred);
    graphics.setFont(10.0f);
    graphics.drawText(L"0 VU = -18 dBFS", bounds.removeFromBottom(18).toNearestInt(), juce::Justification::centred);
}

void MeterPanel::drawLevelBar(juce::Graphics& graphics, juce::Rectangle<float> bounds, int channel) const
{
    const auto labelWidth = 20.0f;
    auto label = bounds.removeFromLeft(labelWidth);
    graphics.setColour(juce::Colour(0xffd7dadd));
    graphics.setFont(juce::Font(13.0f, juce::Font::bold));
    graphics.drawText(channel == 0 ? L"L" : L"R", label.toNearestInt(), juce::Justification::centredLeft);

    const auto valueHeight = 15.0f;
    auto bar = bounds.removeFromTop(bounds.getHeight() - valueHeight);
    graphics.setColour(juce::Colour(0xff30363c));
    graphics.fillRoundedRectangle(bar, 3.0f);

    const auto normalised = juce::jlimit(0.0f, 1.0f, (peakDb[channel] + 60.0f) / 60.0f);
    auto fill = bar.withWidth(bar.getWidth() * normalised);
    graphics.setColour(peakDb[channel] >= -1.0f ? juce::Colour(0xffee665b)
                         : peakDb[channel] >= -6.0f ? juce::Colour(0xffe7b653)
                                                    : juce::Colour(0xff69b58c));
    graphics.fillRoundedRectangle(fill, 3.0f);

    const auto heldX = bar.getX() + bar.getWidth() * juce::jlimit(0.0f, 1.0f, (heldPeakDb[channel] + 60.0f) / 60.0f);
    graphics.setColour(juce::Colours::white);
    graphics.drawLine(heldX, bar.getY(), heldX, bar.getBottom(), 1.4f);

    graphics.setColour(juce::Colour(0xff9ba4ab));
    graphics.setFont(10.0f);
    graphics.drawText(juce::String(peakDb[channel], 1) + L" dBFS",
                      bounds.toNearestInt(), juce::Justification::centredRight);
}

void MeterPanel::updateClipButton()
{
    clipButton.setButtonText(clipLatched ? L"削波已锁存 · 点击复位" : L"样本峰值未削波");
    clipButton.setColour(juce::TextButton::buttonColourId,
                         clipLatched ? juce::Colour(0xffff5f56) : juce::Colour(0xff687078));
    clipButton.setColour(juce::TextButton::textColourOffId,
                         clipLatched ? juce::Colours::white : juce::Colour(0xffe0e3e5));
}
