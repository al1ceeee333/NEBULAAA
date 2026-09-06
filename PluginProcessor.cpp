#include "PluginProcessor.h"
#include "PluginEditor.h"

LSNebulaAudioProcessor::LSNebulaAudioProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
    for (auto& band : spectrumBands)
        band.store (0.0f);
    for (auto& band : spectrumDecibels)
        band.store (-100.0f);
    for (auto& width : spectrumWidth)
        width.store (0.0f);
}

void LSNebulaAudioProcessor::prepareToPlay (double sr, int blockSize)
{
    currentSampleRate = sr;
    fftWritePosition = 0;
    fftData.fill (0.0f);
    sideFFTData.fill (0.0f);
    juce::dsp::ProcessSpec spec { sr, static_cast<juce::uint32> (blockSize), 1 };
    lowPass.prepare (spec);
    highPass.prepare (spec);
    lowPass.setType (juce::dsp::LinkwitzRileyFilterType::lowpass);
    highPass.setType (juce::dsp::LinkwitzRileyFilterType::highpass);
    lowPass.setCutoffFrequency (180.0f);
    highPass.setCutoffFrequency (3500.0f);
    lowPass.reset(); highPass.reset(); envelope = 0.0f;
}

void LSNebulaAudioProcessor::analyseSpectrum() noexcept
{
    fftWindow.multiplyWithWindowingTable (fftData.data(), fftSize);
    fftWindow.multiplyWithWindowingTable (sideFFTData.data(), fftSize);
    forwardFFT.performFrequencyOnlyForwardTransform (fftData.data());
    forwardFFT.performFrequencyOnlyForwardTransform (sideFFTData.data());

    constexpr float minimumFrequency = 30.0f;
    const auto maximumFrequency = static_cast<float> (juce::jmin (18000.0, currentSampleRate * 0.48));

    for (int band = 0; band < spectrumBandCount; ++band)
    {
        const auto t0 = static_cast<float> (band) / spectrumBandCount;
        const auto t1 = static_cast<float> (band + 1) / spectrumBandCount;
        const auto f0 = minimumFrequency * std::pow (maximumFrequency / minimumFrequency, t0);
        const auto f1 = minimumFrequency * std::pow (maximumFrequency / minimumFrequency, t1);
        const auto firstBin = juce::jlimit (1, fftSize / 2,
                                            static_cast<int> (f0 * fftSize / currentSampleRate));
        const auto lastBin = juce::jlimit (firstBin, fftSize / 2,
                                           static_cast<int> (f1 * fftSize / currentSampleRate));

        float peakMagnitude = 0.0f;
        float sideMagnitude = 0.0f;
        for (int bin = firstBin; bin <= lastBin; ++bin)
        {
            peakMagnitude = juce::jmax (peakMagnitude, fftData[static_cast<size_t> (bin)]);
            sideMagnitude = juce::jmax (sideMagnitude, sideFFTData[static_cast<size_t> (bin)]);
        }

        const auto totalMagnitude = std::sqrt (peakMagnitude * peakMagnitude
                                             + sideMagnitude * sideMagnitude);

        // A Hann window reduces a bin-centred sine to roughly one quarter of N.
        // Compensating here gives a useful approximate dBFS value for the visual threshold.
        const auto decibels = juce::Decibels::gainToDecibels (totalMagnitude / (fftSize * 0.25f), -100.0f);
        spectrumDecibels[static_cast<size_t> (band)].store (decibels);
        auto target = juce::jlimit (0.0f, 1.0f, juce::jmap (decibels, -84.0f, -24.0f, 0.0f, 1.0f));
        target = std::sqrt (target);
        const auto old = spectrumBands[static_cast<size_t> (band)].load();
        const auto frequencyPosition = static_cast<float> (band) / static_cast<float> (spectrumBandCount - 1);
        const auto attack = juce::jmap (frequencyPosition, 0.76f, 0.96f);
        const auto release = juce::jmap (frequencyPosition, 0.075f, 0.22f);
        const auto coefficient = target > old ? attack : release;
        spectrumBands[static_cast<size_t> (band)].store (old + (target - old) * coefficient);

        const auto widthTarget = sideMagnitude / (peakMagnitude + sideMagnitude + 1.0e-9f);
        const auto oldWidth = spectrumWidth[static_cast<size_t> (band)].load();
        spectrumWidth[static_cast<size_t> (band)].store (oldWidth + (widthTarget - oldWidth) * 0.22f);
    }
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
        const auto left = buffer.getSample (0, i);
        const auto right = channels > 1 ? buffer.getSample (1, i) : left;
        const auto mono = 0.5f * (left + right);
        const auto side = 0.5f * (left - right);

        fftData[static_cast<size_t> (fftWritePosition)] = mono;
        sideFFTData[static_cast<size_t> (fftWritePosition++)] = side;
        if (fftWritePosition == fftSize)
        {
            analyseSpectrum();
            fftWritePosition = 0;
            std::fill (fftData.begin(), fftData.end(), 0.0f);
            std::fill (sideFFTData.begin(), sideFFTData.end(), 0.0f);
        }

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
