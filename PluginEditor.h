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
    struct Particle
    {
        juce::Point<float> p, previous;
        float angle = 0.0f;
        float shell = 1.0f;
        float phase = 0.0f;
        float speed = 1.0f;
        float brightness = 0.5f;
        float size = 1.0f;
        bool inner = false;
    };

    void timerCallback() override;
    void initialiseParticles();
    float noise (float x, float y, float z) const;

    LSNebulaAudioProcessor& processor;
    juce::Image trail;
    std::vector<Particle> particles;
    juce::Random random;
    float time = 0.0f;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LSNebulaAudioProcessorEditor)
};
