#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include <memory>
#include <vector>

class LSNebulaAudioProcessorEditor final : public juce::AudioProcessorEditor
{
public:
    explicit LSNebulaAudioProcessorEditor (LSNebulaAudioProcessor&);
    ~LSNebulaAudioProcessorEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct Particle
    {
        float angle = 0.0f;
        float radius = 0.0f;
        float depth = 0.0f;
        float phase = 0.0f;
        float drift = 0.0f;
        float alpha = 0.0f;
        float size = 1.0f;
        int band = 0;
        bool emitted = false;
    };

    void initialiseParticles();
    void onVBlank (double presentationTimeSeconds);
    void renderParticleLayer (juce::Rectangle<float> bounds);
    void drawHorseFrame (juce::Graphics&, int frame,
                         juce::Rectangle<float> destination, float opacity);

    LSNebulaAudioProcessor& processor;
    std::unique_ptr<juce::VBlankAttachment> vBlank;
    std::vector<Particle> particles;
    juce::Image particleLayer;
    juce::Image horseSheet;

    double lastPresentationTime = 0.0;
    float animationPhase = 0.0f;
    float motionTime = 0.0f;
    float smoothBass = 0.0f;
    float smoothMid = 0.0f;
    float smoothHigh = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LSNebulaAudioProcessorEditor)
};
