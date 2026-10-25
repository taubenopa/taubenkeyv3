#pragma once
#include "PigeonArt.h"

// Durchsichtiges Overlay: Tauben fliegen, landen auf Panel-Kanten, picken, flattern bei neuer Tonart auf.
// Flügelschlag-Tempo folgt dem erkannten BPM, Fluggeschwindigkeit dem Pegel.
class PigeonFlock : public juce::Component, private juce::Timer
{
public:
    struct Mascot
    {
        juce::Point<float> feet;
        float size = 90.f;
        bool  crown = false;
        bool  faceRight = false;
        juce::String text;
        float bubbleMinX = 0.f, bubbleMaxX = 0.f;
        bool  visible = false;
    };

    PigeonFlock()
    {
        setInterceptsMouseClicks (false, false);
        setOpaque (false);
        auto& rnd = juce::Random::getSystemRandom();
        for (int i = 0; i < 7; ++i)
        {
            Bird b;
            b.pos  = { 80.f + rnd.nextFloat() * 700.f, 120.f + rnd.nextFloat() * 300.f };
            b.vel  = { (rnd.nextFloat() - 0.5f) * 120.f, (rnd.nextFloat() - 0.5f) * 60.f };
            b.size = 26.f + rnd.nextFloat() * 12.f;
            b.phase = rnd.nextFloat();
            b.target = b.pos;
            b.timer = rnd.nextFloat() * 3.f;
            const float t = rnd.nextFloat();
            b.col = juce::Colour (0xff8d97ad).interpolatedWith (juce::Colour (0xffc9b8a8), t * 0.6f)
                                               .withMultipliedBrightness (0.85f + 0.3f * rnd.nextFloat());
            birds.push_back (b);
        }
        startTimerHz (60);
    }
    ~PigeonFlock() override { stopTimer(); }

    void setPerches (std::vector<juce::Point<float>> p)
    {
        perches = std::move (p);
        occupant.assign (perches.size(), -1);
        for (auto& b : birds)
            if (b.state != State::Flying) { b.state = State::Flying; b.perch = -1; b.timer = 0.f; }
    }
    void setBpm (float bpm)
    {
        beatHz = bpm > 0.f ? bpm / 60.f : 1.8f;
        flapHz = bpm > 0.f ? juce::jlimit (2.2f, 6.5f, bpm / 60.f * 2.f) : 3.4f;
    }
    void setLevel (float l) { level = juce::jlimit (0.f, 1.f, l); }
    void setMascot (const Mascot& m) { mascot = m; }

    void scatter()
    {
        auto& rnd = juce::Random::getSystemRandom();
        for (size_t i = 0; i < birds.size(); ++i)
        {
            auto& b = birds[i];
            if (b.perch >= 0 && b.perch < (int) occupant.size()) occupant[(size_t) b.perch] = -1;
            b.perch = -1; b.state = State::Flying; b.timer = 0.4f + rnd.nextFloat();
            b.vel = { (rnd.nextFloat() - 0.5f) * 260.f, -160.f - rnd.nextFloat() * 120.f };
            b.target = pickTarget();
        }
    }

    void paint (juce::Graphics& g) override
    {
        for (const auto& b : birds)
        {
            const bool perched = b.state == State::Perched;
            tk::PigeonPose pose;
            pose.flying = ! perched;
            pose.flap = b.phase;
            pose.bob = b.bob;
            if (! perched)
            {   // weicher Schatten für Tiefe
                g.setColour (juce::Colours::black.withAlpha (0.18f));
                g.fillEllipse (b.pos.x - b.size * 0.35f, b.pos.y + b.size * 0.9f, b.size * 0.7f, b.size * 0.12f);
            }
            tk::drawPigeon (g, b.pos, b.size, b.faceRight, pose, b.col);
        }

        if (mascot.visible)
        {
            tk::PigeonPose pose;
            pose.flying = false;
            pose.bob = std::sin (mascotPhase * juce::MathConstants<float>::twoPi);
            pose.crown = mascot.crown;
            tk::drawPigeon (g, { mascot.feet.x, mascot.feet.y - 0.5f * mascot.size }, mascot.size, mascot.faceRight, pose,
                            juce::Colour (0xff9aa4bb));

            if (mascot.text.isNotEmpty())
            {
                const auto f = tk::font (13.f, true);
                const float w = tk::textWidth (f, mascot.text) + 22.f, h = 26.f;
                const float headX = mascot.feet.x + (mascot.faceRight ? 0.45f : -0.45f) * mascot.size;
                float x = headX - w * 0.5f;
                x = juce::jlimit (mascot.bubbleMinX, std::max (mascot.bubbleMinX, mascot.bubbleMaxX - w), x);
                const float y = mascot.feet.y - 1.18f * mascot.size - h;
                juce::Rectangle<float> r (x, y, w, h);
                g.setColour (tk::pal::ink);
                g.fillRoundedRectangle (r.translated (3.f, 3.f), 9.f);
                g.setColour (tk::pal::cream);
                g.fillRoundedRectangle (r, 9.f);
                juce::Path tail;
                const float tx = juce::jlimit (r.getX() + 12.f, r.getRight() - 12.f, headX);
                tail.addTriangle (tx - 6.f, r.getBottom() - 1.f, tx + 6.f, r.getBottom() - 1.f, headX, r.getBottom() + 9.f);
                g.fillPath (tail);
                g.setColour (tk::pal::ink);
                g.setFont (f);
                g.drawText (mascot.text, r.toNearestInt(), juce::Justification::centred, false);
            }
        }
    }

private:
    enum class State { Flying, Landing, Perched };
    struct Bird
    {
        juce::Point<float> pos, vel, target;
        float size = 30.f, phase = 0.f, timer = 0.f, bob = 0.f;
        int   perch = -1;
        bool  faceRight = true;
        State state = State::Flying;
        juce::Colour col;
    };

    juce::Point<float> pickTarget() const
    {
        auto& rnd = juce::Random::getSystemRandom();
        const float w = (float) std::max (getWidth(), 400), h = (float) std::max (getHeight(), 300);
        return { 40.f + rnd.nextFloat() * (w - 80.f), 40.f + rnd.nextFloat() * (h - 120.f) };
    }
    juce::Rectangle<int> box (const Bird& b) const
    {
        return juce::Rectangle<float> (b.pos.x - b.size * 1.0f, b.pos.y - b.size * 1.0f, b.size * 2.0f, b.size * 1.9f).getSmallestIntegerContainer();
    }

    void timerCallback() override
    {
        const float dt = 1.f / 60.f;
        auto& rnd = juce::Random::getSystemRandom();

        mascotPhase += beatHz * 0.5f * dt;
        if (mascotPhase > 1.f) mascotPhase -= 1.f;
        if (mascot.visible)
        {
            const float s = mascot.size;
            repaint (juce::Rectangle<float> (mascot.feet.x - s * 1.3f, mascot.feet.y - 1.5f * s - 14.f, s * 2.6f, s * 1.5f + 28.f).getSmallestIntegerContainer());
        }

        for (size_t i = 0; i < birds.size(); ++i)
        {
            auto& b = birds[i];
            const auto before = box (b);

            switch (b.state)
            {
                case State::Flying:
                {
                    b.timer -= dt;
                    if (b.timer <= 0.f || b.pos.getDistanceFrom (b.target) < 40.f)
                    {
                        b.target = pickTarget();
                        b.timer = 2.f + rnd.nextFloat() * 3.f;
                        if (! perches.empty() && rnd.nextFloat() < 0.4f)
                        {
                            const int idx = rnd.nextInt ((int) perches.size());
                            if (occupant[(size_t) idx] < 0)
                            {
                                occupant[(size_t) idx] = (int) i;
                                b.perch = idx;
                                b.state = State::Landing;
                            }
                        }
                    }
                    if (b.state == State::Flying)
                    {
                        auto desired = b.target - b.pos;
                        const float d = std::max (1.f, desired.getDistanceFromOrigin());
                        desired = desired * ((105.f + 70.f * level) / d);
                        juce::Point<float> sep;
                        for (size_t j = 0; j < birds.size(); ++j)
                            if (j != i)
                            {
                                auto diff = b.pos - birds[j].pos;
                                const float dd = diff.getDistanceFromOrigin();
                                if (dd < 70.f && dd > 0.01f) sep += diff * ((70.f - dd) / dd);
                            }
                        b.vel += (desired - b.vel) * std::min (1.f, dt * 1.6f) + sep * (dt * 2.f);
                        b.pos += b.vel * dt;
                        b.phase += flapHz * dt * (0.9f + 0.2f * (float) i / (float) birds.size());
                    }
                    break;
                }
                case State::Landing:
                {
                    const auto tgt = perches[(size_t) b.perch] - juce::Point<float> (0.f, 0.5f * b.size);
                    auto desired = tgt - b.pos;
                    const float d = desired.getDistanceFromOrigin();
                    if (d < 3.f)
                    {
                        b.state = State::Perched; b.pos = tgt; b.vel = {};
                        b.timer = 3.f + rnd.nextFloat() * 6.f; b.bob = 0.f;
                    }
                    else
                    {
                        desired = desired * (juce::jlimit (30.f, 150.f, d * 2.2f) / d);
                        b.vel += (desired - b.vel) * std::min (1.f, dt * 3.f);
                        b.pos += b.vel * dt;
                        b.phase += flapHz * dt * 1.3f;
                    }
                    break;
                }
                case State::Perched:
                {
                    b.bob = (float) std::sin (juce::Time::getMillisecondCounterHiRes() * 0.001 * (double) beatHz * juce::MathConstants<double>::pi + (double) i);
                    b.timer -= dt;
                    if (b.timer <= 0.f)
                    {
                        if (b.perch >= 0) occupant[(size_t) b.perch] = -1;
                        b.perch = -1; b.state = State::Flying; b.timer = 0.5f;
                        b.vel = { (rnd.nextFloat() - 0.5f) * 140.f, -150.f };
                        b.target = pickTarget();
                    }
                    break;
                }
            }
            if (b.state != State::Perched && std::abs (b.vel.x) > 8.f) b.faceRight = b.vel.x > 0.f;
            if (b.phase > 1.f) b.phase -= 1.f;

            repaint (before.getUnion (box (b)));
        }
    }

    std::vector<Bird> birds;
    std::vector<juce::Point<float>> perches;
    std::vector<int> occupant;
    Mascot mascot;
    float mascotPhase = 0.f, beatHz = 1.8f, flapHz = 3.4f, level = 0.f;
};
