// Build: g++ -O2 -std=c++17 tests/analysis_test.cpp -o analysis_test && ./analysis_test
#include "../Source/Analysis/Analysis.h"
#include <cstdio>
#include <random>
#include <cstdlib>

using namespace tk;
static const double SR = 44100.0;

static void addNote (std::vector<float>& y, double start, double dur, double midi, double centsOff, float amp)
{
    const double f = 440.0 * std::pow (2.0, (midi - 69.0 + centsOff / 100.0) / 12.0);
    const int s0 = (int) (start * SR), n = (int) (dur * SR);
    for (int i = 0; i < n && s0 + i < (int) y.size(); ++i)
    {
        const double t = i / SR, env = std::min (1.0, t * 80.0) * std::exp (-t * 2.5);
        double v = 0;
        for (int h = 1; h <= 6; ++h) v += std::sin (2 * kPi * f * h * t) / h;
        y[(size_t) (s0 + i)] += (float) (amp * env * v);
    }
}
static void addKick (std::vector<float>& y, double start)
{
    const int s0 = (int) (start * SR);
    for (int i = 0; i < (int) (0.12 * SR) && s0 + i < (int) y.size(); ++i)
    {
        const double t = i / SR;
        y[(size_t) (s0 + i)] += (float) (0.6 * std::exp (-t * 35.0) * std::sin (2 * kPi * (50.0 + 90.0 * std::exp (-t * 40)) * t));
    }
}
static void addHat (std::vector<float>& y, double start, std::mt19937& rng)
{
    std::normal_distribution<float> nd (0.f, 0.15f);
    const int s0 = (int) (start * SR);
    for (int i = 0; i < (int) (0.04 * SR) && s0 + i < (int) y.size(); ++i)
        y[(size_t) (s0 + i)] += nd (rng) * (float) std::exp (-i / SR * 90.0);
}

static void report (const char* name, const std::vector<float>& y)
{
    AnalysisResult r;
    analyseKeyAudio (y.data(), (int) y.size(), SR, r);
    if (std::getenv("DBG")) { double s=0; for(auto v:r.chroma) s+=v; std::printf("  chroma:"); for(int i=0;i<12;++i) std::printf(" %s=%.2f", noteName(i), r.chroma[i]/s); std::printf("\n"); }
    analyseTempo (y.data(), (int) y.size(), SR, r);
    std::printf ("%-34s key=%s %s conf=%.2f | detune=%+6.1f ct q=%.2f | bpm=%.2f conf=%.2f alt=%.1f | top2=%s %s(%.2f)\n",
                 name, noteName (r.tonic), r.minor ? "min" : "maj", r.keyConfidence,
                 r.detuneCents, r.tuningQuality, r.bpm, r.bpmConfidence, r.bpmAlt,
                 noteName (r.top[1].tonic), r.top[1].minor ? "min" : "maj", r.top[1].score);
}

int main()
{
    std::mt19937 rng (1);
    const int len = (int) (12 * SR);

    struct Case { const char* name; int tonic; bool minor; double cents; double bpm; };
    const Case cases[] = {
        { "C major, in tune, 128 bpm",    0, false,   0.0, 128.0 },
        { "C major, +23 ct, 128 bpm",     0, false,  23.0, 128.0 },
        { "F# minor, -31 ct, 93 bpm",     6, true,  -31.0,  93.0 },
        { "D minor, +8 ct, 140 bpm",      2, true,    8.0, 140.0 },
        { "Eb major, -12 ct, 87.5 bpm",   3, false, -12.0,  87.5 },
        { "A minor, 0 ct, 174 bpm",       9, true,    0.0, 174.0 },
    };
    static const int maj[7] = { 0, 2, 4, 5, 7, 9, 11 };
    static const int nmin[7] = { 0, 2, 3, 5, 7, 8, 10 };

    for (const auto& c : cases)
    {
        std::vector<float> y ((size_t) len, 0.f);
        const double beat = 60.0 / c.bpm;
        std::uniform_int_distribution<int> deg (0, 6);
        // Melodie: Achtel, mit Tonika-Betonung, zwei Oktaven
        int step = 0;
        for (double t = 0; t < 11.5; t += beat / 2, ++step)
        {
            static const int motif[16] = { 0, 1, 2, 4, 2, 3, 4, 6, 5, 4, 2, 0, 2, 4, 5, 3 };
            int d = (step % 5 == 0) ? deg (rng) : motif[step % 16];
            const int semi = (c.minor ? nmin : maj)[d];
            const int oct = (step % 3 == 0) ? 60 : 72;
            addNote (y, t, beat * 0.45, oct + c.tonic + semi - ((c.tonic > 6) ? 12 : 0), c.cents, 0.18f);
        }
        // Bass: Tonika auf Taktanfang
        for (double t = 0; t < 11.5; t += beat * 4) addNote (y, t, beat * 3.5, 36 + c.tonic, c.cents, 0.25f);
        for (double t = 0; t < 11.8; t += beat) addKick (y, t);
        for (double t = beat / 2; t < 11.8; t += beat) addHat (y, t, rng);
        report (c.name, y);
    }

    // nur Melodie, ohne Drums, ohne Detune
    {
        std::vector<float> y ((size_t) len, 0.f);
        const int seq[] = { 0, 2, 4, 2, 5, 4, 2, 0, 7, 5, 4, 2, 0, 4, 7, 0 };
        for (int i = 0; i < 24; ++i) addNote (y, i * 0.45, 0.4, 67 + seq[i % 16] + 2, 0.0, 0.3f);   // A-Dur ab A4
        report ("Melody only A major (no drums)", y);
    }
    // Rauschen -> Qualität sollte niedrig sein
    {
        std::vector<float> y ((size_t) len);
        std::normal_distribution<float> nd (0.f, 0.1f);
        for (auto& v : y) v = nd (rng);
        report ("white noise", y);
    }
}
