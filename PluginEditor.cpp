#include "PluginEditor.h"

LSNebulaAudioProcessorEditor::LSNebulaAudioProcessorEditor (LSNebulaAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    setOpaque (true);
    setResizable (true, true);
    setResizeLimits (480, 360, 1400, 1050);
    setSize (800, 600);
    startTimerHz (60);
}

void LSNebulaAudioProcessorEditor::timerCallback()
{
    const auto bpm = juce::jlimit (30.0f, 300.0f, processor.getHostBpm());
    if (processor.getHostIsPlaying())
    {
        const auto ppq = static_cast<float> (processor.getHostPpq());
        displayPhase = std::fmod (ppq * juce::MathConstants<float>::twoPi,
                                  juce::MathConstants<float>::twoPi);
    }
    else
    {
        displayPhase += juce::MathConstants<float>::twoPi * (bpm / 60.0f) / 60.0f;
        displayPhase = std::fmod (displayPhase, juce::MathConstants<float>::twoPi);
    }
    repaint();
}

void LSNebulaAudioProcessorEditor::drawLeg (juce::Graphics& g, juce::Point<float> hip,
                                             float upperAngle, float lowerAngle,
                                             float upperLength, float lowerLength,
                                             float thickness)
{
    const auto knee = hip + juce::Point<float> (std::sin (upperAngle) * upperLength,
                                                 std::cos (upperAngle) * upperLength);
    const auto hoof = knee + juce::Point<float> (std::sin (lowerAngle) * lowerLength,
                                                  std::cos (lowerAngle) * lowerLength);
    g.drawLine ({ hip, knee }, thickness);
    g.drawLine ({ knee, hoof }, thickness * 0.76f);
    g.drawLine (hoof.x - thickness * 0.25f, hoof.y,
                hoof.x + thickness * 1.15f, hoof.y, thickness * 0.48f);
}

void LSNebulaAudioProcessorEditor::drawHorse (juce::Graphics& g, juce::Point<float> centre,
                                               float scale, float phase)
{
    juce::Graphics::ScopedSaveState saved (g);
    g.addTransform (juce::AffineTransform::scale (scale).translated (centre.x, centre.y));
    g.addTransform (juce::AffineTransform::translation (0.0f, std::sin (phase * 2.0f) * 3.5f));
    g.setColour (juce::Colours::black);

    const auto hindPhase = phase + 0.35f;
    const auto frontPhase = phase + juce::MathConstants<float>::pi;
    drawLeg (g, { -67.0f, 25.0f }, -0.72f * std::sin (hindPhase),
             0.95f * std::sin (hindPhase + 0.85f), 56.0f, 53.0f, 13.0f);
    drawLeg (g, { 55.0f, 22.0f }, -0.82f * std::sin (frontPhase),
             1.10f * std::sin (frontPhase + 0.75f), 54.0f, 55.0f, 11.0f);

    g.fillEllipse (-92.0f, -47.0f, 180.0f, 88.0f);
    g.fillEllipse (-72.0f, -59.0f, 128.0f, 91.0f);

    juce::Path neck;
    neck.startNewSubPath (45.0f, -30.0f);
    neck.cubicTo (62.0f, -62.0f, 74.0f, -88.0f, 103.0f, -91.0f);
    neck.lineTo (126.0f, -63.0f);
    neck.cubicTo (99.0f, -58.0f, 91.0f, -28.0f, 77.0f, 4.0f);
    neck.closeSubPath();
    g.fillPath (neck);

    g.fillEllipse (91.0f, -105.0f, 76.0f, 48.0f);
    g.fillEllipse (143.0f, -83.0f, 43.0f, 24.0f);
    juce::Path ears;
    ears.addTriangle (108.0f, -99.0f, 114.0f, -130.0f, 126.0f, -101.0f);
    ears.addTriangle (129.0f, -99.0f, 142.0f, -124.0f, 145.0f, -91.0f);
    g.fillPath (ears);

    drawLeg (g, { 48.0f, 24.0f }, -0.88f * std::sin (phase),
             1.08f * std::sin (phase + 0.72f), 57.0f, 58.0f, 14.0f);
    drawLeg (g, { -57.0f, 26.0f }, -0.78f * std::sin (phase + juce::MathConstants<float>::pi),
             1.00f * std::sin (phase + juce::MathConstants<float>::pi + 0.82f),
             58.0f, 54.0f, 12.0f);

    for (int i = 0; i < 9; ++i)
    {
        const auto offset = static_cast<float> (i) * 3.2f;
        juce::Path tail;
        tail.startNewSubPath (-82.0f, -24.0f + offset);
        tail.cubicTo (-126.0f, -44.0f + offset, -168.0f, -19.0f - offset,
                      -224.0f - i * 4.0f, -31.0f + std::sin (phase + i) * 10.0f);
        g.strokePath (tail, juce::PathStrokeType (8.5f - i * 0.55f));
    }
    for (int i = 0; i < 7; ++i)
    {
        juce::Path mane;
        mane.startNewSubPath (72.0f + i * 4.0f, -70.0f - i * 2.0f);
        mane.cubicTo (41.0f, -89.0f - i * 3.0f, 21.0f - i * 8.0f,
                      -75.0f + std::sin (phase + i * 0.6f) * 12.0f,
                      -13.0f - i * 13.0f, -57.0f + i * 2.0f);
        g.strokePath (mane, juce::PathStrokeType (6.0f - i * 0.45f));
    }
}

void LSNebulaAudioProcessorEditor::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    g.fillAll (juce::Colour::fromRGB (237, 28, 36));

    const auto centre = bounds.getCentre() + juce::Point<float> (bounds.getWidth() * 0.06f,
                                                                  -bounds.getHeight() * 0.01f);
    const auto scale = juce::jmin (bounds.getWidth() / 540.0f, bounds.getHeight() / 420.0f);

    juce::Random streakRandom (7726);
    for (int i = 0; i < 150; ++i)
    {
        const auto y = streakRandom.nextFloat() * bounds.getHeight();
        const auto x = streakRandom.nextFloat() * bounds.getWidth() * 0.72f;
        const auto length = 15.0f + streakRandom.nextFloat() * bounds.getWidth() * 0.23f;
        const auto thickness = 0.35f + streakRandom.nextFloat() * 2.2f;
        const auto distanceToHorse = std::abs (y - centre.y) / bounds.getHeight();
        const auto opacity = (0.035f + streakRandom.nextFloat() * 0.11f)
                           * (1.0f - juce::jlimit (0.0f, 0.8f, distanceToHorse));
        g.setColour (juce::Colours::black.withAlpha (opacity));
        g.drawLine (x - length, y, x, y + streakRandom.nextFloat() * 2.0f - 1.0f, thickness);
    }

    for (int echo = 4; echo >= 1; --echo)
    {
        juce::Graphics::ScopedSaveState saved (g);
        g.setOpacity (0.035f + (4 - echo) * 0.018f);
        drawHorse (g, centre - juce::Point<float> (echo * 18.0f * scale, 0.0f),
                   scale, displayPhase - echo * 0.16f);
    }

    g.setOpacity (1.0f);
    drawHorse (g, centre, scale, displayPhase);
}
