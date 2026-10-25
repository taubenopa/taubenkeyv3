#pragma once
#include <JuceHeader.h>

// "Nachtstadt-Beton trifft Taubenhals-Schillern"
namespace tk::pal
{
    inline const juce::Colour ink      { 0xff0a0c15 };
    inline const juce::Colour night    { 0xff12162a };
    inline const juce::Colour concrete { 0xff262b42 };
    inline const juce::Colour cream    { 0xfff4ead5 };
    inline const juce::Colour amber    { 0xffffc247 };
    inline const juce::Colour mint     { 0xff37e6a0 };
    inline const juce::Colour magenta  { 0xffff3d9a };
    inline const juce::Colour violet   { 0xffa066ff };
    inline const juce::Colour cyan     { 0xff4fd8ff };
    inline const juce::Colour grey     { 0xff8f98ad };
}

namespace tk
{
inline juce::Font font (float h, bool bold = false)
{
    return juce::Font (juce::FontOptions (h, bold ? juce::Font::bold : juce::Font::plain));
}

inline float textWidth (const juce::Font& f, const juce::String& s)
{
    juce::GlyphArrangement ga;
    ga.addLineOfText (f, s, 0.f, 0.f);
    return ga.getBoundingBox (0, -1, true).getWidth();
}

struct PigeonPose
{
    bool  flying = true;
    float flap   = 0.f;    // Flügelphase 0..1
    float bob    = 0.f;    // Kopfnicken -1..1 (nur sitzend)
    bool  crown  = false;
};

// pos = Körpermitte. Sitzend reichen die Beine 0.5*size unter die Körpermitte.
inline void drawPigeon (juce::Graphics& g, juce::Point<float> pos, float size, bool faceRight,
                        const PigeonPose& pose, juce::Colour body = juce::Colour (0xff8d97ad))
{
    juce::Graphics::ScopedSaveState ss (g);
    g.addTransform (juce::AffineTransform::scale (faceRight ? size : -size, size).translated (pos.x, pos.y));

    const auto dark  = body.darker (0.45f);
    const auto light = body.brighter (0.25f);
    const float flapS = std::sin (pose.flap * juce::MathConstants<float>::twoPi);   // -1..1
    const float bobX  = pose.flying ? 0.f : pose.bob * 0.05f;
    const float bobY  = pose.flying ? 0.f : std::abs (pose.bob) * 0.015f;

    auto wing = [&] (juce::Colour c, float dx)
    {
        juce::Path w;
        w.startNewSubPath (0.f, 0.f);
        w.cubicTo (0.14f, -0.25f, 0.10f, -0.55f, -0.10f, -0.80f);
        w.cubicTo (-0.22f, -0.52f, -0.30f, -0.25f, -0.26f, 0.f);
        w.closeSubPath();
        const float angle = -juce::jmap (flapS, -1.f, 1.f, 2.3f, 0.1f);       // flapS = 1 -> Flügel oben
        g.setColour (c);
        g.fillPath (w, juce::AffineTransform::rotation (angle).translated (dx, -0.14f));
    };

    if (pose.flying) wing (dark, -0.04f);                                        // ferner Flügel

    if (! pose.flying)                                                           // Beine
    {
        g.setColour (juce::Colour (0xffe0644a));
        g.drawLine (-0.02f, 0.18f, -0.04f, 0.50f, 0.045f);
        g.drawLine ( 0.14f, 0.18f,  0.16f, 0.50f, 0.045f);
        g.drawLine (-0.04f, 0.50f,  0.07f, 0.50f, 0.04f);
        g.drawLine ( 0.16f, 0.50f,  0.27f, 0.50f, 0.04f);
    }

    juce::Path tail;                                                             // Schwanz
    tail.startNewSubPath (-0.34f, -0.08f);
    tail.lineTo (-0.84f, -0.04f - (pose.flying ? flapS * 0.03f : 0.f));
    tail.lineTo (-0.80f,  0.13f);
    tail.lineTo (-0.34f,  0.12f);
    tail.closeSubPath();
    g.setColour (dark);
    g.fillPath (tail);

    g.setColour (body);                                                          // Körper
    g.fillEllipse (-0.46f, -0.22f, 0.92f, 0.46f);
    g.setColour (light.withAlpha (0.45f));
    g.fillEllipse (-0.18f, 0.00f, 0.62f, 0.20f);

    juce::ColourGradient neck (juce::Colour (0xff35e0a1), 0.15f, -0.32f, juce::Colour (0xffb14dff), 0.55f, 0.02f, false);
    g.setGradientFill (neck);                                                    // Schillerhals
    g.fillEllipse (0.10f + bobX, -0.31f + bobY, 0.42f, 0.38f);

    g.setColour (body.brighter (0.08f));                                         // Kopf
    g.fillEllipse (0.34f + bobX, -0.41f + bobY, 0.27f, 0.26f);

    g.setColour (juce::Colour (0xfff0e0c8));                                     // Schnabel
    juce::Path beak;
    beak.addTriangle (0.58f + bobX, -0.32f + bobY, 0.73f + bobX, -0.26f + bobY, 0.58f + bobX, -0.22f + bobY);
    g.fillPath (beak);
    g.setColour (juce::Colours::white.withAlpha (0.9f));
    g.fillEllipse (0.56f + bobX, -0.335f + bobY, 0.055f, 0.05f);

    g.setColour (juce::Colour (0xffff8a00));                                     // Auge
    g.fillEllipse (0.465f + bobX, -0.355f + bobY, 0.075f, 0.075f);
    g.setColour (juce::Colours::black);
    g.fillEllipse (0.485f + bobX, -0.337f + bobY, 0.038f, 0.038f);

    if (pose.crown)                                                              // Krönchen
    {
        juce::Path c;
        c.startNewSubPath (0.40f + bobX, -0.40f + bobY);
        c.lineTo (0.39f + bobX, -0.54f + bobY);
        c.lineTo (0.44f + bobX, -0.47f + bobY);
        c.lineTo (0.47f + bobX, -0.57f + bobY);
        c.lineTo (0.50f + bobX, -0.47f + bobY);
        c.lineTo (0.55f + bobX, -0.54f + bobY);
        c.lineTo (0.54f + bobX, -0.40f + bobY);
        c.closeSubPath();
        g.setColour (pal::amber);
        g.fillPath (c);
    }

    if (pose.flying)
        wing (body.brighter (0.05f), 0.0f);                                      // naher Flügel
    else
    {
        g.setColour (dark);                                                      // angelegter Flügel
        g.fillEllipse (-0.40f, -0.14f, 0.66f, 0.30f);
        g.setColour (juce::Colours::black.withAlpha (0.55f));                    // Flügelbinde
        g.drawLine (-0.28f, -0.08f, -0.02f, -0.12f, 0.035f);
        g.drawLine (-0.30f,  0.02f, -0.04f, -0.02f, 0.035f);
    }
}
} // namespace tk
