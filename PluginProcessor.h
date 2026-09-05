#pragma once

#include <JuceHeader.h>
#include <atomic>

class LSNebulaAudioProcessor final : public juce::AudioProcessor
{
public:
    LSNebulaAudioProcessor();
    ~LSNebulaAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override {}
    void setStateInformation (const void*, int) override {}

    float getLevel() const noexcept { return level.load(); }
    float getBass() const noexcept { return bass.load(); }
    float getMid() const noexcept { return mid.load(); }
    float getHigh() const noexcept { return high.load(); }
    float getTransient() const noexcept { return transient.load(); }

private:
    juce::dsp::LinkwitzRileyFilter<float> lowPass, highPass;
    std::atomic<float> level { 0.0f }, bass { 0.0f }, mid { 0.0f };
    std::atomic<float> high { 0.0f }, transient { 0.0f };
    float envelope = 0.0f;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LSNebulaAudioProcessor)
};
