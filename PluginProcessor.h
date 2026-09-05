#pragma once

#include <JuceHeader.h>
#include <atomic>
#include <array>

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
    static constexpr int spectrumBandCount = 32;
    float getSpectrumBand (int index) const noexcept
    {
        return spectrumBands[static_cast<size_t> (juce::jlimit (0, spectrumBandCount - 1, index))].load();
    }
    float getSpectrumDecibels (int index) const noexcept
    {
        return spectrumDecibels[static_cast<size_t> (juce::jlimit (0, spectrumBandCount - 1, index))].load();
    }

private:
    static constexpr int fftOrder = 11;
    static constexpr int fftSize = 1 << fftOrder;
    void analyseSpectrum() noexcept;

    juce::dsp::LinkwitzRileyFilter<float> lowPass, highPass;
    juce::dsp::FFT forwardFFT { fftOrder };
    juce::dsp::WindowingFunction<float> fftWindow { fftSize, juce::dsp::WindowingFunction<float>::hann };
    std::array<float, fftSize * 2> fftData {};
    std::array<std::atomic<float>, spectrumBandCount> spectrumBands;
    std::array<std::atomic<float>, spectrumBandCount> spectrumDecibels;
    int fftWritePosition = 0;
    double currentSampleRate = 44100.0;
    std::atomic<float> level { 0.0f }, bass { 0.0f }, mid { 0.0f };
    std::atomic<float> high { 0.0f }, transient { 0.0f };
    float envelope = 0.0f;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LSNebulaAudioProcessor)
};
