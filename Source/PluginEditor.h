#pragma once
#include "PluginProcessor.h"
#include "UI/PigeonArt.h"
#include "UI/PigeonFlock.h"
#include "UI/PianoComponent.h"

//==============================================================================
// Eckiger "Beton-Sticker"-Look: harte Schatten, Cremerahmen, Eier als Regler
class TaubenLookAndFeel : public juce::LookAndFeel_V4
{
public:
    TaubenLookAndFeel()
    {
        using namespace tk::pal;
        setColour (juce::PopupMenu::backgroundColourId, night);
        setColour (juce::PopupMenu::textColourId, cream);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, magenta);
        setColour (juce::PopupMenu::highlightedTextColourId, ink);
        setColour (juce::ComboBox::textColourId, cream);
        setColour (juce::TextButton::textColourOffId, ink);
        setColour (juce::TextButton::textColourOnId, ink);
    }

    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool hover, bool down) override
    {
        using namespace tk::pal;
        auto r = b.getLocalBounds().toFloat().reduced (3.f);
        const float off = down ? 1.f : 3.f;
        g.setColour (magenta);
        g.fillRect (r.translated (off, off));
        g.setColour (down ? amber : (hover ? mint : cream));
        g.fillRect (down ? r.translated (2.f, 2.f) : r);
        g.setColour (ink);
        g.drawRect (down ? r.translated (2.f, 2.f) : r, 2.f);
    }

    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool hover, bool) override
    {
        using namespace tk::pal;
        auto r = b.getLocalBounds().toFloat();
        auto box = juce::Rectangle<float> (4.f, r.getCentreY() - 10.f, 20.f, 20.f);
        g.setColour (magenta);
        g.fillRect (box.translated (2.f, 2.f));
        g.setColour (b.getToggleState() ? mint : (hover ? cream.darker (0.1f) : cream));
        g.fillRect (box);
        g.setColour (ink);
        g.drawRect (box, 2.f);
        if (b.getToggleState())                       // kleines Ei als Haken
        {
            g.fillEllipse (box.getCentreX() - 4.f, box.getCentreY() - 5.5f, 8.f, 11.f);
        }
        g.setColour (cream);
        g.setFont (tk::font (14.f, true));
        g.drawText (b.getButtonText(), r.withTrimmedLeft (32.f).toNearestInt(), juce::Justification::centredLeft, false);
    }

    void drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox&) override
    {
        using namespace tk::pal;
        juce::Rectangle<float> r (3.f, 3.f, (float) w - 6.f, (float) h - 6.f);
        g.setColour (magenta);
        g.fillRect (r.translated (3.f, 3.f));
        g.setColour (night);
        g.fillRect (r);
        g.setColour (cream);
        g.drawRect (r, 2.f);
        juce::Path arrow;
        const float cx = r.getRight() - 16.f, cy = r.getCentreY();
        arrow.addTriangle (cx - 6.f, cy - 3.f, cx + 6.f, cy - 3.f, cx, cy + 5.f);
        g.setColour (amber);
        g.fillPath (arrow);
    }

    juce::Font getComboBoxFont (juce::ComboBox&) override { return tk::font (14.f, true); }

    void drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float, float,
                           juce::Slider::SliderStyle, juce::Slider&) override
    {
        using namespace tk::pal;
        const float cy = (float) y + (float) h * 0.5f;
        g.setColour (concrete.brighter (0.2f));
        g.fillRect ((float) x, cy - 3.f, (float) w, 6.f);
        g.setColour (cream);
        g.drawRect ((float) x, cy - 3.f, (float) w, 6.f, 1.5f);
        g.setColour (cream.withAlpha (0.5f));
        g.fillRect ((float) x + (float) w * 0.5f - 1.f, cy - 8.f, 2.f, 16.f);          // 0-Marke
        g.setColour (ink);
        g.fillEllipse (pos - 9.f, cy - 12.f, 18.f, 24.f);
        g.setColour (amber);
        g.fillEllipse (pos - 7.f, cy - 10.f, 14.f, 20.f);
    }
};

//==============================================================================
class TaubenKeyEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit TaubenKeyEditor (TaubenKeyProcessor&);
    ~TaubenKeyEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void renderBackground();
    void drawKeyPanel (juce::Graphics&);
    void drawTunePanel (juce::Graphics&);
    void drawBpmPanel (juce::Graphics&);
    void drawChromaPanel (juce::Graphics&);
    void drawHeaderStatus (juce::Graphics&);
    void updateMascot();

    TaubenKeyProcessor& proc;
    TaubenLookAndFeel lnf;

    juce::Image bg;
    tk::AnalysisResult shown;
    uint32_t lastVersion = 0xffffffffu;
    float gaugeCents = 0.f;

    juce::Rectangle<int> keyArea  { 24, 84, 392, 212 },
                         tuneArea { 432, 84, 236, 212 },
                         bpmArea  { 684, 84, 192, 212 },
                         chromaArea { 24, 312, 852, 116 },
                         pianoArea  { 24, 444, 852, 128 };

    PianoComponent piano;
    PigeonFlock flock;

    juce::ComboBox windowBox;
    juce::ToggleButton freezeBtn { "Einfrieren" }, testBtn { "Gurr-Ton" };
    juce::TextButton resetBtn { "Neu schnueffeln" };
    juce::Slider detuneSlider;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> windowAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>   freezeAtt, testAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>   detuneAtt;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TaubenKeyEditor)
};
