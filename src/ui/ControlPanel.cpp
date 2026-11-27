#include "ControlPanel.h"

ControlPanel::ControlPanel()
{
    configureControl(0, L"驱动 Drive", 0.0, 1.0, 0.001, 0.25);
    configureControl(1, L"特性 Character", 0.0, 1.0, 0.001, 0.35);
    configureControl(2, L"温暖 Warmth", 0.0, 1.0, 0.001, 0.25);
    configureControl(3, L"混合 Mix", 0.0, 1.0, 0.001, 1.0);
    configureControl(4, L"输出 Output", -24.0, 24.0, 0.1, 0.0);

    presetSelector.addItem(L"清透", 1);
    presetSelector.addItem(L"温暖（默认）", 2);
    presetSelector.addItem(L"浓郁", 3);
    presetSelector.setSelectedId(2, juce::dontSendNotification);
    presetSelector.onChange = [this]
    {
        if (onPresetSelected != nullptr)
            onPresetSelected(presetSelector.getSelectedId());
    };
    addAndMakeVisible(presetSelector);

    savePresetButton.onClick = [this] { if (onSavePreset != nullptr) onSavePreset(); };
    loadPresetButton.onClick = [this] { if (onLoadPreset != nullptr) onLoadPreset(); };
    addAndMakeVisible(savePresetButton);
    addAndMakeVisible(loadPresetButton);

    abButton.setClickingTogglesState(true);
    abButton.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff7b5d43));
    abButton.onClick = [this]
    {
        const auto isDry = abButton.getToggleState();
        abButton.setButtonText(isDry ? L"试听：原声" : L"试听：处理");
        if (onBypassChanged != nullptr)
            onBypassChanged(isDry);
    };
    addAndMakeVisible(abButton);
}

void ControlPanel::configureControl(int index, const juce::String& name,
                                    double min, double max, double interval, double value)
{
    auto& control = controls[static_cast<size_t>(index)];
    control.slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    control.slider.setRange(min, max, interval);
    control.slider.setValue(value, juce::dontSendNotification);
    control.slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 62, 20);
    control.slider.onValueChange = [this] { parametersChanged(); };
    control.label.setText(name, juce::dontSendNotification);
    control.label.setJustificationType(juce::Justification::centred);
    control.label.setFont(12.0f);
    addAndMakeVisible(control.slider);
    addAndMakeVisible(control.label);
}

ValveParameters ControlPanel::getParameters() const
{
    ValveParameters parameters;
    parameters.drive = static_cast<float>(controls[0].slider.getValue());
    parameters.character = static_cast<float>(controls[1].slider.getValue());
    parameters.warmth = static_cast<float>(controls[2].slider.getValue());
    parameters.mix = static_cast<float>(controls[3].slider.getValue());
    parameters.output = static_cast<float>(controls[4].slider.getValue());
    return parameters;
}

void ControlPanel::setParameters(const ValveParameters& parameters)
{
    const juce::ScopedValueSetter<bool> guard(updatingControls, true);
    controls[0].slider.setValue(parameters.drive, juce::dontSendNotification);
    controls[1].slider.setValue(parameters.character, juce::dontSendNotification);
    controls[2].slider.setValue(parameters.warmth, juce::dontSendNotification);
    controls[3].slider.setValue(parameters.mix, juce::dontSendNotification);
    controls[4].slider.setValue(parameters.output, juce::dontSendNotification);
}

void ControlPanel::parametersChanged()
{
    if (! updatingControls && onParametersChanged != nullptr)
        onParametersChanged(getParameters());
}

void ControlPanel::resized()
{
    auto area = getLocalBounds().reduced(12);
    auto presets = area.removeFromTop(34);
    presetSelector.setBounds(presets.removeFromLeft(160));
    presets.removeFromLeft(8);
    savePresetButton.setBounds(presets.removeFromLeft(86));
    presets.removeFromLeft(6);
    loadPresetButton.setBounds(presets.removeFromLeft(86));
    presets.removeFromLeft(12);
    abButton.setBounds(presets.removeFromLeft(130));

    area.removeFromTop(6);
    const auto controlWidth = area.getWidth() / static_cast<int>(controls.size());
    for (auto& control : controls)
    {
        auto bounds = area.removeFromLeft(controlWidth);
        control.label.setBounds(bounds.removeFromTop(22));
        control.slider.setBounds(bounds.reduced(4, 0));
    }
}
