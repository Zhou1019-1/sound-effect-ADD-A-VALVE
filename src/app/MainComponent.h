#pragma once

#include <JuceHeader.h>
#include "../audio/PlaybackEngine.h"
#include "../audio/AudioExporter.h"
#include "../model/AudioDocument.h"
#include "../model/PresetStore.h"
#include "../ui/ControlPanel.h"
#include "../ui/MeterPanel.h"
#include "../ui/WaveformView.h"

class MainComponent : public juce::Component, private juce::Timer
{
public:
    MainComponent();
    ~MainComponent() override;

    void paint(juce::Graphics& graphics) override;
    void resized() override;

private:
    enum class EditOperation { trim, remove, fadeIn, fadeOut, gain, undo, redo };

    void chooseImportFile();
    void importFile(juce::File file);
    void performEdit(EditOperation operation, float gainDb = 0.0f);
    void chooseGain();
    void refreshDocumentView();
    void chooseExportFile();
    AudioExportSettings getExportSettings() const;
    void exportToFile(juce::File file, AudioExportSettings settings, bool overwriteConfirmed = false);
    void savePreset();
    void loadPreset();
    void applyBuiltInPreset(int presetId);
    void showAudioSettings();
    void updateButtonStates();
    void setStatus(const juce::String& text);
    void timerCallback() override;
    juce::String operationName(EditOperation operation) const;
    juce::String formatTime(double seconds) const;

    juce::LookAndFeel_V4 lookAndFeel;
    juce::AudioFormatManager formatManager;
    juce::AudioThumbnailCache thumbnailCache { 1 };
    AudioDocument document;
    PlaybackEngine playback;
    ControlPanel controlPanel;
    MeterPanel meterPanel;
    WaveformView waveformView;
    AudioExporter exporter;
    juce::ThreadPool worker { 1 };
    std::shared_ptr<bool> alive = std::make_shared<bool>(true);

    juce::TextButton importButton { L"导入音频" };
    juce::TextButton deviceButton { L"音频设备" };
    juce::TextButton playButton { L"播放" };
    juce::TextButton stopButton { L"停止" };
    juce::TextButton undoButton { L"撤销" };
    juce::TextButton redoButton { L"重做" };
    juce::TextButton trimButton { L"裁剪到选区" };
    juce::TextButton deleteButton { L"删除选区" };
    juce::TextButton fadeInButton { L"淡入" };
    juce::TextButton fadeOutButton { L"淡出" };
    juce::TextButton gainButton { L"增益…" };
    juce::TextButton exportButton { L"导出" };
    juce::TextButton cancelExportButton { L"取消导出" };
    juce::ComboBox exportFormat;
    juce::Label fileLabel;
    juce::Label timeLabel;
    juce::Label statusLabel;
    double exportProgress = 0.0;
    juce::ProgressBar progressBar { exportProgress };
    std::unique_ptr<juce::FileChooser> activeChooser;
    bool importInProgress = false;
    bool editInProgress = false;
    bool exportInProgress = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};
