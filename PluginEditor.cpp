#include "PluginEditor.h"

LSNebulaAudioProcessorEditor::LSNebulaAudioProcessorEditor (LSNebulaAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    setOpaque (true);
    setResizable (true, true);
    setResizeLimits (400, 400, 1200, 1200);
    setSize (700, 700);
    particles.resize (2600);
    initialiseParticles();
    startTimerHz (60);
}

float LSNebulaAudioProcessorEditor::noise (float x, float y, float z) const
{
    return std::sin (x * 0.019f + std::sin (y * 0.027f + z))
         + 0.55f * std::cos (y * 0.023f - std::sin (x * 0.017f - z * 0.71f));
}

void LSNebulaAudioProcessorEditor::initialiseParticles()
{
    const auto w = static_cast<float> (juce::jmax (1, getWidth()));
    const auto h = static_cast<float> (juce::jmax (1, getHeight()));
    const juce::Point<float> centre (w * 0.5f, h * 0.5f);
    const auto boundary = juce::jmin (w, h) * 0.37f;

    for (size_t i = 0; i < particles.size(); ++i)
    {
        auto& q = particles[i];
        q.angle = juce::MathConstants<float>::twoPi * random.nextFloat();
        q.band = static_cast<int> (i % LSNebulaAudioProcessor::spectrumBandCount);
        q.shell = random.nextFloat() < 0.78f ? 0.80f + 0.20f * std::sqrt (random.nextFloat())
                                             : 0.22f + 0.58f * std::sqrt (random.nextFloat());
        q.radius = q.shell;
        q.phase = random.nextFloat() * juce::MathConstants<float>::twoPi;
        q.speed = 0.55f + random.nextFloat() * 1.25f;
        q.inwardSpeed = 0.00022f + random.nextFloat() * 0.00065f;
        q.brightness = 0.28f + random.nextFloat() * 0.72f;
        q.size = 0.22f + random.nextFloat() * 0.62f;
        q.p = centre + juce::Point<float> (std::cos (q.angle) * boundary * q.radius,
                                           std::sin (q.angle) * boundary * q.radius * 0.97f);
        q.previous = q.p;
    }
}

void LSNebulaAudioProcessorEditor::timerCallback()
{
    const auto w = getWidth(), h = getHeight();
    if (w <= 0 || h <= 0) return;
    if (! trail.isValid() || trail.getWidth() != w || trail.getHeight() != h)
    {
        trail = juce::Image (juce::Image::ARGB, w, h, true);
        initialiseParticles();
    }

    const auto level = processor.getLevel();
    const auto mid = processor.getMid();
    const auto hit = processor.getTransient();
    time += 0.0075f + mid * 0.009f;

    juce::Graphics g (trail);
    g.setColour (juce::Colours::black.withAlpha (0.115f));
    g.fillAll();

    const juce::Point<float> centre (w * 0.5f, h * 0.5f);
    const auto boundary = juce::jmin (w, h) * 0.37f;
    const auto lowColour = juce::Colour::fromRGB (112, 3, 13);
    const auto midColour = juce::Colour::fromRGB (237, 28, 36);
    const auto highColour = juce::Colour::fromRGB (255, 105, 113);

    for (auto& q : particles)
    {
        q.previous = q.p;
        const auto spectrum = processor.getSpectrumBand (q.band);
        const auto frequencyPosition = static_cast<float> (q.band)
                                     / static_cast<float> (LSNebulaAudioProcessor::spectrumBandCount - 1);
        const auto a = q.angle + time * (0.07f + frequencyPosition * 0.05f) * q.speed;

        q.radius -= q.inwardSpeed * (0.65f + spectrum * 8.5f + hit * 3.0f);
        if (q.radius < 0.18f)
            q.radius = 0.94f + random.nextFloat() * 0.055f;

        const auto broadWave = std::sin (a * 3.0f + time * 1.15f + q.phase) * (0.035f + spectrum * 0.25f);
        const auto fineWave = std::sin (a * 7.0f - time * 1.8f + q.phase * 0.63f) * (0.018f + spectrum * 0.12f);
        const auto organic = noise (a * 67.0f, q.phase * 43.0f, time) * (0.010f + spectrum * 0.065f);
        const auto shellWeight = juce::jlimit (0.0f, 1.0f, (q.radius - 0.45f) * 2.2f);
        const auto radial = juce::jlimit (0.10f, 0.995f,
                                         q.radius + (broadWave + fineWave + organic) * shellWeight);
        const auto squash = 0.96f + 0.025f * std::sin (time * 0.43f);
        q.p = centre + juce::Point<float> (std::cos (a) * boundary * radial,
                                           std::sin (a) * boundary * radial * squash);

        juce::Colour colour;
        if (frequencyPosition < 0.45f)
            colour = lowColour.interpolatedWith (midColour, frequencyPosition / 0.45f);
        else
            colour = midColour.interpolatedWith (highColour, (frequencyPosition - 0.45f) / 0.55f);

        const auto edge = juce::jmap (radial, 0.10f, 1.0f, 0.24f, 1.0f);
        const auto alpha = juce::jlimit (0.018f, 0.98f,
                                         (0.070f + spectrum * 1.18f + level * 0.20f)
                                         * q.brightness * edge);
        const auto dot = q.size * (0.70f + spectrum * 1.20f);

        g.setColour (colour.withAlpha (alpha * 0.10f));
        const auto glow = dot * (6.0f + spectrum * 12.0f);
        g.fillEllipse (q.p.x - glow * 0.5f, q.p.y - glow * 0.5f, glow, glow);
        g.setColour (colour.brighter (0.38f).withAlpha (alpha));
        g.fillEllipse (q.p.x - dot * 0.5f, q.p.y - dot * 0.5f, dot, dot);

        if (spectrum > 0.42f && radial > 0.62f)
        {
            g.setColour (colour.withAlpha (alpha * 0.18f));
            g.drawLine ({ q.previous, q.p }, 0.28f);
        }
    }
    repaint();
}

void LSNebulaAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black);
    if (trail.isValid())
        g.drawImageAt (trail, 0, 0);
}
