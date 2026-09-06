#include "PluginEditor.h"
#include "HorseAssets.h"

namespace
{
constexpr int numberOfFrames = 8;
constexpr float twoPi = juce::MathConstants<float>::twoPi;
}

LSNebulaAudioProcessorEditor::LSNebulaAudioProcessorEditor (LSNebulaAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    horseSheet = juce::ImageFileFormat::loadFrom (HorseGallop_png,
                                                   HorseGallop_png_len);
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
        displayPhase = std::fmod (static_cast<float> (processor.getHostPpq()), 1.0f);
        if (displayPhase < 0.0f)
            displayPhase += 1.0f;
    }
    else
    {
        displayPhase += (bpm / 60.0f) / 60.0f;
        displayPhase = std::fmod (displayPhase, 1.0f);
    }
    repaint();
}

void LSNebulaAudioProcessorEditor::drawHorseFrame (juce::Graphics& g, int frame,
                                                     juce::Rectangle<float> destination,
                                                     float opacity)
{
    if (! horseSheet.isValid())
        return;

    frame = ((frame % numberOfFrames) + numberOfFrames) % numberOfFrames;
    const auto sheetWidth = horseSheet.getWidth();
    const auto x1 = juce::roundToInt (static_cast<float> (frame) * sheetWidth / numberOfFrames);
    const auto x2 = juce::roundToInt (static_cast<float> (frame + 1) * sheetWidth / numberOfFrames);
    const auto sourceY = juce::roundToInt (horseSheet.getHeight() * 0.30f);
    const auto sourceH = juce::roundToInt (horseSheet.getHeight() * 0.40f);

    g.setOpacity (opacity);
    g.drawImage (horseSheet,
                 destination.getX(), destination.getY(),
                 destination.getWidth(), destination.getHeight(),
                 x1, sourceY, x2 - x1, sourceH);
    g.setOpacity (1.0f);
}

void LSNebulaAudioProcessorEditor::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    g.fillAll (juce::Colour::fromRGB (237, 28, 36));

    const auto frame = static_cast<int> (std::floor (displayPhase * numberOfFrames)) % numberOfFrames;
    const auto gallop = std::sin (displayPhase * twoPi);
    const auto impact = std::pow (std::abs (gallop), 5.0f);
    const auto horseWidth = bounds.getWidth() * 0.82f;
    const auto horseHeight = bounds.getHeight() * 0.54f;
    const auto horseX = bounds.getCentreX() - horseWidth * 0.45f;
    const auto horseY = bounds.getCentreY() - horseHeight * 0.45f
                      + gallop * bounds.getHeight() * 0.018f
                      - impact * bounds.getHeight() * 0.012f;
    const juce::Rectangle<float> horseBounds (horseX, horseY, horseWidth, horseHeight);

    juce::Random random (7726);
    for (int i = 0; i < 90; ++i)
    {
        const auto y = random.nextFloat() * bounds.getHeight();
        const auto x = random.nextFloat() * bounds.getWidth() * 0.72f;
        const auto length = bounds.getWidth() * (0.025f + random.nextFloat() * 0.15f);
        g.setColour (juce::Colours::black.withAlpha (0.025f + random.nextFloat() * 0.065f));
        g.drawLine (x - length, y, x, y, 0.5f + random.nextFloat() * 1.4f);
    }

    drawHorseFrame (g, frame - 2, horseBounds.translated (-bounds.getWidth() * 0.045f, 0.0f), 0.055f);
    drawHorseFrame (g, frame - 1, horseBounds.translated (-bounds.getWidth() * 0.022f, 0.0f), 0.10f);
    drawHorseFrame (g, frame, horseBounds, 1.0f);
}
