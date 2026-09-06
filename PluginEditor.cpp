#include "PluginEditor.h"
#include <algorithm>
#include <numeric>

LSNebulaAudioProcessorEditor::LSNebulaAudioProcessorEditor (LSNebulaAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    setOpaque (true);
    setResizable (true, true);
    setResizeLimits (400, 400, 1200, 1200);
    setSize (700, 700);
    particles.resize (9000);
    initialiseParticles();
    startTimerHz (60);
}

float LSNebulaAudioProcessorEditor::noise (float x, float y, float z) const
{
    return std::sin (x * 1.7f + z) * 0.55f
         + std::cos (y * 2.3f - z * 0.73f) * 0.30f
         + std::sin ((x + y) * 3.1f + z * 1.21f) * 0.15f;
}

void LSNebulaAudioProcessorEditor::initialiseParticles()
{
    constexpr auto goldenAngle = 2.39996323f;
    const auto count = static_cast<float> (particles.size());
    for (size_t i = 0; i < particles.size(); ++i)
    {
        auto& q = particles[i];
        const auto fi = static_cast<float> (i);
        const auto y = 1.0f - 2.0f * ((fi + 0.5f) / count);
        const auto horizontal = std::sqrt (juce::jmax (0.0f, 1.0f - y * y));
        const auto longitude = goldenAngle * fi;
        q.direction = { std::cos (longitude) * horizontal, y, std::sin (longitude) * horizontal };
        q.longitude = std::atan2 (q.direction.z, q.direction.x);
        q.latitude = std::asin (q.direction.y);
        const auto layerChoice = random.nextFloat();
        q.layer = layerChoice < 0.84f ? 1.0f : (layerChoice < 0.95f ? 0.94f : 0.86f);
        const auto frequencyPosition = juce::jlimit (0.0f, 0.999f,
                                                     (q.longitude + juce::MathConstants<float>::pi)
                                                     / juce::MathConstants<float>::twoPi);
        q.band = static_cast<int> (frequencyPosition * LSNebulaAudioProcessor::spectrumBandCount);
        q.phase = random.nextFloat() * juce::MathConstants<float>::twoPi;
        q.speed = 0.75f + random.nextFloat() * 0.50f;
        q.brightness = 0.72f + random.nextFloat() * 0.28f;
        q.size = 0.48f + random.nextFloat() * 0.48f;
    }
}

void LSNebulaAudioProcessorEditor::timerCallback()
{
    const auto w = getWidth(), h = getHeight();
    if (w <= 0 || h <= 0) return;
    if (! trail.isValid() || trail.getWidth() != w || trail.getHeight() != h)
        trail = juce::Image (juce::Image::RGB, w, h, true);

    const auto mid = processor.getMid();
    time += 0.0045f + mid * 0.0040f;
    juce::Graphics g (trail);
    g.fillAll (juce::Colours::black);

    const juce::Point<float> centre (w * 0.5f, h * 0.5f);
    const auto boundary = juce::jmin (w, h) * 0.355f;
    const auto rotateY = time * 0.37f;
    const auto rotateX = std::sin (time * 0.21f) * 0.23f;
    const auto cy = std::cos (rotateY), sy = std::sin (rotateY);
    const auto cx = std::cos (rotateX), sx = std::sin (rotateX);

    // A permanent circle made from tiny particles, not a blurred solid shape.
    const auto guideColour = juce::Colour::fromRGB (237, 28, 36);
    const auto guideRadius = boundary * 0.90f;
    for (int i = 0; i < 420; ++i)
    {
        const auto a = juce::MathConstants<float>::twoPi * static_cast<float> (i) / 420.0f;
        const auto x = centre.x + std::cos (a) * guideRadius;
        const auto y = centre.y + std::sin (a) * guideRadius;
        g.setColour (guideColour.withAlpha (0.24f));
        g.fillEllipse (x - 0.30f, y - 0.30f, 0.60f, 0.60f);
    }

    for (auto& q : particles)
    {
        const auto energy = processor.getSpectrumBand (q.band);
        const auto stereoWidth = processor.getSpectrumWidth (q.band);
        const auto frequencyPosition = static_cast<float> (q.band)
                                     / static_cast<float> (LSNebulaAudioProcessor::spectrumBandCount - 1);
        const auto motionRate = juce::jmap (frequencyPosition, 0.58f, 2.35f);
        const auto localTime = time * motionRate;
        const auto spatialDetail = juce::jmap (frequencyPosition, 2.0f, 7.2f);
        // Shared phase fields create the coherent liquid sheets visible in the
        // reference. Individual random phase is only a tiny surface texture.
        const auto waveA = std::sin (q.longitude * spatialDetail
                                   + q.latitude * 2.4f - localTime * 1.75f);
        const auto waveB = std::sin (q.latitude * (spatialDetail + 1.8f)
                                   - q.longitude * 1.7f + localTime * 1.28f);
        const auto waveC = std::sin ((q.longitude + q.latitude) * 3.1f
                                   + localTime * 0.82f);
        const auto membrane = waveA * 0.58f + waveB * 0.30f + waveC * 0.12f;
        const auto fine = std::sin (q.phase + localTime * (1.0f + frequencyPosition)) * 0.012f;
        const auto movementAmount = juce::jmap (frequencyPosition, 1.22f, 0.78f);
        const auto baseBreathing = 0.025f * membrane;
        const auto audioDeformation = energy * 0.34f * movementAmount * membrane;
        const auto radius = q.layer * (1.0f + baseBreathing + audioDeformation + fine);

        // Each frequency band owns a different pseudo-random direction. The
        // direction drifts slowly so hits scatter organically, never jitter.
        const auto bandSeed = static_cast<float> (q.band) * 1.618034f;
        auto scatterX = std::sin (bandSeed * 2.17f + localTime * 0.31f);
        auto scatterY = std::cos (bandSeed * 1.43f - localTime * 0.27f);
        auto scatterZ = std::sin (bandSeed * 2.91f + localTime * 0.19f);
        const auto scatterLength = std::sqrt (scatterX * scatterX + scatterY * scatterY
                                            + scatterZ * scatterZ) + 1.0e-6f;
        scatterX /= scatterLength;
        scatterY /= scatterLength;
        scatterZ /= scatterLength;
        const auto scatter = std::pow (energy, 1.35f) * (0.10f + stereoWidth * 0.16f);

        const auto x0 = q.direction.x * radius + scatterX * scatter;
        const auto y0 = q.direction.y * radius + scatterY * scatter;
        const auto z0 = q.direction.z * radius + scatterZ * scatter;
        const auto x1 = x0 * cy + z0 * sy;
        const auto z1 = -x0 * sy + z0 * cy;
        const auto y2 = y0 * cx - z1 * sx;
        const auto z2 = y0 * sx + z1 * cx;
        const auto perspective = 1.0f / (1.70f - z2 * 0.35f);
        const auto scale = boundary * 1.48f * perspective;
        q.previous = q.p;
        q.p = centre + juce::Point<float> (x1 * scale, y2 * scale);
        q.depth = z2;
    }

    std::vector<size_t> order (particles.size());
    std::iota (order.begin(), order.end(), 0);
    std::sort (order.begin(), order.end(), [this] (size_t a, size_t b)
    {
        return particles[a].depth < particles[b].depth;
    });

    const auto nebulaRed = juce::Colour::fromRGB (237, 28, 36);
    for (const auto index : order)
    {
        const auto& q = particles[index];
        const auto colour = nebulaRed;

        const auto front = juce::jmap (juce::jlimit (-1.0f, 1.0f, q.depth), -1.0f, 1.0f, 0.0f, 1.0f);
        const auto layerVisibility = q.layer > 0.95f ? 1.0f : (q.layer > 0.80f ? 0.82f : 0.68f);
        // Appearance never depends on the input: every particle permanently
        // keeps the same red core and a soft red halo. Audio only changes motion.
        const auto idleAlpha = (0.34f + front * 0.42f)
                             * layerVisibility
                             * (0.72f + q.brightness * 0.28f);
        const auto alpha = juce::jlimit (0.30f, 0.88f, idleAlpha);
        const auto dot = juce::jlimit (0.48f, 1.18f, q.size * 0.96f);

        const auto glow = dot * 3.6f;
        g.setColour (nebulaRed.withAlpha ((0.055f + front * 0.055f) * layerVisibility));
        g.fillEllipse (q.p.x - glow * 0.5f, q.p.y - glow * 0.5f, glow, glow);

        g.setColour (colour.withAlpha (alpha));
        g.fillEllipse (q.p.x - dot * 0.5f, q.p.y - dot * 0.5f, dot, dot);
    }
    repaint();
}

void LSNebulaAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black);
    if (trail.isValid())
        g.drawImageAt (trail, 0, 0);
}
