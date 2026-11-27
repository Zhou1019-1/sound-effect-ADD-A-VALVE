#include <JuceHeader.h>
#include <iostream>

class ConsoleTestRunner : public juce::UnitTestRunner
{
protected:
    void logMessage(const juce::String& message) override
    {
        std::cout << message.toStdString() << std::endl;
    }
};

int main(int, char**)
{
    juce::ScopedJuceInitialiser_GUI initialiseJuce;
    ConsoleTestRunner runner;
    runner.setAssertOnFailure(false);
    runner.setPassesAreLogged(false);
    runner.runAllTests();

    auto failures = 0;
    for (int index = 0; index < runner.getNumResults(); ++index)
        failures += runner.getResult(index)->failures;
    return failures == 0 ? 0 : 1;
}
