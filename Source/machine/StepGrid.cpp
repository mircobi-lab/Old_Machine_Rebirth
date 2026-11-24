#include "StepGrid.h"

#include <juce_core/juce_core.h>

namespace machine
{

bool isStepCell (int rowFromTop)
{
    return rowFromTop >= 0 && rowFromTop < stepRows;
}

int stepForColumn (int columnFromLeft)
{
    return juce::isPositiveAndBelow (columnFromLeft, stepColumns) ? columnFromLeft : -1;
}

int trackForRow (int rowFromTop)
{
    return isStepCell (rowFromTop) ? rowFromTop : -1;
}

} // namespace machine
