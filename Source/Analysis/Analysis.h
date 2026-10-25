// TaubenKey – Analyse-Engine (reines C++, keine JUCE-Abhängigkeit => testbar)
//  * Tonart:   hochaufgelöste Spektralpeaks (log-parabolische Interpolation) -> Chroma
//              -> Korrelation mit Krumhansl-Kessler + Temperley Profilen (24 Tonarten)
//  * Detune:   gewichtetes zirkuläres Mittel der Peak-Abweichungen vom 12-TET-Raster
//  * Tempo:    log-komprimierter Spectral-Flux -> Autokorrelation -> Kamm-Score (fein, 0.05 BPM)
#pragma once
#include <vector>
#include <array>
#include <complex>
#include <cmath>
#include <algorithm>
#include <cstdint>

namespace tk
{
constexpr double kPi = 3.14159265358979323846;

inline const char* noteName (int pc)
{
    static const char* n[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    return n[((pc % 12) + 12) % 12];
}
inline const char* noteNameFlat (int pc)
{
    static const char* n[12] = { "C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B" };
    return n[((pc % 12) + 12) % 12];
}

struct KeyCandidate { int tonic = 0; bool minor = false; float score = 0.f; };

struct AnalysisResult
{
    bool  signalPresent = false;
    float analysedSeconds = 0.f;

    bool  hasKey = false;
    int   tonic = 0;
    bool  minor = false;
    float keyConfidence = 0.f;
    bool  fromMidi = false;
    std::array<KeyCandidate, 3> top {};
    std::array<float, 12> chroma {};

    bool  hasTuning = false;
    float detuneCents = 0.f;
    float tuningQuality = 0.f;   // 0..1  (wie "tonal/eindeutig" das Material ist)
    float referenceHz = 440.f;

    bool  hasBpm = false;
    float bpm = 0.f;
    float bpmConfidence = 0.f;
    float bpmAlt = 0.f;

    std::array<float, 128> heard {};   // aktuell hörbare Grundtöne (0..1) pro MIDI-Note
};

// ------------------------------------------------------------------ FFT
class FFT
{
public:
    explicit FFT (int order) : n (1 << order), tw ((size_t) n / 2), rev ((size_t) n)
    {
        for (int i = 0; i < n / 2; ++i)
            tw[(size_t) i] = std::polar (1.0, -2.0 * kPi * i / n);
        for (int i = 0; i < n; ++i)
        {
            int r = 0;
            for (int b = 0; b < order; ++b)
                if (i & (1 << b)) r |= 1 << (order - 1 - b);
            rev[(size_t) i] = r;
        }
    }
    int size() const { return n; }

    void transform (std::vector<std::complex<double>>& a) const
    {
        for (int i = 0; i < n; ++i)
            if (i < rev[(size_t) i]) std::swap (a[(size_t) i], a[(size_t) rev[(size_t) i]]);
        for (int len = 2; len <= n; len <<= 1)
        {
            const int half = len / 2, step = n / len;
            for (int i = 0; i < n; i += len)
                for (int j = 0; j < half; ++j)
                {
                    const auto u = a[(size_t) (i + j)];
                    const auto v = a[(size_t) (i + j + half)] * tw[(size_t) (j * step)];
                    a[(size_t) (i + j)] = u + v;
                    a[(size_t) (i + j + half)] = u - v;
                }
        }
    }
private:
    int n;
    std::vector<std::complex<double>> tw;
    std::vector<int> rev;
};

// ------------------------------------------------------------------ Spektralpeaks
struct Peak { double freq; float w; };

class SpectralAnalyzer
{
public:
    explicit SpectralAnalyzer (int order)
        : N (1 << order), fft (order), win ((size_t) N), buf ((size_t) N), mag ((size_t) N / 2 + 8)
    {
        for (int i = 0; i < N; ++i) win[(size_t) i] = 0.5 * (1.0 - std::cos (2.0 * kPi * i / N));
    }
    int frameSize() const { return N; }

    // liefert false bei (fast) Stille
    bool framePeaks (const float* x, double sr, std::vector<Peak>& out)
    {
        out.clear();
        double e = 0.0;
        for (int i = 0; i < N; ++i)
        {
            const double v = x[i];
            e += v * v;
            buf[(size_t) i] = { v * win[(size_t) i], 0.0 };
        }
        if (e / N < 1e-9) return false;     // < ca. -90 dBFS RMS

        fft.transform (buf);

        const double binHz = sr / N;
        const int kMin = std::max (5, (int) std::ceil (60.0 / binHz));
        const int kMax = std::min (N / 2 - 5, (int) (5000.0 / binHz));
        for (int k = kMin - 4; k <= kMax + 4; ++k) mag[(size_t) k] = std::abs (buf[(size_t) k]);

        double mx = 0.0;
        for (int k = kMin; k <= kMax; ++k) mx = std::max (mx, mag[(size_t) k]);
        if (mx <= 0.0) return false;
        const double thr = mx * 0.0056;     // -45 dB

        for (int k = kMin; k <= kMax; ++k)
        {
            const double m = mag[(size_t) k];
            if (m < thr) continue;
            bool isMax = true;                 // lokales Maximum in +-3 Bins (unterdrückt Hann-Nebenmaxima)
            for (int j = -3; j <= 3 && isMax; ++j)
                if (j != 0 && mag[(size_t) (k + j)] > m) isMax = false;
            if (! isMax) continue;

            const double a = std::log (mag[(size_t) (k - 1)] + 1e-12), b = std::log (m), c = std::log (mag[(size_t) (k + 1)] + 1e-12);
            const double d = a - 2.0 * b + c;
            if (d >= 0.0) continue;
            const double p = 0.5 * (a - c) / d;
            if (std::abs (p) > 0.5) continue;
            out.push_back ({ (k + p) * binHz, (float) std::sqrt (m / mx) });
        }
        return ! out.empty();
    }
private:
    int N;
    FFT fft;
    std::vector<double> win;
    std::vector<std::complex<double>> buf;
    std::vector<double> mag;
};


// Ordnung k (2..8) falls Peak i ein Oberton eines tieferen, ausreichend starken Peaks ist, sonst 0.
// Erwartet nach Frequenz sortierte Peaks.
inline void findHarmonics (const std::vector<Peak>& pk, std::vector<int>& kOut)
{
    kOut.assign (pk.size(), 0);
    for (size_t i = 1; i < pk.size(); ++i)
        for (size_t j = 0; j < i; ++j)
        {
            if (pk[j].w < 0.6f * pk[i].w) continue;
            const int k = (int) std::lround (pk[i].freq / pk[j].freq);
            if (k < 2 || k > 8) continue;
            if (std::abs (1200.0 * std::log2 (pk[i].freq / (k * pk[j].freq))) < 30.0) { kOut[i] = k; break; }
        }
}

inline double pearson (const double* a, const double* b)
{
    double ma = 0, mb = 0;
    for (int i = 0; i < 12; ++i) { ma += a[i]; mb += b[i]; }
    ma /= 12; mb /= 12;
    double num = 0, da = 0, db = 0;
    for (int i = 0; i < 12; ++i)
    {
        num += (a[i] - ma) * (b[i] - mb);
        da += (a[i] - ma) * (a[i] - ma);
        db += (b[i] - mb) * (b[i] - mb);
    }
    return (da > 0 && db > 0) ? num / std::sqrt (da * db) : 0.0;
}

// ------------------------------------------------------------------ Tonart aus Chroma
inline void rankKeys (const std::array<float, 12>& chroma, AnalysisResult& r)
{
    static const double kkMaj[12] = { 6.35, 2.23, 3.48, 2.33, 4.38, 4.09, 2.52, 5.19, 2.39, 3.66, 2.29, 2.88 };
    static const double kkMin[12] = { 6.33, 2.68, 3.52, 5.38, 2.60, 3.53, 2.54, 4.75, 3.98, 2.69, 3.34, 3.17 };
    static const double tmMaj[12] = { 5.0, 2.0, 3.5, 2.0, 4.5, 4.0, 2.0, 4.5, 2.0, 3.5, 1.5, 4.0 };
    static const double tmMin[12] = { 5.0, 2.0, 3.5, 4.5, 2.0, 4.0, 2.0, 4.5, 3.5, 2.0, 1.5, 4.0 };

    double sum = 0.0;
    for (auto v : chroma) sum += v;
    r.chroma = chroma;
    if (sum <= 1e-9) { r.hasKey = false; return; }

    double c[12];
    for (int i = 0; i < 12; ++i) c[i] = chroma[(size_t) i] / sum;

    std::array<KeyCandidate, 24> all;
    int idx = 0;
    for (int t = 0; t < 12; ++t)
        for (int m = 0; m < 2; ++m)
        {
            double kk[12], tm[12];
            for (int pc = 0; pc < 12; ++pc)
            {
                const int d = (pc - t + 12) % 12;
                kk[pc] = m ? kkMin[d] : kkMaj[d];
                tm[pc] = m ? tmMin[d] : tmMaj[d];
            }
            all[(size_t) idx++] = { t, m == 1, (float) (0.5 * (pearson (c, kk) + pearson (c, tm))) };
        }
    std::sort (all.begin(), all.end(), [] (const KeyCandidate& a, const KeyCandidate& b) { return a.score > b.score; });

    for (size_t i = 0; i < 3; ++i) r.top[i] = all[i];
    const auto best = all[0];
    const int relTonic = best.minor ? (best.tonic + 3) % 12 : (best.tonic + 9) % 12;
    float secondNR = -1.f;
    for (size_t i = 1; i < all.size(); ++i)
        if (! (all[i].tonic == relTonic && all[i].minor != best.minor)) { secondNR = all[i].score; break; }

    r.hasKey = true;
    r.tonic = best.tonic;
    r.minor = best.minor;
    r.keyConfidence = std::clamp (0.9f * (best.score - 0.30f) + 2.2f * (best.score - secondNR), 0.f, 1.f);
}

// ------------------------------------------------------------------ Audio -> Tonart + Stimmung + hörbare Noten
inline void analyseKeyAudio (const float* x, int n, double sr, AnalysisResult& r)
{
    const int order = sr > 70000.0 ? 15 : 14;
    SpectralAnalyzer sa (order);
    const int N = sa.frameSize();
    if (n < N + N / 2) return;

    struct P { double midi; double freq; float w; };
    std::vector<P> all;
    std::vector<Peak> pk;
    std::vector<int> harm;
    int frames = 0;

    for (int s = 0; s + N <= n; s += N / 2)
        if (sa.framePeaks (x + s, sr, pk))
        {
            ++frames;
            std::sort (pk.begin(), pk.end(), [] (const Peak& a, const Peak& b) { return a.freq < b.freq; });
            findHarmonics (pk, harm);
            for (size_t i = 0; i < pk.size(); ++i)
            {
                // Oktav-Obertöne (2,4,8) fallen auf dieselbe Tonklasse -> behalten.
                // Quint-/Terz-/Septim-Obertöne (3,5,6,7) würden den Modus verfälschen -> stark dämpfen.
                const int k = harm[i];
                const float damp = (k == 0 || k == 2 || k == 4 || k == 8) ? 1.f : 0.15f;
                all.push_back ({ 69.0 + 12.0 * std::log2 (pk[i].freq / 440.0), pk[i].freq, pk[i].w * damp });
            }
        }
    if (frames == 0 || all.empty()) return;

    // ---- Stimmung: zirkuläres Mittel der Abweichung vom Halbtonraster
    double sc = 0, ss = 0, sw = 0;
    for (const auto& p : all)
    {
        if (p.w < 0.12f) continue;
        const double dev = p.midi - std::round (p.midi);
        sc += p.w * std::cos (2.0 * kPi * dev);
        ss += p.w * std::sin (2.0 * kPi * dev);
        sw += p.w;
    }
    double offset = 0.0;
    if (sw > 0.0)
    {
        offset = std::atan2 (ss, sc) / (2.0 * kPi);                // in Halbtönen, -0.5..0.5
        const double R = std::sqrt (sc * sc + ss * ss) / sw;       // 0..1
        r.hasTuning = true;
        r.detuneCents = (float) (offset * 100.0);
        r.tuningQuality = (float) std::clamp ((R - 0.30) / 0.60, 0.0, 1.0);
        r.referenceHz = (float) (440.0 * std::pow (2.0, offset / 12.0));
    }

    // ---- Chroma (Stimmungskompensiert) + Bass-Chroma
    std::array<float, 12> chroma {}, bass {};
    float sumC = 0, sumB = 0;
    for (const auto& p : all)
    {
        const int pc = (((int) std::lround (p.midi - offset)) % 12 + 12) % 12;
        chroma[(size_t) pc] += p.w; sumC += p.w;
        if (p.freq < 250.0) { bass[(size_t) pc] += p.w; sumB += p.w; }
    }
    std::array<float, 12> total {};
    for (size_t i = 0; i < 12; ++i)
        total[i] = chroma[i] / std::max (sumC, 1e-6f) + (sumB > 0 ? 0.4f * bass[i] / sumB : 0.f);
    rankKeys (total, r);

    // ---- Hörbare Noten (letzter Frame, Obertöne unterdrückt)
    if (sa.framePeaks (x + (n - N), sr, pk))
    {
        std::sort (pk.begin(), pk.end(), [] (const Peak& a, const Peak& b) { return a.freq < b.freq; });
        findHarmonics (pk, harm);
        for (size_t i = 0; i < pk.size(); ++i)
        {
            if (pk[i].w < 0.22f || harm[i] != 0) continue;
            const int note = (int) std::lround (69.0 + 12.0 * std::log2 (pk[i].freq / 440.0) - offset);
            if (note >= 0 && note < 128) r.heard[(size_t) note] = std::max (r.heard[(size_t) note], pk[i].w);
        }
    }
    r.analysedSeconds = (float) (n / sr);
}

// ------------------------------------------------------------------ Tempo
inline double interpAt (const std::vector<double>& v, double x)
{
    if (x < 0.0) return 0.0;
    const int i = (int) x;
    if (i + 1 >= (int) v.size()) return 0.0;
    const double f = x - i;
    return v[(size_t) i] * (1.0 - f) + v[(size_t) i + 1] * f;
}

inline void analyseTempo (const float* x, int n, double sr, AnalysisResult& r)
{
    constexpr int order = 10, N = 1 << order, hop = 256;
    if (n < (int) (4.0 * sr)) return;

    const int frames = (n - N) / hop + 1;
    const double fps = sr / hop;
    FFT fft (order);
    std::vector<double> win ((size_t) N), flux ((size_t) frames, 0.0), prev ((size_t) N / 2, 0.0);
    std::vector<std::complex<double>> buf ((size_t) N);
    for (int i = 0; i < N; ++i) win[(size_t) i] = 0.5 * (1.0 - std::cos (2.0 * kPi * i / N));

    const double binHz = sr / N;
    const int kLo = 2, kHi = std::min (N / 2 - 1, (int) (9000.0 / binHz));
    for (int f = 0; f < frames; ++f)
    {
        for (int i = 0; i < N; ++i) buf[(size_t) i] = { x[f * hop + i] * win[(size_t) i], 0.0 };
        fft.transform (buf);
        double fl = 0.0;
        for (int k = kLo; k <= kHi; ++k)
        {
            const double v = std::log1p (std::abs (buf[(size_t) k]) * (1000.0 / N));
            if (f > 0) fl += std::max (0.0, v - prev[(size_t) k]);
            prev[(size_t) k] = v;
        }
        flux[(size_t) f] = fl;
    }

    // Onset-Hüllkurve: lokalen Mittelwert abziehen
    std::vector<double> pre ((size_t) frames + 1, 0.0), env ((size_t) frames, 0.0);
    for (int i = 0; i < frames; ++i) pre[(size_t) i + 1] = pre[(size_t) i] + flux[(size_t) i];
    const int h = std::max (2, (int) std::lround (0.2 * fps));
    double gm = 0.0;
    for (int i = 0; i < frames; ++i)
    {
        const int a = std::max (0, i - h), b = std::min (frames, i + h + 1);
        const double mean = (pre[(size_t) b] - pre[(size_t) a]) / (b - a);
        env[(size_t) i] = std::max (0.0, flux[(size_t) i] - mean);
        gm += env[(size_t) i];
    }
    gm /= frames;
    double energy = 0.0;
    for (auto& v : env) { v -= gm; energy += v * v; }
    if (energy < 1e-9) return;

    // Autokorrelation
    const int maxLag = std::min (frames / 2, (int) std::ceil (4.0 * fps * 60.0 / 55.0) + 2);
    std::vector<double> ac ((size_t) maxLag);
    for (int l = 0; l < maxLag; ++l)
    {
        double s = 0.0;
        for (int i = 0; i + l < frames; ++i) s += env[(size_t) i] * env[(size_t) (i + l)];
        ac[(size_t) l] = s / (frames - l);
    }
    const double a0 = ac[0];
    if (a0 <= 0.0) return;
    for (auto& v : ac) v /= a0;

    auto rawScore = [&] (double bpm)
    {
        const double lag = fps * 60.0 / bpm;
        static const double wk[4] = { 1.0, 0.7, 0.5, 0.4 };
        double s = 0.0, ws = 0.0;
        for (int k = 1; k <= 4; ++k)
        {
            const double pos = k * lag;
            if (pos + 1 >= (double) ac.size()) break;
            s += wk[k - 1] * interpAt (ac, pos);
            ws += wk[k - 1];
        }
        return ws > 0.0 ? s / ws : 0.0;
    };
    auto prior = [] (double bpm) { const double d = std::log2 (bpm / 118.0) / 0.6; return 0.6 + 0.4 * std::exp (-0.5 * d * d); };

    double best = -1e9, bestBpm = 0.0;
    for (double b = 55.0; b <= 215.0; b += 0.05)
    {
        const double s = rawScore (b) * prior (b);
        if (s > best) { best = s; bestBpm = b; }
    }
    if (bestBpm <= 0.0) return;

    // Feinabgleich ohne Prior
    double fine = -1e9, fineBpm = bestBpm;
    for (double b = bestBpm * 0.985; b <= bestBpm * 1.015; b += 0.01)
    {
        const double s = rawScore (b);
        if (s > fine) { fine = s; fineBpm = b; }
    }
    // auf ganze BPM einrasten, wenn praktisch gleichwertig
    const double rounded = std::round (fineBpm);
    if (std::abs (rounded - fineBpm) < 0.2 && rawScore (rounded) > 0.95 * fine) fineBpm = rounded;

    r.hasBpm = true;
    r.bpm = (float) fineBpm;
    r.bpmConfidence = (float) std::clamp (interpAt (ac, fps * 60.0 / fineBpm) * 2.5, 0.0, 1.0);
    r.bpmAlt = (float) (fineBpm >= 110.0 ? fineBpm / 2.0 : fineBpm * 2.0);
}

} // namespace tk
