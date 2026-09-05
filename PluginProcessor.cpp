#include "PluginProcessor.h"
#include "PluginEditor.h"

LSNebulaAudioProcessor::LSNebulaAudioProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)) {}

void LSNebulaAudioProcessor::prepareToPlay (double sr, int blockSize)
{
    juce::dsp::ProcessSpec spec { sr, static_cast<juce::uint32> (blockSize), 1 };
    lowPass.prepare (spec);
    highPass.prepare (spec);
    lowPass.setType (juce::dsp::LinkwitzRileyFilterType::lowpass);
    highPass.setType (juce::dsp::LinkwitzRileyFilterType::highpass);
    lowPass.setCutoffFrequency (180.0f);
    highPass.setCutoffFrequency (3500.0f);
    lowPass.reset(); highPass.reset(); envelope = 0.0f;
}

bool LSNebulaAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet();
    return in == layouts.getMainOutputChannelSet()
        && (in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo());
}

void LSNebulaAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const auto channels = juce::jmax (1, buffer.getNumChannels());
    const auto samples = buffer.getNumSamples();
    float sum = 0.0f, peak = 0.0f, lowSum = 0.0f, highSum = 0.0f;

    for (int i = 0; i < samples; ++i)
    {
        float mono = 0.0f;
        for (int ch = 0; ch < channels; ++ch) mono += buffer.getSample (ch, i);
        mono /= static_cast<float> (channels);
        const auto lo = lowPass.processSample (0, mono);
        const auto hi = highPass.processSample (0, mono);
        sum += mono * mono; lowSum += lo * lo; highSum += hi * hi;
        peak = juce::jmax (peak, std::abs (mono));
    }

    if (samples == 0) return;
    const auto rms = std::sqrt (sum / samples);
    const auto lo = std::sqrt (lowSum / samples);
    const auto hi = std::sqrt (highSum / samples);
    const auto mi = std::sqrt (juce::jmax (0.0f, rms * rms - lo * lo - hi * hi));
    const auto hit = juce::jmax (0.0f, peak - envelope * 1.35f);
    envelope = 0.92f * envelope + 0.08f * peak;

    auto smooth = [] (std::atomic<float>& dst, float v, float rise, float fall)
    {
        const auto old = dst.load();
        dst.store (old + (v - old) * (v > old ? rise : fall));
    };
    smooth (level, juce::jlimit (0.0f, 1.0f, rms * 4.0f), 0.45f, 0.08f);
    smooth (bass, juce::jlimit (0.0f, 1.0f, lo * 6.0f), 0.35f, 0.06f);
    smooth (mid, juce::jlimit (0.0f, 1.0f, mi * 6.0f), 0.35f, 0.07f);
    smooth (high, juce::jlimit (0.0f, 1.0f, hi * 10.0f), 0.5f, 0.12f);
    smooth (transient, juce::jlimit (0.0f, 1.0f, hit * 8.0f), 0.8f, 0.16f);
}

juce::AudioProcessorEditor* LSNebulaAudioProcessor::createEditor()
{
    return new LSNebulaAudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new LSNebulaAudioProcessor();
}
