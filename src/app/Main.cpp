#include <JuceHeader.h>
#include "MainComponent.h"

class AddAValveApplication : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override { return "ADD A VALVE"; }
    const juce::String getApplicationVersion() override { return "0.1.1"; }
    bool moreThanOneInstanceAllowed() override { return true; }

    void initialise(const juce::String&) override
    {
        mainWindow = std::make_unique<MainWindow>(getApplicationName());
    }

    void shutdown() override
    {
        mainWindow.reset();
    }

    void systemRequestedQuit() override
    {
        quit();
    }

    void anotherInstanceStarted(const juce::String&) override
    {
        if (mainWindow != nullptr)
            mainWindow->toFront(true);
    }

private:
    class MainWindow : public juce::DocumentWindow
    {
    public:
        explicit MainWindow(const juce::String& name)
            : juce::DocumentWindow(name, juce::Colour(0xff171a1e), juce::DocumentWindow::allButtons)
        {
            setUsingNativeTitleBar(true);
            setResizable(true, true);
            setResizeLimits(960, 640, 2400, 1600);
            setContentOwned(new MainComponent(), true);
            centreWithSize(1280, 800);
            setVisible(true);
        }

        void closeButtonPressed() override
        {
            juce::JUCEApplication::getInstance()->systemRequestedQuit();
        }
    };

    std::unique_ptr<MainWindow> mainWindow;
};

START_JUCE_APPLICATION(AddAValveApplication)
