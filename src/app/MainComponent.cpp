#include "MainComponent.h"

juce::String MainComponent::operationName(EditOperation operation) const
{
    switch (operation)
    {
        case EditOperation::trim: return L"裁剪";
        case EditOperation::remove: return L"删除选区";
        case EditOperation::fadeIn: return L"淡入";
        case EditOperation::fadeOut: return L"淡出";
        case EditOperation::gain: return L"增益";
        case EditOperation::undo: return L"撤销";
        case EditOperation::redo: return L"重做";
    }
    return L"编辑";
}

MainComponent::MainComponent()
    : meterPanel(playback.getMetering()), waveformView(formatManager, thumbnailCache)
{
    setOpaque(true);
    setLookAndFeel(&lookAndFeel);
    lookAndFeel.setColour(juce::Label::textColourId, juce::Colour(0xffdce1e5));
    lookAndFeel.setColour(juce::Slider::textBoxTextColourId, juce::Colour(0xffdce1e5));
    lookAndFeel.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(0xff252b31));
    lookAndFeel.setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xffb17a4d));
    lookAndFeel.setColour(juce::Slider::thumbColourId, juce::Colour(0xffe1b279));
    lookAndFeel.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff343c43));
    lookAndFeel.setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff252b31));

    formatManager.registerBasicFormats();
    const auto initialError = playback.initialise();
    playback.setParameters(controlPanel.getParameters());

    for (auto* button : { &importButton, &deviceButton, &playButton, &stopButton,
                          &undoButton, &redoButton, &trimButton, &deleteButton,
                          &fadeInButton, &fadeOutButton, &gainButton, &exportButton,
                          &cancelExportButton })
        addAndMakeVisible(*button);

    addAndMakeVisible(exportFormat);
    exportFormat.addItem(L"WAV 24-bit PCM", 1);
    exportFormat.addItem(L"WAV 32-bit float", 2);
    exportFormat.addItem(L"FLAC 24-bit（无损）", 3);
    exportFormat.addItem(L"MP3 128 kbps（有损）", 4);
    exportFormat.addItem(L"MP3 192 kbps（有损）", 5);
    exportFormat.addItem(L"MP3 256 kbps（有损）", 6);
    exportFormat.addItem(L"MP3 320 kbps（有损）", 7);
    exportFormat.setSelectedId(1, juce::dontSendNotification);

    for (auto* label : { &fileLabel, &timeLabel, &statusLabel })
        addAndMakeVisible(*label);
    fileLabel.setText(L"未加载音频", juce::dontSendNotification);
    fileLabel.setMinimumHorizontalScale(0.6f);
    timeLabel.setJustificationType(juce::Justification::centredRight);
    statusLabel.setJustificationType(juce::Justification::centredLeft);

    addAndMakeVisible(controlPanel);
    addAndMakeVisible(waveformView);
    addAndMakeVisible(meterPanel);
    addChildComponent(progressBar);
    cancelExportButton.setVisible(false);

    importButton.onClick = [this] { chooseImportFile(); };
    deviceButton.onClick = [this] { showAudioSettings(); };
    playButton.onClick = [this] { playback.isPlaying() ? playback.stop() : playback.play(); };
    stopButton.onClick = [this] { playback.stop(); playback.setPosition(0.0); };
    undoButton.onClick = [this] { performEdit(EditOperation::undo); };
    redoButton.onClick = [this] { performEdit(EditOperation::redo); };
    trimButton.onClick = [this] { performEdit(EditOperation::trim); };
    deleteButton.onClick = [this] { performEdit(EditOperation::remove); };
    fadeInButton.onClick = [this] { performEdit(EditOperation::fadeIn); };
    fadeOutButton.onClick = [this] { performEdit(EditOperation::fadeOut); };
    gainButton.onClick = [this] { chooseGain(); };
    exportButton.onClick = [this] { chooseExportFile(); };
    cancelExportButton.onClick = [this]
    {
        exporter.cancel();
        setStatus(L"正在取消导出…");
    };

    controlPanel.onParametersChanged = [this](const ValveParameters& parameters)
    {
        playback.setParameters(parameters);
    };
    controlPanel.onBypassChanged = [this](bool bypassed) { playback.setBypassed(bypassed); };
    controlPanel.onPresetSelected = [this](int presetId) { applyBuiltInPreset(presetId); };
    controlPanel.onSavePreset = [this] { savePreset(); };
    controlPanel.onLoadPreset = [this] { loadPreset(); };

    waveformView.onSeek = [this](double seconds) { playback.setPosition(seconds); };
    waveformView.onSelectionChanged = [this](juce::int64 start, juce::int64 end)
    {
        document.setSelection(start, end);
        updateButtonStates();
    };

    if (initialError.isNotEmpty())
        setStatus(L"音频设备初始化失败：" + initialError);
    else
        setStatus(L"就绪 · 导入 WAV / FLAC / MP3 开始胆味渲染");

    updateButtonStates();
    startTimerHz(30);
}

MainComponent::~MainComponent()
{
    stopTimer();
    alive.reset();
    activeChooser.reset();
    playback.stop();
    exporter.cancel();
    worker.removeAllJobs(true, 60000);
    playback.shutdown();
    setLookAndFeel(nullptr);
}

void MainComponent::paint(juce::Graphics& graphics)
{
    graphics.fillAll(juce::Colour(0xff171a1e));
}

void MainComponent::resized()
{
    auto area = getLocalBounds().reduced(8);
    auto toolbar = area.removeFromTop(36);
    importButton.setBounds(toolbar.removeFromLeft(92));
    toolbar.removeFromLeft(6);
    deviceButton.setBounds(toolbar.removeFromLeft(82));
    toolbar.removeFromLeft(12);
    playButton.setBounds(toolbar.removeFromLeft(64));
    toolbar.removeFromLeft(5);
    stopButton.setBounds(toolbar.removeFromLeft(64));
    toolbar.removeFromLeft(12);
    undoButton.setBounds(toolbar.removeFromLeft(58));
    toolbar.removeFromLeft(5);
    redoButton.setBounds(toolbar.removeFromLeft(58));
    toolbar.removeFromLeft(8);
    timeLabel.setBounds(toolbar.removeFromLeft(150));
    toolbar.removeFromLeft(8);
    exportFormat.setBounds(toolbar.removeFromRight(190));
    toolbar.removeFromRight(6);
    exportButton.setBounds(toolbar.removeFromRight(68));
    cancelExportButton.setBounds(toolbar.removeFromRight(82));

    area.removeFromTop(5);
    auto editBar = area.removeFromTop(32);
    trimButton.setBounds(editBar.removeFromLeft(102));
    editBar.removeFromLeft(5);
    deleteButton.setBounds(editBar.removeFromLeft(90));
    editBar.removeFromLeft(5);
    fadeInButton.setBounds(editBar.removeFromLeft(62));
    editBar.removeFromLeft(5);
    fadeOutButton.setBounds(editBar.removeFromLeft(62));
    editBar.removeFromLeft(5);
    gainButton.setBounds(editBar.removeFromLeft(70));
    editBar.removeFromLeft(10);
    fileLabel.setBounds(editBar);

    auto status = area.removeFromBottom(25);
    progressBar.setBounds(status.removeFromRight(220).reduced(4, 7));
    statusLabel.setBounds(status);
    controlPanel.setBounds(area.removeFromBottom(178));
    area.removeFromBottom(6);
    meterPanel.setBounds(area.removeFromRight(320));
    area.removeFromRight(6);
    waveformView.setBounds(area);
}

void MainComponent::chooseImportFile()
{
    if (importInProgress || editInProgress || exportInProgress)
        return;

    activeChooser = std::make_unique<juce::FileChooser>(L"选择 WAV / FLAC / MP3 音频", juce::File(),
                                                         L"*.wav;*.flac;*.mp3");
    activeChooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                               [this](const juce::FileChooser& chooser)
                               {
                                   const auto file = chooser.getResult();
                                   if (file.existsAsFile())
                                       importFile(file);
                               });
}

void MainComponent::importFile(juce::File file)
{
    if (! file.hasFileExtension("wav;flac;mp3"))
    {
        setStatus(L"仅支持 WAV、FLAC 和 MP3 文件");
        return;
    }

    importInProgress = true;
    setStatus(L"正在读取 " + file.getFileName() + L" …");
    updateButtonStates();
    std::weak_ptr<bool> weakAlive = alive;

    worker.addJob([this, file = std::move(file), weakAlive]() mutable
    {
        juce::String error;
        double sampleRate = 0.0;
        std::shared_ptr<const juce::AudioBuffer<float>> loadedAudio;

        std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(file));
        if (reader == nullptr)
            error = L"无法识别或文件已损坏";
        else if (reader->numChannels < 1 || reader->numChannels > 2)
            error = L"仅支持单声道或立体声 WAV、FLAC、MP3";
        else if (reader->lengthInSamples <= 0 || reader->lengthInSamples > std::numeric_limits<int>::max())
            error = L"音频为空或超出当前编辑器长度限制";
        else
        {
            sampleRate = reader->sampleRate;
            auto buffer = std::make_shared<juce::AudioBuffer<float>>(
                static_cast<int>(reader->numChannels), static_cast<int>(reader->lengthInSamples));
            if (! reader->read(buffer.get(), 0, static_cast<int>(reader->lengthInSamples), 0, true, true))
                error = L"读取音频样本失败";
            else
                loadedAudio = std::move(buffer);
        }

        if (! weakAlive.expired())
        {
            juce::MessageManager::callAsync([this, weakAlive, file = std::move(file), loadedAudio = std::move(loadedAudio),
                                             sampleRate, error]() mutable
            {
                if (weakAlive.expired())
                    return;
                importInProgress = false;
                if (error.isNotEmpty() || loadedAudio == nullptr)
                    setStatus(L"导入失败：" + error);
                else
                {
                    document.setAudio(loadedAudio, sampleRate, file);
                    playback.loadAudio(*loadedAudio, sampleRate);
                    waveformView.setAudio(loadedAudio, sampleRate);
                    fileLabel.setText(file.getFileName() + L" · " + juce::String(sampleRate / 1000.0, 1) + L" kHz",
                                      juce::dontSendNotification);
                    setStatus(L"导入完成 · 拖动波形创建选区");
                }
                updateButtonStates();
            });
        }
        return;
    });
}

void MainComponent::performEdit(EditOperation operation, float gainDb)
{
    if (document.getAudio() == nullptr || importInProgress || editInProgress || exportInProgress)
        return;

    editInProgress = true;
    setStatus(L"正在执行" + operationName(operation) + L"…");
    updateButtonStates();
    std::weak_ptr<bool> weakAlive = alive;

    worker.addJob([this, operation, gainDb, weakAlive]()
    {
        bool success = false;
        switch (operation)
        {
            case EditOperation::trim: success = document.trimToSelection(); break;
            case EditOperation::remove: success = document.deleteSelection(); break;
            case EditOperation::fadeIn: success = document.fadeSelection(true); break;
            case EditOperation::fadeOut: success = document.fadeSelection(false); break;
            case EditOperation::gain: success = document.applyGainDb(gainDb); break;
            case EditOperation::undo: success = document.undo(); break;
            case EditOperation::redo: success = document.redo(); break;
        }

        if (! weakAlive.expired())
        {
            juce::MessageManager::callAsync([this, weakAlive, operation, success]
            {
                if (weakAlive.expired())
                    return;
                editInProgress = false;
                if (success)
                {
                    refreshDocumentView();
                    setStatus(operationName(operation) + L"完成");
                }
                else
                    setStatus(operationName(operation) + L"未执行：请检查选区或撤销历史");
                updateButtonStates();
            });
        }
        return;
    });
}

void MainComponent::chooseGain()
{
    if (document.getAudio() == nullptr || editInProgress)
        return;

    auto dialog = std::make_unique<juce::AlertWindow>(
        L"应用增益", L"正数提升、负数衰减。空选区时作用于整个文件。", juce::AlertWindow::NoIcon);
    dialog->addTextEditor(L"gain", L"0.0", L"增益 (dB)");
    dialog->addButton(L"确定", 1, juce::KeyPress(juce::KeyPress::returnKey));
    dialog->addButton(L"取消", 0, juce::KeyPress(juce::KeyPress::escapeKey));
    // AlertWindow 的 addButton() 内部会按内容重算高度（说明文字 + 文本框 + 按钮行），
    // 这里不要再写死高度：原来给的 160 在 150% DPI 下比内容矮，底部按钮会被裁掉一半。
    dialog->centreWithSize (juce::jmax (380, dialog->getWidth()), dialog->getHeight());

    auto* dialogPointer = dialog.release();
    std::weak_ptr<bool> weakAlive = alive;
    dialogPointer->enterModalState(true, juce::ModalCallbackFunction::create(
        [this, dialogPointer, weakAlive](int result)
        {
            std::unique_ptr<juce::Component> cleanup(dialogPointer);
            if (weakAlive.expired() || result != 1)
                return;
            const auto value = dialogPointer->getTextEditorContents(L"gain").getFloatValue();
            if (std::isfinite(value) && std::abs(value) <= 96.0f)
                performEdit(EditOperation::gain, value);
            else
                setStatus(L"增益必须是 -96 到 +96 dB 之间的数字");
        }), false);
}

void MainComponent::refreshDocumentView()
{
    const auto audio = document.getAudio();
    if (audio == nullptr)
        return;

    playback.loadAudio(*audio, document.getSampleRate());
    waveformView.setAudio(audio, document.getSampleRate());
    waveformView.setSelection(document.getSelectionStart(), document.getSelectionEnd());
}

AudioExportSettings MainComponent::getExportSettings() const
{
    AudioExportSettings settings;
    switch (exportFormat.getSelectedId())
    {
        case 2: settings.format = AudioExportFormat::wav32Float; break;
        case 3: settings.format = AudioExportFormat::flac24; break;
        case 4:
        case 5:
        case 6:
        case 7:
            settings.format = AudioExportFormat::mp3;
            settings.mp3Bitrate = exportFormat.getSelectedId() == 4 ? 128
                : exportFormat.getSelectedId() == 5 ? 192
                : exportFormat.getSelectedId() == 6 ? 256 : 320;
            break;
        default: settings.format = AudioExportFormat::wav24; break;
    }
    return settings;
}

void MainComponent::chooseExportFile()
{
    if (document.getAudio() == nullptr || importInProgress || editInProgress || exportInProgress)
        return;

    const auto settings = getExportSettings();
    const auto extension = AudioExporter::getFileExtension(settings.format);
    const auto source = document.getSourceFile();
    const auto suggested = source.existsAsFile()
        ? source.getSiblingFile(source.getFileNameWithoutExtension() + L"_valve." + extension)
        : juce::File::getSpecialLocation(juce::File::userMusicDirectory).getChildFile(L"ADD_A_VALVE." + extension);
    const auto title = settings.format == AudioExportFormat::mp3
        ? L"导出 MP3（有损压缩）" : L"导出 " + extension.toUpperCase();
    activeChooser = std::make_unique<juce::FileChooser>(title, suggested, L"*." + extension);
    activeChooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                                   | juce::FileBrowserComponent::warnAboutOverwriting,
                               [this, settings, extension](const juce::FileChooser& chooser)
                               {
                                   auto file = chooser.getResult();
                                   if (file.getFullPathName().isNotEmpty())
                                   {
                                       if (! file.hasFileExtension(extension))
                                           file = file.withFileExtension("." + extension);
                                       exportToFile(file, settings);
                                   }
                               });
}

void MainComponent::exportToFile(juce::File file, AudioExportSettings settings, bool overwriteConfirmed)
{
    const auto sourcePath = document.getSourceFile().getFullPathName();
    if (sourcePath.isNotEmpty() && file.getFullPathName().equalsIgnoreCase(sourcePath))
    {
        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon, L"已保护原文件",
                                               L"导出路径不能与原音频文件相同，请选择其他文件名。");
        return;
    }

    if (file.existsAsFile() && ! overwriteConfirmed)
    {
        std::weak_ptr<bool> weakAlive = alive;
        juce::AlertWindow::showOkCancelBox(
            juce::AlertWindow::WarningIcon, L"确认替换",
            L"目标文件已存在，是否替换？\n" + file.getFullPathName(),
            L"替换", L"取消", this,
            juce::ModalCallbackFunction::create([this, file, settings, weakAlive](int result)
            {
                if (result == 1 && ! weakAlive.expired())
                    exportToFile(file, settings, true);
            }));
        return;
    }

    exportInProgress = true;
    exportProgress = 0.0;
    progressBar.setVisible(true);
    cancelExportButton.setVisible(true);
    exportButton.setVisible(false);
    setStatus(settings.format == AudioExportFormat::mp3 ? L"正在后台导出 MP3（有损）…" : L"正在后台导出…");
    updateButtonStates();
    std::weak_ptr<bool> weakAlive = alive;

    if (! exporter.start(document.getAudio(), document.getSampleRate(), controlPanel.getParameters(),
                         file, settings,
                         [this, weakAlive, file](bool success, bool cancelled, const juce::String& error)
                         {
                             if (weakAlive.expired())
                                 return;
                             exportInProgress = false;
                             progressBar.setVisible(false);
                             cancelExportButton.setVisible(false);
                             exportButton.setVisible(true);
                             if (success)
                                 setStatus(L"导出完成：" + file.getFullPathName());
                             else if (cancelled)
                                 setStatus(L"导出已取消，未生成成品");
                             else
                                 setStatus(L"导出失败：" + error);
                             updateButtonStates();
                         }))
    {
        exportInProgress = false;
        progressBar.setVisible(false);
        cancelExportButton.setVisible(false);
        exportButton.setVisible(true);
        setStatus(L"无法启动导出");
        updateButtonStates();
    }
}

void MainComponent::savePreset()
{
    activeChooser = std::make_unique<juce::FileChooser>(L"保存胆味预设", juce::File(), L"*.valve.json");
    activeChooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                                   | juce::FileBrowserComponent::warnAboutOverwriting,
                               [this](const juce::FileChooser& chooser)
                               {
                                   auto file = chooser.getResult();
                                   if (file.getFullPathName().isEmpty())
                                       return;
                                   if (! file.hasFileExtension("json"))
                                       file = file.withFileExtension(".valve.json");
                                   const auto parameters = controlPanel.getParameters();
                                   PresetData data;
                                   data.drive = parameters.drive;
                                   data.character = parameters.character;
                                   data.warmth = parameters.warmth;
                                   data.mix = parameters.mix;
                                   data.outputDb = parameters.output;
                                   const auto result = PresetStore::save(file, data);
                                   setStatus(result.wasOk() ? L"预设已保存" : L"预设保存失败：" + result.getErrorMessage());
                               });
}

void MainComponent::loadPreset()
{
    activeChooser = std::make_unique<juce::FileChooser>(L"载入胆味预设", juce::File(), L"*.valve.json;*.json");
    activeChooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                               [this](const juce::FileChooser& chooser)
                               {
                                   const auto file = chooser.getResult();
                                   if (! file.existsAsFile())
                                       return;
                                   PresetData data;
                                   const auto result = PresetStore::load(file, data);
                                   if (result.failed())
                                   {
                                       setStatus(L"预设载入失败：" + result.getErrorMessage());
                                       return;
                                   }
                                   ValveParameters parameters;
                                   parameters.drive = data.drive;
                                   parameters.character = data.character;
                                   parameters.warmth = data.warmth;
                                   parameters.mix = data.mix;
                                   parameters.output = data.outputDb;
                                   controlPanel.setParameters(parameters);
                                   playback.setParameters(parameters);
                                   setStatus(L"预设已载入");
                               });
}

void MainComponent::applyBuiltInPreset(int presetId)
{
    ValveParameters parameters;
    if (presetId == 1)
    {
        parameters.drive = 0.16f;
        parameters.character = 0.20f;
        parameters.warmth = 0.15f;
        parameters.mix = 0.85f;
    }
    else if (presetId == 3)
    {
        parameters.drive = 0.65f;
        parameters.character = 0.65f;
        parameters.warmth = 0.55f;
        parameters.mix = 0.90f;
        parameters.output = -1.0f;
    }

    controlPanel.setParameters(parameters);
    playback.setParameters(parameters);
    setStatus(presetId == 1 ? L"预设：清透" : presetId == 3 ? L"预设：浓郁" : L"预设：温暖（默认）");
}

void MainComponent::showAudioSettings()
{
    juce::DialogWindow::LaunchOptions options;
    options.dialogTitle = L"音频设备";
    options.content.setOwned(new juce::AudioDeviceSelectorComponent(
        playback.getDeviceManager(), 0, 0, 1, 2, false, false, false, false));
    options.content->setSize(560, 420);
    options.componentToCentreAround = this;
    options.escapeKeyTriggersCloseButton = true;
    options.useNativeTitleBar = true;
    options.resizable = false;
    options.launchAsync();
}

void MainComponent::updateButtonStates()
{
    const auto hasAudio = document.getAudio() != nullptr;
    const auto busy = importInProgress || editInProgress || exportInProgress;
    const auto hasSelection = document.getSelectionEnd() > document.getSelectionStart();
    importButton.setEnabled(! busy);
    playButton.setEnabled(hasAudio && ! importInProgress && ! editInProgress);
    stopButton.setEnabled(hasAudio);
    undoButton.setEnabled(hasAudio && document.canUndo() && ! busy);
    redoButton.setEnabled(hasAudio && document.canRedo() && ! busy);
    trimButton.setEnabled(hasAudio && hasSelection && ! busy);
    deleteButton.setEnabled(hasAudio && hasSelection && ! busy);
    fadeInButton.setEnabled(hasAudio && ! busy);
    fadeOutButton.setEnabled(hasAudio && ! busy);
    gainButton.setEnabled(hasAudio && ! busy);
    exportButton.setEnabled(hasAudio && ! busy);
    exportFormat.setEnabled(! busy);
}

void MainComponent::setStatus(const juce::String& text)
{
    statusLabel.setText(text, juce::dontSendNotification);
}

juce::String MainComponent::formatTime(double seconds) const
{
    const auto totalSeconds = juce::jmax(0, juce::roundToInt(seconds));
    return juce::String(totalSeconds / 60).paddedLeft('0', 2) + L":"
        + juce::String(totalSeconds % 60).paddedLeft('0', 2) + L"."
        + juce::String(static_cast<int>(seconds * 10.0) % 10);
}

void MainComponent::timerCallback()
{
    const auto position = playback.getPosition();
    waveformView.setPlayheadSeconds(position);
    timeLabel.setText(formatTime(position) + L" / " + formatTime(playback.getLength()), juce::dontSendNotification);
    playButton.setButtonText(playback.isPlaying() ? L"暂停" : L"播放");

    if (exportInProgress)
    {
        exportProgress = exporter.getProgress();
        progressBar.setTextToDisplay(L"导出 " + juce::String(static_cast<int>(exportProgress * 100.0)) + L"%");
    }
}
