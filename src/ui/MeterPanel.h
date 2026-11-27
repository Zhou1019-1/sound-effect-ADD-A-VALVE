#pragma once

#include <JuceHeader.h>
#include "../audio/Metering.h"

class MeterPanel : public juce::Component, private juce::Timer
{
public:
    explicit MeterPanel(Metering& metering);
    ~MeterPanel() override;

    void resetClip();
    void paint(juce::Graphics& graphics) override;
    void resized() override;

private:
    void timerCallback() override;
    void updateClipButton();
    void drawVuMeter(juce::Graphics& graphics, juce::Rectangle<float> bounds, int channel) const;
    void drawLevelBar(juce::Graphics& graphics, juce::Rectangle<float> bounds, int channel) const;

    Metering& source;
    std::array<float, 2> vuValues { -20.0f, -20.0f };
    std::array<float, 2> peakDb { -60.0f, -60.0f };
    std::array<float, 2> heldPeakDb { -60.0f, -60.0f };
    std::array<double, 2> holdUntilMs { 0.0, 0.0 };
    double lastUpdateMs = 0.0;
    bool clipLatched = false;
    juce::Rectangle<int> clipBounds;
    juce::TextButton clipButton;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MeterPanel)
};
