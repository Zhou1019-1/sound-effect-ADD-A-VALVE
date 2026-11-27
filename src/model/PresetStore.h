#pragma once

#include <JuceHeader.h>

struct PresetData
{
    int version = 1;
    float drive = 0.25f;
    float character = 0.35f;
    float warmth = 0.25f;
    float mix = 1.0f;
    float outputDb = 0.0f;
};

class PresetStore
{
public:
    static PresetData makeDefaultPreset();
    static juce::Result save(const juce::File& file, const PresetData& preset);
    static juce::Result load(const juce::File& file, PresetData& preset);

private:
    static juce::Result validate(const PresetData& preset);
};
