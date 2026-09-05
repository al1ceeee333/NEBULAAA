#include "PluginEditor.h"

LSNebulaAudioProcessorEditor::LSNebulaAudioProcessorEditor (LSNebulaAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    setOpaque (true);
    setResizable (true, true);
    setResizeLimits (400, 400, 1200, 1200);
    setSize (700, 700);

    particles.resize (1800);
    initialiseParticles();
    startTimerHz (60);
}

float LSNebulaAudioProcessorEditor::noise (float x, float y, float z) const
{
    return std::sin (x * 0.021f + std::sin (y * 0.017f + z))
         + std::cos (y * 0.019f - std::sin (x * 0.013f - z * 0.7f));
}

void LSNebulaAudioProcessorEditor::initialiseParticles()
{
    const auto w = static_cast<float> (juce::jmax (1, getWidth()));
    const auto h = static_cast<float> (juce::jmax (1, getHeight()));
    const juce::Point<float> centre (w * 0.5f, h * 0.5f);
    const auto baseRadius = juce::jmin (w, h) * 0.30f;

    for (size_t i = 0; i < particles.size(); ++i)
    {
        auto& q = particles[i];
        q.angle = juce::MathConstants<float>::twoPi * static_cast<float> (i) / static_cast<float> (particles.size())
                + (random.nextFloat() - 0.5f) * 0.08f;
        q.inner = random.nextFloat() < 0.34f;
        q.shell = q.inner ? 0.12f + std::sqrt (random.nextFloat()) * 0.68f
                          : 0.80f + std::sqrt (random.nextFloat()) * 0.18f;
        q.phase = random.nextFloat() * juce::MathConstants<float>::twoPi;
        q.speed = 0.55f + random.nextFloat() * 1.1f;
        q.brightness = 0.25f + random.nextFloat() * 0.75f;
        q.size = 0.45f + random.nextFloat() * 1.35f;
        const auto radius = baseRadius * q.shell;
        q.p = centre + juce::Point<float> (std::cos (q.angle) * radius,
                                           std::sin (q.angle) * radius * 0.94f);
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
    const auto bass = processor.getBass();
    const auto mid = processor.getMid();
    const auto high = processor.getHigh();
    const auto hit = processor.getTransient();
    time += 0.010f + mid * 0.016f;

    juce::Graphics g (trail);
    g.setColour (juce::Colours::black.withAlpha (0.075f));
    g.fillAll();
    const juce::Point<float> centre (w * 0.5f, h * 0.5f);
    const auto boundaryRadius = juce::jmin (w, h) * 0.355f;
    const auto pulse = 1.0f + bass * 0.025f + hit * 0.018f;
    const auto red = juce::Colour::fromRGB (237, 28, 36);

    for (auto& q : particles)
    {
        q.previous = q.p;
        const auto a = q.angle + time * 0.10f * q.speed;
        const auto coarse = std::sin (a * 3.0f + time * 0.9f + q.phase) * (8.0f + mid * 22.0f);
        const auto fine = std::sin (a * 7.0f - time * 1.25f + q.phase * 0.7f) * (4.0f + high * 12.0f);
        const auto drift = noise (a * 90.0f, q.phase * 55.0f, time * 0.75f) * (2.0f + mid * 7.0f);
        const auto shellNoise = q.inner ? drift * 0.45f : coarse + fine + drift;
        const auto desiredRadius = boundaryRadius * q.shell * pulse + shellNoise;
        const auto radius = juce::jlimit (boundaryRadius * 0.05f,
                                          boundaryRadius * 0.985f,
                                          desiredRadius);
        const auto squash = 0.93f + std::sin (time * 0.55f) * 0.035f;
        const auto wobbleX = std::sin (time * 0.7f + q.phase) * (1.0f + mid * 4.0f);
        const auto wobbleY = std::cos (time * 0.6f + q.phase) * (1.0f + mid * 4.0f);
        q.p = centre + juce::Point<float> (std::cos (a) * radius + wobbleX,
                                           std::sin (a) * radius * squash + wobbleY);

        const auto shellBoost = q.inner ? 0.34f : 1.0f;
        const auto alpha = juce::jlimit (0.025f, 0.92f,
                                         (0.20f + level * 0.48f + high * 0.24f)
                                         * q.brightness * shellBoost);

        g.setColour (red.withAlpha (alpha * 0.065f));
        const auto wideHalo = q.size * 11.0f + level * 6.0f;
        g.fillEllipse (q.p.x - wideHalo * 0.5f, q.p.y - wideHalo * 0.5f, wideHalo, wideHalo);

        g.setColour (red.withAlpha (alpha * 0.20f));
        const auto halo = q.size * 5.2f + level * 3.0f;
        g.fillEllipse (q.p.x - halo * 0.5f, q.p.y - halo * 0.5f, halo, halo);

        g.setColour (red.brighter (0.30f).withAlpha (alpha));
        const auto dot = q.size + high * 0.75f;
        g.fillEllipse (q.p.x - dot * 0.5f, q.p.y - dot * 0.5f, dot, dot);

        if (! q.inner)
        {
            g.setColour (red.withAlpha (alpha * 0.27f));
            g.drawLine ({ q.previous, q.p }, 0.45f + q.size * 0.25f);
        }
    }
    repaint();
}

void LSNebulaAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black);
    if (trail.isValid()) g.drawImageAt (trail, 0, 0);

}
