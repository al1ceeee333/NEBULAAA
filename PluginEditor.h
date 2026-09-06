#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

class LSNebulaAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                           private juce::Timer
{
public:
    explicit LSNebulaAudioProcessorEditor (LSNebulaAudioProcessor&);
    ~LSNebulaAudioProcessorEditor() override = default;
    void paint (juce::Graphics&) override;
    void resized() override {}

private:
    void timerCallback() override;
    void drawHorseFrame (juce::Graphics&, int frame, juce::Rectangle<float> destination,
                         float opacity = 1.0f);

    LSNebulaAudioProcessor& processor;
    float displayPhase = 0.0f;
    juce::Image horseSheet;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LSNebulaAudioProcessorEditor)
};
