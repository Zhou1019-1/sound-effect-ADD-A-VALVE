#pragma once

#include <JuceHeader.h>
#include "../dsp/ValveParameters.h"
#include "RotaryKnob.h"

class ControlPanel : public juce::Component
{
public:
    ControlPanel();

    ValveParameters getParameters() const;
    void setParameters(const ValveParameters& parameters);

    std::function<void(const ValveParameters&)> onParametersChanged;
    std::function<void(int)> onPresetSelected;
    std::function<void()> onSavePreset;
    std::function<void()> onLoadPreset;
    std::function<void(bool)> onBypassChanged;

    void resized() override;

private:
    struct Control
    {
        RotaryKnob slider;      // 角度式拖拽，见 RotaryKnob.h
        juce::Label label;
    };

    std::array<Control, 5> controls;
    juce::ComboBox presetSelector;
    juce::TextButton savePresetButton { L"保存预设" };
    juce::TextButton loadPresetButton { L"载入预设" };
    juce::TextButton abButton { L"试听：处理" };
    bool updatingControls = false;

    void parametersChanged();
    void configureControl(int index, const juce::String& name, double min, double max, double interval, double value);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ControlPanel)
};
