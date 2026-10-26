#include "Editor.h"
juce::AudioProcessorEditor* SnapProcessor::createEditor(){return new SnapEditor(*this);}
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter(){return new SnapProcessor();}
