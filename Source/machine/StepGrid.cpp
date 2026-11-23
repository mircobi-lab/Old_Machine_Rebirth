#include "StepGrid.h"

#include <juce_core/juce_core.h>

namespace machine
{

bool isStepCell (int rowFromTop)
{
    return rowFromTop >= 0 && rowFromTop < stepRows;
}

int stepForCell (int rowFromTop, int columnFromLeft)
{
    if (! isStepCell (rowFromTop) || ! juce::isPositiveAndBelow (columnFromLeft, stepColumns))
        return -1;

    return rowFromTop * stepColumns + columnFromLeft;
}

std::optional<std::pair<int, int>> cellForStep (int step)
{
    if (! juce::isPositiveAndBelow (step, numSteps))
        return {};

    return std::make_pair (step / stepColumns, step % stepColumns);
}

} // namespace machine
