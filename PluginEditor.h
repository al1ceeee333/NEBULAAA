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
    void drawHorse (juce::Graphics&, juce::Point<float>, float scale, float phase);
    void drawLeg (juce::Graphics&, juce::Point<float> hip, float upperAngle,
                  float lowerAngle, float upperLength, float lowerLength, float thickness);

    LSNebulaAudioProcessor& processor;
    float displayPhase = 0.0f;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LSNebulaAudioProcessorEditor)
};
