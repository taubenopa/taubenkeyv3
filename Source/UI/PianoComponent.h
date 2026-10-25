#pragma once
#include "PigeonArt.h"
#include "../Analysis/Analysis.h"

// Kleines Test-Klavier (C2..C6): klickbar, zeigt Tonleiter, Tonika, "gehörte" Noten und gedrückte Tasten.
class PianoComponent : public juce::Component, private juce::Timer
{
public:
    PianoComponent (juce::MidiKeyboardState& s, int lowest = 36, int highest = 84)
        : state (s), lo (lowest), hi (highest)
    {
        for (int n = lo; n <= hi; ++n) if (! isBlack (n)) ++numWhite;
        heardRaw.fill (0.f);
        heardSmooth.fill (0.f);
        startTimerHz (30);
    }
    ~PianoComponent() override { stopTimer(); }

    void setScale (bool valid, int tonic, bool minor)
    {
        static const int maj[7] = { 0, 2, 4, 5, 7, 9, 11 }, mnr[7] = { 0, 2, 3, 5, 7, 8, 10 };
        scaleMask.fill (false);
        hasScale = valid;
        tonicPc = tonic;
        if (valid)
            for (int i = 0; i < 7; ++i) scaleMask[(size_t) ((tonic + (minor ? mnr : maj)[i]) % 12)] = true;
        repaint();
    }
    void setHeard (const std::array<float, 128>& h) { heardRaw = h; }

    //--------------------------------------------------------------------------
    void paint (juce::Graphics& g) override
    {
        const float ww = whiteWidth(), h = (float) getHeight();

        for (int n = lo; n <= hi; ++n)
        {
            if (isBlack (n)) continue;
            auto r = whiteRect (n);
            drawKey (g, n, r, false);
            g.setColour (tk::pal::ink);
            g.drawRect (r, 1.2f);

            const int pc = n % 12;
            const bool inScale = hasScale && scaleMask[(size_t) pc];
            if (inScale || pc == 0)
            {
                g.setColour (pc == tonicPc && hasScale ? tk::pal::ink : (inScale ? tk::pal::ink.withAlpha (0.85f) : tk::pal::grey));
                g.setFont (tk::font (10.5f, true));
                juce::String label = tk::noteName (pc);
                if (pc == 0) label << (n / 12 - 1);
                g.drawText (label, r.withTrimmedBottom (4.f).toNearestInt(), juce::Justification::centredBottom, false);
            }
        }
        for (int n = lo; n <= hi; ++n)
            if (isBlack (n)) drawKey (g, n, blackRect (n), true);

        juce::ignoreUnused (ww, h);
    }

    //--------------------------------------------------------------------------
    void mouseDown (const juce::MouseEvent& e) override { press (noteAt (e.position)); }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        const int n = noteAt (e.position);
        if (n != current) press (n);
    }
    void mouseUp (const juce::MouseEvent&) override { press (-1); }

private:
    static bool isBlack (int n) { const int pc = n % 12; return pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10; }
    float whiteWidth() const { return (float) getWidth() / (float) numWhite; }
    int whiteIndex (int n) const { int c = 0; for (int i = lo; i < n; ++i) if (! isBlack (i)) ++c; return c; }

    juce::Rectangle<float> whiteRect (int n) const
    {
        const float ww = whiteWidth();
        return { (float) whiteIndex (n) * ww, 0.f, ww, (float) getHeight() };
    }
    juce::Rectangle<float> blackRect (int n) const
    {
        const float ww = whiteWidth(), bw = ww * 0.62f;
        const float x = (float) whiteIndex (n) * ww - bw * 0.5f;          // Grenze zur vorherigen weißen Taste
        return { x, 0.f, bw, (float) getHeight() * 0.62f };
    }

    int noteAt (juce::Point<float> p) const
    {
        for (int n = lo; n <= hi; ++n) if (isBlack (n) && blackRect (n).contains (p)) return n;
        for (int n = lo; n <= hi; ++n) if (! isBlack (n) && whiteRect (n).contains (p)) return n;
        return -1;
    }

    void press (int n)
    {
        if (n == current) return;
        if (current >= 0) state.noteOff (1, current, 0.f);
        current = n;
        if (current >= 0) state.noteOn (1, current, 0.8f);
    }

    void drawKey (juce::Graphics& g, int n, juce::Rectangle<float> r, bool black)
    {
        const int pc = n % 12;
        const bool down  = state.isNoteOnForChannels (0xffff, n);
        const bool inScl = hasScale && scaleMask[(size_t) pc];
        const bool tonic = hasScale && pc == tonicPc;
        const float heard = heardSmooth[(size_t) n];

        juce::Colour base = black ? tk::pal::ink.brighter (0.08f) : tk::pal::cream;
        if (inScl) base = black ? tk::pal::mint.darker (0.55f) : tk::pal::mint.interpolatedWith (tk::pal::cream, 0.55f);
        if (tonic) base = tk::pal::amber;
        if (down)  base = tk::pal::magenta;

        auto rr = r.reduced (black ? 0.f : 0.5f);
        g.setColour (base);
        g.fillRoundedRectangle (rr, 2.5f);

        if (heard > 0.01f)       // gehört: cyanfarbener Schein von oben
        {
            g.setGradientFill (juce::ColourGradient::vertical (tk::pal::cyan.withAlpha (0.25f + 0.65f * heard), rr.getY(),
                                                               tk::pal::cyan.withAlpha (0.f), rr.getY() + rr.getHeight() * 0.6f));
            g.fillRoundedRectangle (rr, 2.5f);
        }
        if (black)
        {
            g.setColour (tk::pal::cream.withAlpha (0.25f));
            g.drawRoundedRectangle (rr, 2.5f, 1.f);
        }
        if (tonic)               // kleines Ei als Tonika-Marker
        {
            const auto c = juce::Point<float> (rr.getCentreX(), black ? rr.getBottom() - 12.f : rr.getBottom() - 30.f);
            g.setColour (tk::pal::ink);
            g.fillEllipse (c.x - 4.5f, c.y - 6.f, 9.f, 12.f);
            g.setColour (tk::pal::cream);
            g.fillEllipse (c.x - 3.3f, c.y - 5.f, 6.6f, 9.4f);
        }
    }

    void timerCallback() override
    {
        bool changed = false;
        for (size_t i = 0; i < 128; ++i)
        {
            const float target = heardRaw[i];
            const float v = std::max (target, heardSmooth[i] * 0.86f);
            const float nv = v < 0.01f ? 0.f : v;
            if (std::abs (nv - heardSmooth[i]) > 0.002f) changed = true;
            heardSmooth[i] = nv;
        }
        for (int n = lo; n <= hi; ++n)
        {
            const bool d = state.isNoteOnForChannels (0xffff, n);
            if (d != lastDown[(size_t) n]) { lastDown[(size_t) n] = d; changed = true; }
        }
        if (changed) repaint();
    }

    juce::MidiKeyboardState& state;
    int lo, hi, numWhite = 0, current = -1;
    bool hasScale = false;
    int  tonicPc = 0;
    std::array<bool, 12> scaleMask {};
    std::array<float, 128> heardRaw {}, heardSmooth {};
    std::array<bool, 128> lastDown {};
};
