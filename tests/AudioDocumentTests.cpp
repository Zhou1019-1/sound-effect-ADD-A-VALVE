#include <JuceHeader.h>
#include "../src/model/AudioDocument.h"
#include "../src/model/PresetStore.h"

namespace
{
    std::shared_ptr<const juce::AudioBuffer<float>> makeAudio(int length = 8)
    {
        auto audio = std::make_shared<juce::AudioBuffer<float>>(1, length);
        for (int sample = 0; sample < length; ++sample)
            audio->setSample(0, sample, static_cast<float>(sample + 1));
        return audio;
    }
}

class AudioDocumentTests : public juce::UnitTest
{
public:
    AudioDocumentTests() : juce::UnitTest("Audio document", "Editing") {}

    void runTest() override
    {
        beginTest("裁剪、删除与撤销重做");
        AudioDocument document;
        document.setAudio(makeAudio(), 48000.0);
        document.setSelection(2, 5);
        expect(document.trimToSelection());
        expectEquals(document.getLengthSamples(), static_cast<juce::int64>(3));
        expectWithinAbsoluteError(document.getAudio()->getSample(0, 0), 3.0f, 0.0001f);
        expect(document.undo());
        expectEquals(document.getLengthSamples(), static_cast<juce::int64>(8));
        expect(document.redo());
        expectEquals(document.getLengthSamples(), static_cast<juce::int64>(3));

        document.undo();
        document.setSelection(2, 5);
        expect(document.deleteSelection());
        expectEquals(document.getLengthSamples(), static_cast<juce::int64>(5));
        expectWithinAbsoluteError(document.getAudio()->getSample(0, 2), 6.0f, 0.0001f);

        beginTest("旧快照不可变且无效选区不修改音频");
        const auto snapshot = document.getAudio();
        document.setSelection(0, 0);
        expect(! document.trimToSelection());
        expect(! document.deleteSelection());
        document.setSelection(0, 2);
        expect(document.applyGainDb(6.0206f));
        expectWithinAbsoluteError(snapshot->getSample(0, 0), 1.0f, 0.0001f);
        expectWithinAbsoluteError(document.getAudio()->getSample(0, 0), 2.0f, 0.001f);

        beginTest("空选区淡入淡出作用于全文件");
        auto ones = std::make_shared<juce::AudioBuffer<float>>(1, 4);
        ones->clear();
        juce::FloatVectorOperations::fill(ones->getWritePointer(0), 1.0f, 4);
        document.setAudio(ones, 48000.0);
        expect(document.fadeSelection(true));
        expectWithinAbsoluteError(document.getAudio()->getSample(0, 0), 0.0f, 0.0001f);
        expectWithinAbsoluteError(document.getAudio()->getSample(0, 1), 1.0f / 3.0f, 0.0001f);
        expectWithinAbsoluteError(document.getAudio()->getSample(0, 3), 1.0f, 0.0001f);
        expect(document.fadeSelection(false));
        expectWithinAbsoluteError(document.getAudio()->getSample(0, 3), 0.0f, 0.0001f);

        beginTest("历史预算淘汰最旧命令");
        document.setAudio(makeAudio(), 48000.0);
        document.setHistoryByteBudget(64 * 1024);
        document.setSelection(0, 1);
        expect(document.applyGainDb(1.0f));
        expect(document.applyGainDb(1.0f));
        expect(document.applyGainDb(1.0f));
        expect(document.canUndo());
        expect(document.undo());
        expect(! document.canUndo());
        expect(document.canRedo());

        beginTest("预设往返和非法值校验");
        const auto file = juce::File::getSpecialLocation(juce::File::tempDirectory)
            .getChildFile("valve-preset-" + juce::String(juce::Random::getSystemRandom().nextInt()) + ".json");
        auto preset = PresetStore::makeDefaultPreset();
        preset.drive = 0.7f;
        preset.outputDb = -3.0f;
        expect(PresetStore::save(file, preset).wasOk());
        PresetData loaded;
        const auto loadResult = PresetStore::load(file, loaded);
        expect(loadResult.wasOk(), loadResult.getErrorMessage());
        expectWithinAbsoluteError(loaded.drive, 0.7f, 0.0001f);
        expectWithinAbsoluteError(loaded.outputDb, -3.0f, 0.0001f);
        expect(file.replaceWithText("{\"version\":1,\"drive\":2,\"character\":0.3,\"warmth\":0.2,\"mix\":1,\"outputDb\":0}"));
        expect(PresetStore::load(file, loaded).failed());
        expect(file.deleteFile());
    }
};

static AudioDocumentTests audioDocumentTests;
