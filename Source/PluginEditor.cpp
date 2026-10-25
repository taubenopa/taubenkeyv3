#include "PluginEditor.h"

using namespace tk;
using namespace tk::pal;

namespace
{
    constexpr int kW = 900, kH = 640;

    juce::String bpmText (float bpm)
    {
        return std::abs (bpm - std::round (bpm)) < 0.05f ? juce::String ((int) std::lround (bpm)) : juce::String (bpm, 1);
    }

    // Beton-Sticker-Panel: harter Versatzschatten, Cremerahmen, Titel-Tab
    void drawPanel (juce::Graphics& g, juce::Rectangle<int> area, juce::Colour accent, const juce::String& title)
    {
        auto r = area.toFloat();
        g.setColour (accent.withAlpha (0.85f));
        g.fillRect (r.translated (6.f, 6.f));
        g.setGradientFill (juce::ColourGradient::vertical (night.brighter (0.05f), r.getY(), night.darker (0.25f), r.getBottom()));
        g.fillRect (r);

        g.setColour (cream.withAlpha (0.035f));                      // Beton-Schraffur
        for (float x = r.getX() - r.getHeight(); x < r.getRight(); x += 14.f)
        {
            juce::Graphics::ScopedSaveState ss (g);
            g.reduceClipRegion (area);
            g.drawLine (x, r.getBottom(), x + r.getHeight(), r.getY(), 1.f);
        }
        g.setColour (cream);
        g.drawRect (r, 3.f);

        const auto f = tk::font (12.f, true).withExtraKerningFactor (0.14f);
        const float tw = tk::textWidth (f, title.toUpperCase()) + 20.f;
        juce::Rectangle<float> tab (r.getX() + 14.f, r.getY() - 12.f, tw, 22.f);
        g.setColour (accent);
        g.fillRect (tab);
        g.setColour (ink);
        g.drawRect (tab, 2.f);
        g.setFont (f);
        g.drawText (title.toUpperCase(), tab.toNearestInt(), juce::Justification::centred, false);
    }
}

//==============================================================================
TaubenKeyEditor::TaubenKeyEditor (TaubenKeyProcessor& p)
    : juce::AudioProcessorEditor (&p), proc (p), piano (p.keyboardState)
{
    setLookAndFeel (&lnf);
    setSize (kW, kH);
    setResizable (false, false);

    addAndMakeVisible (piano);

    windowBox.addItemList ({ "Gedaechtnis 6 s", "Gedaechtnis 12 s", "Gedaechtnis 20 s" }, 1);
    addAndMakeVisible (windowBox);
    addAndMakeVisible (freezeBtn);
    addAndMakeVisible (testBtn);
    addAndMakeVisible (resetBtn);

    detuneSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    detuneSlider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    detuneSlider.setDoubleClickReturnValue (true, 0.0);
    addAndMakeVisible (detuneSlider);

    windowAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, "window", windowBox);
    freezeAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>   (proc.apvts, "freeze", freezeBtn);
    testAtt   = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>   (proc.apvts, "testSound", testBtn);
    detuneAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>   (proc.apvts, "testDetune", detuneSlider);

    resetBtn.onClick = [this] { proc.requestReset(); flock.scatter(); };

    addAndMakeVisible (flock);          // zuletzt -> liegt über allem, fängt aber keine Mausklicks

    resized();
    startTimerHz (30);
}

TaubenKeyEditor::~TaubenKeyEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

//==============================================================================
void TaubenKeyEditor::resized()
{
    flock.setBounds (getLocalBounds());
    piano.setBounds (pianoArea.getX() + 14, pianoArea.getY() + 22, pianoArea.getWidth() - 28, 88);

    const int y = 584;
    windowBox.setBounds (24, y, 170, 30);
    freezeBtn.setBounds (210, y, 130, 30);
    resetBtn.setBounds (350, y, 160, 30);
    testBtn.setBounds (528, y, 120, 30);
    detuneSlider.setBounds (730, y, 146, 30);

    flock.setPerches ({
        { 262.f, (float) keyArea.getY() },   { 372.f, (float) keyArea.getY() },
        { 540.f, (float) tuneArea.getY() },  { 630.f, (float) tuneArea.getY() },
        { 770.f, (float) bpmArea.getY() },   { 846.f, (float) bpmArea.getY() },
        { 330.f, (float) chromaArea.getY() },{ 660.f, (float) chromaArea.getY() },
        { 220.f, (float) pianoArea.getY() }, { 720.f, (float) pianoArea.getY() },
        { 560.f, 72.f },                     { 800.f, 72.f } });

    renderBackground();
}

//==============================================================================
void TaubenKeyEditor::renderBackground()
{
    const int w = getWidth(), h = getHeight();
    if (w <= 0 || h <= 0) return;
    bg = juce::Image (juce::Image::ARGB, w, h, true);
    juce::Graphics g (bg);

    g.setGradientFill (juce::ColourGradient::vertical (ink, 0.f, night.darker (0.2f), (float) h));
    g.fillAll();

    // Laternenlicht + magentafarbener Neon-Schein
    g.setGradientFill (juce::ColourGradient (amber.withAlpha (0.20f), (float) w - 120.f, -10.f, amber.withAlpha (0.f), (float) w - 120.f, 430.f, true));
    g.fillRect (0, 0, w, h);
    g.setGradientFill (juce::ColourGradient (magenta.withAlpha (0.13f), 40.f, (float) h, magenta.withAlpha (0.f), 40.f, (float) h - 420.f, true));
    g.fillRect (0, 0, w, h);

    // Raster-Punkte (wachsen nach unten)
    for (int y = 80; y < h; y += 14)
        for (int x = ((y / 14) % 2) * 7; x < w; x += 14)
        {
            const float rad = 0.4f + 1.3f * (float) y / (float) h;
            g.setColour (cream.withAlpha (0.045f));
            g.fillEllipse ((float) x - rad, (float) y - rad, rad * 2.f, rad * 2.f);
        }

    // Betonplatten-Fugen
    g.setColour (ink.withAlpha (0.55f));
    for (int x = 150; x < w; x += 150) g.drawVerticalLine (x, 74.f, (float) h);
    for (int y = 170; y < h; y += 100) g.drawHorizontalLine (y, 0.f, (float) w);

    // Skyline im Header
    juce::Random rng (7);
    for (int x = 0; x < w;)
    {
        const int bw = 18 + rng.nextInt (28), bh = 14 + rng.nextInt (40);
        g.setColour (concrete.darker (0.3f));
        g.fillRect (x, 72 - bh, bw, bh);
        for (int wy = 72 - bh + 5; wy < 68; wy += 8)
            for (int wx = x + 4; wx < x + bw - 4; wx += 7)
                if (rng.nextFloat() < 0.28f)
                {
                    g.setColour ((rng.nextBool() ? amber : cyan).withAlpha (0.35f + 0.4f * rng.nextFloat()));
                    g.fillRect (wx, wy, 3, 4);
                }
        x += bw + rng.nextInt (6);
    }
    g.setColour (magenta);
    g.fillRect (0, 72, w, 4);
    g.setColour (cream);
    g.fillRect (0, 76, w, 2);

    // Logo-Taube + Titel mit "chromatischer" Verschiebung
    tk::PigeonPose logoPose; logoPose.flying = true; logoPose.flap = 0.25f;
    tk::drawPigeon (g, { 58.f, 44.f }, 40.f, true, logoPose, juce::Colour (0xffa3adc4));

    const auto ft = tk::font (46.f, true);
    juce::Rectangle<int> tr (96, 8, 360, 58);
    g.setFont (ft);
    g.setColour (magenta);  g.drawText ("TaubenKey", tr.translated (3, 3), juce::Justification::centredLeft, false);
    g.setColour (cyan);     g.drawText ("TaubenKey", tr.translated (-2, -1), juce::Justification::centredLeft, false);
    g.setColour (cream);    g.drawText ("TaubenKey", tr, juce::Justification::centredLeft, false);

    const float tw = tk::textWidth (ft, "TaubenKey");
    g.setFont (tk::font (14.f, true).withExtraKerningFactor (0.1f));
    g.setColour (amber);
    g.drawText ("von taubenopa", juce::Rectangle<int> (96 + (int) tw + 14, 32, 200, 22), juce::Justification::centredLeft, false);

    // Panel-Hüllen
    drawPanel (g, keyArea,    mint,    "Tonart");
    drawPanel (g, tuneArea,   magenta, "Stimmung");
    drawPanel (g, bpmArea,    amber,   "Tempo");
    drawPanel (g, chromaArea, violet,  "Taubenfutter  -  Tonverteilung");
    drawPanel (g, pianoArea,  cyan,    "Test-Klavier  -  klick mich");
}

//==============================================================================
void TaubenKeyEditor::paint (juce::Graphics& g)
{
    g.drawImageAt (bg, 0, 0);
    drawHeaderStatus (g);
    drawKeyPanel (g);
    drawTunePanel (g);
    drawBpmPanel (g);
    drawChromaPanel (g);

    // Beschriftung der Test-Verstimmung
    const float cents = proc.apvts.getRawParameterValue ("testDetune")->load();
    g.setColour (cream);
    g.setFont (tk::font (12.f, true));
    g.drawText ("Test-Verstimmung: " + juce::String (cents, 1) + " ct", juce::Rectangle<int> (656, 568, 224, 16),
                juce::Justification::centredRight, false);
}

void TaubenKeyEditor::drawHeaderStatus (juce::Graphics& g)
{
    juce::String s; juce::Colour c = grey;
    if (proc.apvts.getRawParameterValue ("freeze")->load() > 0.5f) { s = "EINGEFROREN"; c = cyan; }
    else if (shown.fromMidi)                                       { s = "MIDI-MODUS";  c = violet; }
    else if (shown.signalPresent)                                  { s = "AUDIO  -  " + juce::String (shown.analysedSeconds, 0) + " s"; c = mint; }
    else if (shown.hasKey)                                         { s = "PAUSE  -  GEDAECHTNIS"; c = amber; }
    else                                                           { s = "WARTE AUF TON"; c = grey; }

    const auto f = tk::font (12.f, true).withExtraKerningFactor (0.12f);
    const float w = tk::textWidth (f, s) + 40.f;
    juce::Rectangle<float> chip ((float) getWidth() - 24.f - w, 12.f, w, 26.f);
    g.setColour (ink);          g.fillRect (chip.translated (3.f, 3.f));
    g.setColour (night);        g.fillRect (chip);
    g.setColour (c);            g.drawRect (chip, 2.f);

    const float pulse = shown.signalPresent ? 0.5f + 0.5f * std::sin ((float) juce::Time::getMillisecondCounterHiRes() * 0.008f) : 0.4f;
    g.setColour (c.withAlpha (pulse));
    g.fillEllipse (chip.getX() + 10.f, chip.getCentreY() - 4.f, 8.f, 8.f);
    g.setColour (c);
    g.setFont (f);
    g.drawText (s, chip.withTrimmedLeft (22.f).toNearestInt(), juce::Justification::centred, false);
}

//==============================================================================
void TaubenKeyEditor::drawKeyPanel (juce::Graphics& g)
{
    auto r = keyArea.toFloat();

    if (! shown.hasKey)
    {
        g.setColour (grey.withAlpha (0.7f));
        g.setFont (tk::font (110.f, true));
        g.drawText ("?", juce::Rectangle<float> (r.getX() + 24.f, r.getY() + 20.f, 140.f, 120.f).toNearestInt(), juce::Justification::centredLeft, false);
        g.setFont (tk::font (15.f));
        g.setColour (cream.withAlpha (0.85f));
        g.drawFittedText ("Spiel Audio ab oder tipp auf das Klavier unten - die Tauben horchen mit.",
                          juce::Rectangle<int> (keyArea.getX() + 22, keyArea.getY() + 150, 250, 50), juce::Justification::topLeft, 3);
        return;
    }

    const juce::String tonic = tk::noteName (shown.tonic);
    const auto big = tk::font (96.f, true);
    const float w = tk::textWidth (big, tonic);
    juce::Rectangle<int> tr (keyArea.getX() + 20, keyArea.getY() + 20, 220, 112);
    g.setFont (big);
    g.setColour (magenta);  g.drawText (tonic, tr.translated (4, 4), juce::Justification::centredLeft, false);
    g.setColour (cream);    g.drawText (tonic, tr, juce::Justification::centredLeft, false);

    g.setFont (tk::font (34.f, true));
    g.setColour (amber);
    g.drawText (shown.minor ? "Moll" : "Dur", juce::Rectangle<int> (tr.getX() + (int) w + 10, keyArea.getY() + 64, 120, 44),
                juce::Justification::centredLeft, false);

    // Enharmonik, Parallel-Tonart, Zweit-/Drittplatzierte
    const int x = keyArea.getX() + 22;
    int y = keyArea.getY() + 134;
    g.setFont (tk::font (14.f, true));
    g.setColour (cream.withAlpha (0.9f));
    juce::String alt = juce::String (tk::noteName (shown.tonic)) != juce::String (tk::noteNameFlat (shown.tonic))
                           ? juce::String ("  -  auch: ") + tk::noteNameFlat (shown.tonic) + (shown.minor ? " Moll" : " Dur") : juce::String();
    const int relT = shown.minor ? (shown.tonic + 3) % 12 : (shown.tonic + 9) % 12;
    g.drawText (juce::String ("Parallele: ") + tk::noteName (relT) + (shown.minor ? " Dur" : " Moll") + alt,
                juce::Rectangle<int> (x, y, 250, 18), juce::Justification::centredLeft, false);
    y += 20;
    g.setFont (tk::font (12.f));
    g.setColour (grey);
    auto cand = [] (const tk::KeyCandidate& k) { return juce::String (tk::noteName (k.tonic)) + (k.minor ? " Moll" : " Dur"); };
    g.drawText ("Danach: " + cand (shown.top[1]) + "  -  " + cand (shown.top[2]),
                juce::Rectangle<int> (x, y, 250, 16), juce::Justification::centredLeft, false);

    // Sicherheit als Eier-Reihe
    y += 24;
    g.setFont (tk::font (11.f, true).withExtraKerningFactor (0.1f));
    g.setColour (cream);
    g.drawText ("SICHERHEIT", juce::Rectangle<int> (x, y, 80, 16), juce::Justification::centredLeft, false);
    const int segs = 12, lit = (int) std::lround (shown.keyConfidence * segs);
    const auto col = shown.keyConfidence > 0.66f ? mint : (shown.keyConfidence > 0.4f ? amber : magenta);
    for (int i = 0; i < segs; ++i)
    {
        juce::Rectangle<float> egg ((float) x + 80.f + (float) i * 14.f, (float) y, 10.f, 15.f);
        g.setColour (i < lit ? col : concrete.brighter (0.15f));
        g.fillEllipse (egg);
        g.setColour (ink);
        g.drawEllipse (egg, 1.2f);
    }
    g.setFont (tk::font (11.f, true));
    g.setColour (col);
    g.drawText (juce::String ((int) std::lround (shown.keyConfidence * 100.f)) + " %",
                juce::Rectangle<int> (x + 80 + segs * 14 + 2, y, 40, 16), juce::Justification::centredLeft, false);
}

//==============================================================================
void TaubenKeyEditor::drawTunePanel (juce::Graphics& g)
{
    auto r = tuneArea.toFloat();
    const float cx = r.getCentreX(), cy = r.getY() + 112.f, rad = 78.f, maxA = 1.9f;
    auto ang = [&] (float c) { return juce::jlimit (-50.f, 50.f, c) / 50.f * maxA; };

    auto arc = [&] (float from, float to, juce::Colour c, float th)
    {
        juce::Path p;
        p.addCentredArc (cx, cy, rad, rad, 0.f, from, to, true);
        g.setColour (c);
        g.strokePath (p, juce::PathStrokeType (th, juce::PathStrokeType::curved, juce::PathStrokeType::butt));
    };
    arc (-maxA, maxA, concrete.brighter (0.25f), 10.f);
    arc (ang (-20.f), ang (-5.f), amber.withAlpha (0.55f), 10.f);
    arc (ang (5.f), ang (20.f), amber.withAlpha (0.55f), 10.f);
    arc (ang (-5.f), ang (5.f), mint, 10.f);

    g.setColour (cream.withAlpha (0.8f));
    for (int c = -50; c <= 50; c += 10)
    {
        const float a = ang ((float) c), l0 = rad + 9.f, l1 = rad + (c % 50 == 0 ? 17.f : 13.f);
        g.drawLine (cx + l0 * std::sin (a), cy - l0 * std::cos (a), cx + l1 * std::sin (a), cy - l1 * std::cos (a), c == 0 ? 2.5f : 1.5f);
    }
    g.setFont (tk::font (11.f, true));
    g.setColour (grey);
    g.drawText ("-50", juce::Rectangle<int> ((int) (cx - rad - 30), (int) (cy + 22), 34, 14), juce::Justification::centred, false);
    g.drawText ("+50", juce::Rectangle<int> ((int) (cx + rad - 4), (int) (cy + 22), 34, 14), juce::Justification::centred, false);

    const bool ok = shown.hasTuning && shown.tuningQuality >= 0.25f;
    const float c = gaugeCents;
    const auto needleCol = ! ok ? grey : (std::abs (c) <= 5.f ? mint : (std::abs (c) <= 20.f ? amber : magenta));
    const float a = ang (ok ? c : 0.f);

    // Nadel als Feder
    juce::Path feather;
    feather.startNewSubPath (0.f, 0.f);
    feather.cubicTo (6.f, -rad * 0.35f, 4.f, -rad * 0.8f, 0.f, -rad + 6.f);
    feather.cubicTo (-4.f, -rad * 0.8f, -6.f, -rad * 0.35f, 0.f, 0.f);
    feather.closeSubPath();
    g.setColour (needleCol.withAlpha (ok ? 1.f : 0.45f));
    g.fillPath (feather, juce::AffineTransform::rotation (a).translated (cx, cy));
    g.setColour (ink);
    g.fillEllipse (cx - 8.f, cy - 8.f, 16.f, 16.f);
    g.setColour (cream);
    g.drawEllipse (cx - 8.f, cy - 8.f, 16.f, 16.f, 2.f);

    // Werte
    g.setFont (tk::font (28.f, true));
    g.setColour (ok ? needleCol : grey);
    const juce::String centsText = ok ? juce::String (shown.detuneCents > 0.05f ? "+" : "") + juce::String (shown.detuneCents, 1) + " ct"
                                      : juce::String ("-- ct");
    g.drawText (centsText, juce::Rectangle<int> (tuneArea.getX(), (int) cy + 32, tuneArea.getWidth(), 34), juce::Justification::centred, false);

    juce::String status = "NOCH KEIN TON";
    if (shown.hasTuning)
    {
        if (! ok)                         status = "UNKLAR - DRUMS/RAUSCHEN?";
        else if (std::abs (shown.detuneCents) <= 5.f) status = "SAUBER GESTIMMT";
        else                              status = shown.detuneCents > 0 ? "VERSTIMMT - ZU HOCH" : "VERSTIMMT - ZU TIEF";
    }
    g.setFont (tk::font (12.f, true).withExtraKerningFactor (0.06f));
    g.setColour (ok ? needleCol : grey);
    g.drawText (status, juce::Rectangle<int> (tuneArea.getX(), (int) cy + 68, tuneArea.getWidth(), 18), juce::Justification::centred, false);

    if (ok)
    {
        g.setFont (tk::font (12.f));
        g.setColour (cream.withAlpha (0.8f));
        g.drawText ("A = " + juce::String (shown.referenceHz, 2) + " Hz",
                    juce::Rectangle<int> (tuneArea.getX(), (int) cy + 88, tuneArea.getWidth(), 16), juce::Justification::centred, false);
    }
}

//==============================================================================
void TaubenKeyEditor::drawBpmPanel (juce::Graphics& g)
{
    auto r = bpmArea.toFloat();
    const double now = juce::Time::getMillisecondCounterHiRes();

    if (! shown.hasBpm)
    {
        g.setColour (grey.withAlpha (0.7f));
        g.setFont (tk::font (60.f, true));
        g.drawText ("---", bpmArea.withTrimmedTop (26).withHeight (80), juce::Justification::centred, false);
        g.setFont (tk::font (12.f));
        g.setColour (cream.withAlpha (0.8f));
        g.drawFittedText ("braucht ca. 4 s Audio mit Rhythmus", bpmArea.reduced (18).withTrimmedTop (120), juce::Justification::centredTop, 2);
    }
    else
    {
        const juce::String t = bpmText (shown.bpm);
        const auto f = tk::font (t.length() > 3 ? 54.f : 62.f, true);
        juce::Rectangle<int> tr (bpmArea.getX(), bpmArea.getY() + 24, bpmArea.getWidth(), 72);
        g.setFont (f);
        g.setColour (magenta); g.drawText (t, tr.translated (3, 3), juce::Justification::centred, false);
        g.setColour (cream);   g.drawText (t, tr, juce::Justification::centred, false);

        g.setFont (tk::font (13.f, true).withExtraKerningFactor (0.2f));
        g.setColour (amber);
        g.drawText ("BPM", juce::Rectangle<int> (bpmArea.getX(), bpmArea.getY() + 94, bpmArea.getWidth(), 18), juce::Justification::centred, false);

        // Sicherheit: 5 Körner
        for (int i = 0; i < 5; ++i)
        {
            g.setColour (i < (int) std::lround (shown.bpmConfidence * 5.f) ? amber : concrete.brighter (0.2f));
            g.fillEllipse (r.getCentreX() - 35.f + (float) i * 15.f, r.getY() + 118.f, 9.f, 7.f);
        }
        g.setFont (tk::font (12.f));
        g.setColour (grey);
        g.drawText (juce::String (shown.bpmAlt < shown.bpm ? "oder halb: " : "oder doppelt: ") + bpmText (shown.bpmAlt),
                    juce::Rectangle<int> (bpmArea.getX(), bpmArea.getY() + 132, bpmArea.getWidth(), 16), juce::Justification::centred, false);

        // pulsierendes Ei im Takt (Rate = erkannte BPM; die Phase ist nicht zum Song synchron)
        const double phase = std::fmod (now * (double) shown.bpm / 60000.0, 1.0);
        const float pulse = (float) std::pow (1.0 - phase, 3.0);
        const float rr = 7.f + 8.f * pulse;
        const auto c = juce::Point<float> (r.getCentreX(), r.getBottom() - 30.f);
        g.setColour (amber.withAlpha (0.25f * pulse));
        g.fillEllipse (c.x - rr * 1.9f, c.y - rr * 2.3f, rr * 3.8f, rr * 4.6f);
        g.setColour (ink);    g.fillEllipse (c.x - rr * 0.8f - 2.f, c.y - rr * 1.05f - 2.f, rr * 1.6f + 4.f, rr * 2.1f + 4.f);
        g.setColour (cream);  g.fillEllipse (c.x - rr * 0.8f, c.y - rr * 1.05f, rr * 1.6f, rr * 2.1f);
    }

    const float host = proc.getHostBpm();
    if (host > 0.f)
    {
        g.setFont (tk::font (11.f));
        g.setColour (cyan);
        g.drawText ("Host: " + juce::String (host, 2), juce::Rectangle<int> (bpmArea.getX(), bpmArea.getBottom() - 18, bpmArea.getWidth(), 14),
                    juce::Justification::centred, false);
    }
}

//==============================================================================
void TaubenKeyEditor::drawChromaPanel (juce::Graphics& g)
{
    const auto r = chromaArea.toFloat();
    const float x0 = r.getX() + 30.f, colW = (r.getWidth() - 60.f) / 12.f, base = r.getBottom() - 30.f;
    const double now = juce::Time::getMillisecondCounterHiRes() * 0.001;

    static const int maj[7] = { 0, 2, 4, 5, 7, 9, 11 }, mnr[7] = { 0, 2, 3, 5, 7, 8, 10 };
    std::array<bool, 12> inScale {};
    if (shown.hasKey) for (int i = 0; i < 7; ++i) inScale[(size_t) ((shown.tonic + (shown.minor ? mnr : maj)[i]) % 12)] = true;

    float mx = 0.f;
    for (auto v : shown.chroma) mx = std::max (mx, v);

    static const int caps[8] = { 6, 5, 5, 4, 3, 2, 2, 1 };
    float tonicTop = base;
    for (int pc = 0; pc < 12; ++pc)
    {
        const float cx = x0 + ((float) pc + 0.5f) * colW;
        const bool tonic = shown.hasKey && pc == shown.tonic;
        const float norm = mx > 0.f ? shown.chroma[(size_t) pc] / mx : 0.f;
        int seeds = (int) std::lround (norm * 28.f);

        const auto seedCol = tonic ? mint : (inScale[(size_t) pc] ? amber : grey.darker (0.2f));
        float top = base;
        for (int row = 0; row < 8 && seeds > 0; ++row)
        {
            const int n = std::min (seeds, caps[row]);
            const float y = base - 7.f - (float) row * 6.5f;
            for (int i = 0; i < n; ++i)
            {
                const float x = cx + ((float) i - (float) (n - 1) * 0.5f) * 9.f;
                g.setColour (ink);        g.fillEllipse (x - 4.8f, y - 3.6f, 9.6f, 7.2f);
                g.setColour (seedCol);    g.fillEllipse (x - 3.8f, y - 2.8f, 7.6f, 5.6f);
            }
            seeds -= n;
            top = y - 3.6f;
        }
        if (tonic) tonicTop = top;

        g.setFont (tk::font (14.f, true));
        g.setColour (tonic ? ink : (inScale[(size_t) pc] ? amber : grey));
        if (tonic)
        {
            g.setColour (mint);
            g.fillRect (cx - 17.f, base + 3.f, 34.f, 20.f);
            g.setColour (ink);
        }
        g.drawText (tk::noteName (pc), juce::Rectangle<int> ((int) (cx - 24.f), (int) base + 3, 48, 20), juce::Justification::centred, false);
    }

    // die pickende Chef-Taube über der Tonika
    if (shown.hasKey)
    {
        const float cx = x0 + ((float) shown.tonic + 0.5f) * colW;
        const float peck = std::max (0.f, (float) std::sin (now * 7.0)) ;
        tk::PigeonPose pose; pose.flying = false; pose.bob = peck * 2.0f; pose.crown = false;
        const float size = 30.f;
        tk::drawPigeon (g, { cx - 6.f, tonicTop - 0.5f * size - 2.f + peck * 3.f }, size, true, pose, juce::Colour (0xff9aa4bb));
    }
}

//==============================================================================
void TaubenKeyEditor::updateMascot()
{
    PigeonFlock::Mascot m;
    m.visible = true;
    m.size = 70.f;
    m.feet = { (float) keyArea.getRight() - 50.f, (float) keyArea.getBottom() - 14.f };
    m.faceRight = false;
    m.crown = shown.hasKey && shown.keyConfidence > 0.6f;
    m.bubbleMinX = (float) keyArea.getX() + 190.f;
    m.bubbleMaxX = (float) keyArea.getRight() - 6.f;

    if (! shown.hasKey)
        m.text = shown.signalPresent ? "Gurr... ich horche" : "Gurr...? Spiel was!";
    else
    {
        m.text = juce::String ("Gurr! ") + tk::noteName (shown.tonic) + (shown.minor ? " Moll" : " Dur");
        if (shown.hasTuning && shown.tuningQuality >= 0.35f)
            m.text += std::abs (shown.detuneCents) > 5.f ? " - schief!" : " - sauber";
    }
    flock.setMascot (m);
}

void TaubenKeyEditor::timerCallback()
{
    uint32_t v = 0;
    auto r = proc.getResult (&v);
    if (v != lastVersion)
    {
        const bool keyChanged = r.hasKey && (! shown.hasKey || r.tonic != shown.tonic || r.minor != shown.minor);
        shown = r;
        lastVersion = v;
        piano.setScale (shown.hasKey, shown.tonic, shown.minor);
        piano.setHeard (shown.heard);
        flock.setBpm (shown.hasBpm ? shown.bpm : 0.f);
        updateMascot();
        if (keyChanged) flock.scatter();
        repaint (keyArea);
        repaint (getLocalBounds().withHeight (80));
    }

    const float target = shown.hasTuning ? shown.detuneCents : 0.f;
    gaugeCents += (target - gaugeCents) * 0.2f;
    flock.setLevel (proc.getLevel() * 4.f);

    repaint (tuneArea);
    repaint (bpmArea);
    repaint (chromaArea);
}
