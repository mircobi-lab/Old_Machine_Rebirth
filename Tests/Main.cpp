#include <juce_core/juce_core.h>

// Fa girare tutte le UnitTest registrate e torna 1 se qualcosa non va.
int main (int, char**)
{
    juce::UnitTestRunner runner;
    runner.setAssertOnFailure (false);
    runner.runAllTests();

    int failures = 0;
    for (int i = 0; i < runner.getNumResults(); ++i)
        failures += runner.getResult (i)->failures;

    std::cout << (failures == 0 ? "TUTTO A POSTO" : "CI SONO ERRORI") << ": "
              << runner.getNumResults() << " gruppi, " << failures << " errori" << std::endl;
    return failures == 0 ? 0 : 1;
}
