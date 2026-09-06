#include "PluginEditor.h"
#include "HorseAssets.h"

#include <cmath>

namespace
{
constexpr int numberOfHorseFrames = 29;
constexpr int spriteColumns = 8;
constexpr int spriteRows = 4;
constexpr int sphereParticleCount = 10800;
constexpr int emittedParticleCount = 2600;
constexpr juce::uint32 backgroundColour = 0xffed1c24u;

float smoothingCoefficient (float deltaSeconds, float timeSeconds)
{
    return 1.0f - std::exp (-deltaSeconds / juce::jmax (0.001f, timeSeconds));
}
}

LSNebulaAudioProcessorEditor::LSNebulaAudioProcessorEditor (LSNebulaAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    horseSheet = juce::ImageFileFormat::loadFrom (HorseGallop_png,
                                                   HorseGallop_png_len);

    // The embedded optical-flow atlas contains a solid red background. Convert
    // its darkness relative to that red into alpha once at load time. Without
    // this keying pass, every sprite cell paints a red rectangle over the
    // particle sphere.
    if (horseSheet.isValid())
    {
        juce::Image::BitmapData bitmap (horseSheet,
                                        juce::Image::BitmapData::readWrite);

        for (int y = 0; y < bitmap.height; ++y)
        {
            for (int x = 0; x < bitmap.width; ++x)
            {
                const auto source = bitmap.getPixelColour (x, y);
                if (source.getAlpha() == 0)
                    continue;

                const auto darkness = juce::jlimit
                (
                    0.0f, 1.0f,
                    (237.0f - static_cast<float> (source.getRed())) / 237.0f
                );
                const auto alpha = juce::jlimit (0.0f, 1.0f,
                                                 (darkness - 0.012f) * 1.18f);
                bitmap.setPixelColour (x, y,
                                       juce::Colours::black.withAlpha (alpha));
            }
        }
    }

    initialiseParticles();

    setOpaque (true);
    setResizable (true, true);
    setResizeLimits (480, 360, 1400, 1050);
    setSize (800, 600);

    vBlank = std::make_unique<juce::VBlankAttachment>
    (
        this,
        [this] (double presentationTimeSeconds)
        {
            onVBlank (presentationTimeSeconds);
        }
    );
}

void LSNebulaAudioProcessorEditor::initialiseParticles()
{
    particles.clear();
    particles.reserve (sphereParticleCount + emittedParticleCount);

    juce::Random random (7726);

    for (int i = 0; i < sphereParticleCount; ++i)
    {
        Particle particle;
        particle.angle = random.nextFloat() * juce::MathConstants<float>::twoPi;
        particle.radius = 0.25f + 0.75f * std::pow (random.nextFloat(), 0.33f);
        particle.depth = random.nextFloat();
        particle.phase = random.nextFloat() * juce::MathConstants<float>::twoPi;
        particle.drift = 0.035f + random.nextFloat() * 0.095f;
        particle.alpha = 0.42f + random.nextFloat() * 0.53f;
        particle.size = random.nextFloat() < 0.84f ? 1.0f : 2.0f;
        particle.band = i % 3;
        particles.push_back (particle);
    }

    for (int i = 0; i < emittedParticleCount; ++i)
    {
        Particle particle;
        particle.angle = random.nextFloat() * juce::MathConstants<float>::twoPi;
        particle.radius = 0.06f + random.nextFloat() * 0.72f;
        particle.depth = random.nextFloat();
        particle.phase = random.nextFloat();
        particle.drift = 0.12f + random.nextFloat() * 0.30f;
        particle.alpha = 0.36f + random.nextFloat() * 0.56f;
        particle.size = random.nextFloat() < 0.88f ? 1.0f : 2.0f;
        particle.band = i % 3;
        particle.emitted = true;
        particles.push_back (particle);
    }
}

void LSNebulaAudioProcessorEditor::onVBlank (double presentationTimeSeconds)
{
    if (lastPresentationTime <= 0.0)
    {
        lastPresentationTime = presentationTimeSeconds;
        repaint();
        return;
    }

    const auto delta = juce::jlimit (0.0f, 0.05f,
                                     static_cast<float> (presentationTimeSeconds
                                                         - lastPresentationTime));
    lastPresentationTime = presentationTimeSeconds;

    const auto bpm = juce::jlimit (30.0f, 300.0f, processor.getHostBpm());
    animationPhase = std::fmod (animationPhase + delta * bpm / 60.0f, 1.0f);
    motionTime += delta;

    const auto follow = [delta] (float current, float target,
                                 float attack, float release)
    {
        const auto coefficient = smoothingCoefficient (delta,
                                                        target > current ? attack : release);
        return current + (target - current) * coefficient;
    };

    smoothBass = follow (smoothBass, processor.getBass(), 0.028f, 0.22f);
    smoothMid  = follow (smoothMid,  processor.getMid(),  0.020f, 0.16f);
    smoothHigh = follow (smoothHigh, processor.getHigh(), 0.012f, 0.10f);
    repaint();
}

void LSNebulaAudioProcessorEditor::resized()
{
    particleLayer = juce::Image();
}

void LSNebulaAudioProcessorEditor::drawHorseFrame (juce::Graphics& g, int frame,
                                                     juce::Rectangle<float> destination,
                                                     float opacity)
{
    if (! horseSheet.isValid())
        return;

    frame = ((frame % numberOfHorseFrames) + numberOfHorseFrames)
          % numberOfHorseFrames;

    const auto sourceW = horseSheet.getWidth() / spriteColumns;
    const auto sourceH = horseSheet.getHeight() / spriteRows;
    const auto sourceX = (frame % spriteColumns) * sourceW;
    const auto sourceY = (frame / spriteColumns) * sourceH;

    g.setOpacity (opacity);
    g.drawImage (horseSheet,
                 destination.getX(), destination.getY(),
                 destination.getWidth(), destination.getHeight(),
                 sourceX, sourceY, sourceW, sourceH);
    g.setOpacity (1.0f);
}

void LSNebulaAudioProcessorEditor::renderParticleLayer (juce::Rectangle<float> bounds)
{
    const auto width = getWidth();
    const auto height = getHeight();

    if (width <= 0 || height <= 0)
        return;

    if (! particleLayer.isValid()
        || particleLayer.getWidth() != width
        || particleLayer.getHeight() != height)
    {
        particleLayer = juce::Image (juce::Image::ARGB, width, height, true);
    }
    else
    {
        particleLayer.clear (particleLayer.getBounds(), juce::Colours::transparentBlack);
    }

    juce::Image::BitmapData pixels (particleLayer,
                                    juce::Image::BitmapData::readWrite);

    const auto centre = bounds.getCentre();
    const auto sphereRadius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.432f;
    const float energies[] =
    {
        juce::jlimit (0.0f, 1.0f, std::sqrt (juce::jmax (0.0f, smoothBass))),
        juce::jlimit (0.0f, 1.0f, std::sqrt (juce::jmax (0.0f, smoothMid))),
        juce::jlimit (0.0f, 1.0f, std::sqrt (juce::jmax (0.0f, smoothHigh)))
    };

    const auto putBlackPixel = [&pixels, width, height]
        (int x, int y, float alpha, int particleSize)
    {
        const auto a = juce::jlimit (0.0f, 1.0f, alpha);
        if (a <= 0.005f)
            return;

        const auto colour = juce::Colours::black.withAlpha (a);
        const auto extent = juce::jlimit (1, 2, particleSize);

        for (int py = 0; py < extent; ++py)
        {
            const auto yy = y + py;
            if (yy < 0 || yy >= height)
                continue;

            for (int px = 0; px < extent; ++px)
            {
                const auto xx = x + px;
                if (xx < 0 || xx >= width)
                    continue;

                pixels.setPixelColour (xx, yy,
                                       pixels.getPixelColour (xx, yy).overlaidWith (colour));
            }
        }
    };

    for (const auto& particle : particles)
    {
        const auto energy = energies[particle.band];
        auto angle = particle.angle;
        auto radiusNormalised = particle.radius;
        auto trailAngle = angle;
        auto trailRadius = radiusNormalised;

        if (particle.band == 0)
        {
            // BASS (0-100 Hz): particles launch from the centre and travel
            // radially outwards, stopping at the sphere's outer safety zone.
            const auto travelSpeed = particle.drift
                                   * (0.16f + energy * 1.35f)
                                   * (particle.emitted ? 1.45f : 0.82f);
            radiusNormalised = 0.055f
                             + std::fmod (particle.radius + motionTime * travelSpeed,
                                          0.905f);
            angle += std::sin (particle.phase + motionTime * 0.24f) * 0.018f;
            trailRadius = juce::jmax (0.05f,
                                      radiusNormalised - (0.005f + energy * 0.020f));
            trailAngle = angle;
        }
        else if (particle.band == 1)
        {
            // MIDS: the shell stays recognisable, while short audio-driven
            // impulses make local particle groups jump outwards.
            const auto jumpWave = juce::jmax
            (
                0.0f,
                std::sin (motionTime * 7.2f + particle.phase * 1.73f)
            );
            const auto jump = std::pow (jumpWave, 7.0f) * energy
                            * (particle.emitted ? 0.145f : 0.085f);
            radiusNormalised = juce::jmin (0.985f,
                                           particle.radius * 0.92f + 0.045f + jump);
            angle -= motionTime * particle.drift * 0.18f;
            trailRadius = juce::jmax (0.05f, radiusNormalised - jump * 0.34f);
            trailAngle = angle;
        }
        else
        {
            // HIGHS: fast tangential movement around the circumference reads
            // immediately differently from the radial bass motion.
            radiusNormalised = 0.46f + particle.radius * 0.52f
                             + std::sin (particle.phase + motionTime * 1.8f) * 0.008f;
            radiusNormalised = juce::jlimit (0.46f, 0.985f, radiusNormalised);
            const auto orbitSpeed = particle.drift * (1.25f + energy * 8.0f)
                                  * (particle.emitted ? 1.32f : 1.0f);
            angle += motionTime * orbitSpeed;
            trailAngle = angle - (0.006f + energy * 0.026f);
            trailRadius = radiusNormalised;
        }

        const auto radius = sphereRadius * radiusNormalised;
        const auto perspective = 0.84f + particle.depth * 0.16f;
        const auto jitter = particle.band == 2 ? energy * 0.75f : energy * 0.35f;
        const auto jitterX = std::sin (particle.phase * 3.7f + motionTime * 17.0f) * jitter;
        const auto jitterY = std::cos (particle.phase * 2.9f + motionTime * 15.0f) * jitter;
        const auto x = centre.x + std::cos (angle) * radius * perspective + jitterX;
        const auto y = centre.y + std::sin (angle) * radius + jitterY;

        const auto edgeDistance = juce::jlimit (0.0f, 1.0f,
                                                (1.02f - radiusNormalised) / 0.10f);
        const auto alpha = particle.alpha
                         * (0.62f + 0.38f * particle.depth)
                         * (0.58f + 0.42f * edgeDistance);
        const auto size = static_cast<int> (particle.size);

        putBlackPixel (juce::roundToInt (x), juce::roundToInt (y), alpha, size);

        if (particle.emitted || energy > 0.10f)
        {
            const auto trailX = centre.x + std::cos (trailAngle)
                               * sphereRadius * trailRadius * perspective;
            const auto trailY = centre.y + std::sin (trailAngle)
                               * sphereRadius * trailRadius;
            putBlackPixel (juce::roundToInt (trailX), juce::roundToInt (trailY),
                           alpha * (0.18f + energy * 0.14f), 1);
        }
    }
}

void LSNebulaAudioProcessorEditor::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    g.fillAll (juce::Colour (backgroundColour));

    const auto horseWidth = juce::jmin (bounds.getWidth() * 0.405f,
                                        bounds.getHeight() * 0.47f * 600.0f / 520.0f);
    const auto horseHeight = horseWidth * 520.0f / 600.0f;
    const juce::Rectangle<float> horseBounds
    (
        bounds.getCentreX() - horseWidth * 0.5f,
        bounds.getCentreY() - horseHeight * 0.5f,
        horseWidth,
        horseHeight
    );

    renderParticleLayer (bounds);
    if (particleLayer.isValid())
        g.drawImageAt (particleLayer, 0, 0);

    const auto exactFrame = animationPhase * static_cast<float> (numberOfHorseFrames);
    const auto frame = static_cast<int> (std::floor (exactFrame)) % numberOfHorseFrames;
    const auto blend = exactFrame - std::floor (exactFrame);

    // Two restrained echoes produce motion blur without smearing the silhouette.
    auto blurBounds = horseBounds.translated (-1.7f, 0.15f);
    drawHorseFrame (g, frame - 2, blurBounds, 0.055f);
    blurBounds = horseBounds.translated (-0.8f, 0.08f);
    drawHorseFrame (g, frame - 1, blurBounds, 0.10f);

    // Crossfade between adjacent optical-flow frames. The atlas has exactly 29
    // valid frames, so the cycle never enters the three blank cells.
    drawHorseFrame (g, frame, horseBounds, 1.0f - blend * 0.46f);
    drawHorseFrame (g, (frame + 1) % numberOfHorseFrames,
                    horseBounds, blend * 0.46f);
}
