#include "PresetStore.h"

PresetData PresetStore::makeDefaultPreset()
{
    return {};
}

juce::Result PresetStore::validate(const PresetData& preset)
{
    const auto unitValuesValid = std::isfinite(preset.drive) && std::isfinite(preset.character)
        && std::isfinite(preset.warmth) && std::isfinite(preset.mix)
        && preset.drive >= 0.0f && preset.drive <= 1.0f
        && preset.character >= 0.0f && preset.character <= 1.0f
        && preset.warmth >= 0.0f && preset.warmth <= 1.0f
        && preset.mix >= 0.0f && preset.mix <= 1.0f;
    if (preset.version != 1)
        return juce::Result::fail(L"不支持的预设版本");
    if (! unitValuesValid)
        return juce::Result::fail(L"Drive、Character、Warmth 和 Mix 必须位于 0 到 1");
    if (! std::isfinite(preset.outputDb) || preset.outputDb < -24.0f || preset.outputDb > 24.0f)
        return juce::Result::fail(L"Output 必须位于 -24 到 +24 dB");
    return juce::Result::ok();
}

juce::Result PresetStore::save(const juce::File& file, const PresetData& preset)
{
    const auto validation = validate(preset);
    if (validation.failed())
        return validation;
    if (file.getFullPathName().isEmpty())
        return juce::Result::fail(L"预设路径无效");

    auto object = std::make_unique<juce::DynamicObject>();
    object->setProperty("version", preset.version);
    object->setProperty("drive", preset.drive);
    object->setProperty("character", preset.character);
    object->setProperty("warmth", preset.warmth);
    object->setProperty("mix", preset.mix);
    object->setProperty("outputDb", preset.outputDb);

    if (! file.replaceWithText(juce::JSON::toString(juce::var(object.release()))))
        return juce::Result::fail(L"无法写入预设文件");
    return juce::Result::ok();
}

juce::Result PresetStore::load(const juce::File& file, PresetData& preset)
{
    if (! file.existsAsFile())
        return juce::Result::fail(L"预设文件不存在");

    const auto root = juce::JSON::parse(file.loadFileAsString());
    if (! root.isObject())
        return juce::Result::fail(L"预设不是有效的 JSON 对象");

    const auto isNumber = [](const juce::var& value)
    {
        return value.isInt() || value.isInt64() || value.isDouble();
    };
    for (const auto* name : { "version", "drive", "character", "warmth", "mix", "outputDb" })
    {
        if (! root.hasProperty(name) || ! isNumber(root.getProperty(name, {})))
            return juce::Result::fail(L"预设缺少有效字段：" + juce::String(name));
    }

    PresetData loaded;
    loaded.version = static_cast<int>(root.getProperty("version", 0));
    loaded.drive = static_cast<float>(static_cast<double>(root.getProperty("drive", 0.0)));
    loaded.character = static_cast<float>(static_cast<double>(root.getProperty("character", 0.0)));
    loaded.warmth = static_cast<float>(static_cast<double>(root.getProperty("warmth", 0.0)));
    loaded.mix = static_cast<float>(static_cast<double>(root.getProperty("mix", 0.0)));
    loaded.outputDb = static_cast<float>(static_cast<double>(root.getProperty("outputDb", 0.0)));

    const auto validation = validate(loaded);
    if (validation.failed())
        return validation;
    preset = loaded;
    return juce::Result::ok();
}
