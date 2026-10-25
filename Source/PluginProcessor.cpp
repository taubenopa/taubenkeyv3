#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    constexpr double kRingSeconds = 28.0;   // Fenster max. 20 s + Sicherheitsabstand
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout TaubenKeyProcessor::createLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;
    p.push_back (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "window", 1 }, "Gedaechtnis",
                                                               juce::StringArray { "6 s", "12 s", "20 s" }, 1));
    p.push_back (std::make_unique<juce::AudioParameterBool>   (juce::ParameterID { "freeze", 1 }, "Einfrieren", false));
    p.push_back (std::make_unique<juce::AudioParameterBool>   (juce::ParameterID { "testSound", 1 }, "Gurr-Ton", true));
    p.push_back (std::make_unique<juce::AudioParameterFloat>  (juce::ParameterID { "testDetune", 1 }, "Test-Verstimmung",
                                                               juce::NormalisableRange<float> (-100.f, 100.f, 0.1f), 0.f));
    return { p.begin(), p.end() };
}

TaubenKeyProcessor::TaubenKeyProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "TAUBENKEY", createLayout()),
      worker (*this)
{
    for (auto& a : midiChroma) a.store (0.f);
    pWindow = apvts.getRawParameterValue ("window");
    pFreeze = apvts.getRawParameterValue ("freeze");
    pTest   = apvts.getRawParameterValue ("testSound");
    pDetune = apvts.getRawParameterValue ("testDetune");
    worker.startThread (juce::Thread::Priority::low);
}

TaubenKeyProcessor::~TaubenKeyProcessor()
{
    worker.stopThread (3000);
}

bool TaubenKeyProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto& out = l.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo()) return false;
    return out == l.getMainInputChannelSet();
}

void TaubenKeyProcessor::prepareToPlay (double sr, int)
{
    {
        const juce::ScopedLock sl (ringLock);
        sampleRate = sr;
        ring.assign ((size_t) (sr * kRingSeconds), 0.f);
        ringCap = (int64_t) ring.size();
        totalWritten.store (0);
        validFrom.store (0);
    }
    for (auto& v : voices) v = Voice {};
}

//==============================================================================
void TaubenKeyProcessor::noteOn (int note, float vel)
{
    for (auto& v : voices)
        if (v.note == note) { v.env = vel; v.vel = vel; v.att = 0.f; v.held = true; return; }

    Voice* slot = nullptr;
    for (auto& v : voices) if (v.note < 0) { slot = &v; break; }
    if (slot == nullptr)
    {
        slot = &voices[0];
        for (auto& v : voices) if (v.env < slot->env) slot = &v;
    }
    *slot = Voice { note, 0.0, vel, vel, 0.f, true };
}

void TaubenKeyProcessor::noteOff (int note)
{
    for (auto& v : voices) if (v.note == note) v.held = false;
}

void TaubenKeyProcessor::renderVoices (juce::AudioBuffer<float>& buffer, int n, float detuneCents)
{
    const double sr = sampleRate;
    const float relCoef  = std::exp (-1.f / (0.12f * (float) sr));
    const float holdCoef = std::exp (-1.f / (1.6f  * (float) sr));
    const float attInc   = 1.f / (0.004f * (float) sr);
    const int numCh = buffer.getNumChannels();

    for (auto& v : voices)
    {
        if (v.note < 0) continue;
        const double f   = 440.0 * std::pow (2.0, ((double) v.note - 69.0 + (double) detuneCents / 100.0) / 12.0);
        const double inc = juce::MathConstants<double>::twoPi * f / sr;

        for (int i = 0; i < n; ++i)
        {
            v.att = std::min (1.f, v.att + attInc);
            v.env *= v.held ? holdCoef : relCoef;
            const float s = (float) (std::sin (v.phase) + 0.35 * std::sin (2.0 * v.phase) + 0.15 * std::sin (3.0 * v.phase));
            const float out = s * v.env * v.att * 0.22f;
            for (int ch = 0; ch < numCh; ++ch) buffer.addSample (ch, i, out);
            v.phase += inc;
            if (v.phase > juce::MathConstants<double>::twoPi) v.phase -= juce::MathConstants<double>::twoPi;
        }
        if (! v.held && v.env < 1e-4f) v.note = -1;
    }
}

//==============================================================================
void TaubenKeyProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int numCh = buffer.getNumChannels();
    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, n);

    // Klicks auf dem Plugin-Klavier mit Host-MIDI zusammenführen
    keyboardState.processNextMidiBuffer (midi, 0, n, true);

    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto b = pos->getBpm())
                hostBpm.store ((float) *b);

    // ---- MIDI -> Noten-Histogramm (dauergewichtet, Halbwertszeit 25 s)
    const bool testOn = pTest->load() > 0.5f;
    std::array<float, 12> add {};
    for (const auto meta : midi)
    {
        const auto m = meta.getMessage();
        if (m.isNoteOn())
        {
            const int note = m.getNoteNumber();
            held[(size_t) note] = true;
            add[(size_t) (note % 12)] += 0.10f * m.getFloatVelocity();
            if (testOn) noteOn (note, 0.5f + 0.5f * m.getFloatVelocity());
        }
        else if (m.isNoteOff())
        {
            held[(size_t) m.getNoteNumber()] = false;
            noteOff (m.getNoteNumber());
        }
        else if (m.isAllNotesOff() || m.isAllSoundOff())
        {
            held.fill (false);
            for (auto& v : voices) v.held = false;
        }
    }
    const float dt = (float) ((double) n / sampleRate);
    for (int note = 0; note < 128; ++note)
        if (held[(size_t) note]) add[(size_t) (note % 12)] += dt;
    const float decay = std::exp (-0.6931f * dt / 25.f);
    for (size_t pc = 0; pc < 12; ++pc)
        midiChroma[pc].store (midiChroma[pc].load() * decay + add[pc]);

    // ---- Gurr-Ton (Test-Synth, geht mit in die Analyse)
    renderVoices (buffer, n, pDetune->load());

    // ---- Pegel für die Tauben
    level.store (std::max (buffer.getMagnitude (0, n), level.load() * 0.92f));

    // ---- Mono-Tap in den Ringpuffer
    if (ringCap > 0 && numCh > 0)
    {
        int64_t w = totalWritten.load (std::memory_order_relaxed);
        const float inv = 1.f / (float) numCh;
        for (int i = 0; i < n; ++i)
        {
            float s = 0.f;
            for (int ch = 0; ch < numCh; ++ch) s += buffer.getReadPointer (ch)[i];
            ring[(size_t) ((w + i) % ringCap)] = s * inv;
        }
        totalWritten.store (w + n, std::memory_order_release);
    }
}

//==============================================================================
void TaubenKeyProcessor::requestReset()
{
    for (auto& a : midiChroma) a.store (0.f);
    resetFlag.store (true);
}

tk::AnalysisResult TaubenKeyProcessor::getResult (uint32_t* version) const
{
    const juce::ScopedLock sl (resultLock);
    if (version != nullptr) *version = resultVersion;
    return result;
}

void TaubenKeyProcessor::stabilise (tk::AnalysisResult& r)
{
    // ---- Tonart: Hysterese, damit die Anzeige nicht flackert
    if (r.hasKey)
    {
        if (stableTonic < 0)                                  { stableTonic = r.tonic; stableMinor = r.minor; pendingKey = 0; }
        else if (r.tonic == stableTonic && r.minor == stableMinor) pendingKey = 0;
        else if (++pendingKey >= 3 || r.keyConfidence > 0.85f)    { stableTonic = r.tonic; stableMinor = r.minor; pendingKey = 0; }
        else { r.tonic = stableTonic; r.minor = stableMinor; r.keyConfidence *= 0.7f; }
    }
    // ---- Stimmung glätten
    if (r.hasTuning)
    {
        smoothDetune = haveTune ? smoothDetune + 0.5f * (r.detuneCents - smoothDetune) : r.detuneCents;
        haveTune = true;
        r.detuneCents = smoothDetune;
    }
    // ---- BPM: erst nach 2 übereinstimmenden Messungen springen
    if (r.hasBpm)
    {
        if (stableBpm <= 0.f) stableBpm = r.bpm;
        else if (std::abs (r.bpm - stableBpm) / stableBpm < 0.012f) { stableBpm = r.bpm; pendingBpm = 0; }
        else if (++pendingBpm >= 2) { stableBpm = r.bpm; pendingBpm = 0; }
        r.bpm = stableBpm;
        r.bpmAlt = stableBpm >= 110.f ? stableBpm * 0.5f : stableBpm * 2.f;
    }
}

void TaubenKeyProcessor::analyseOnce()
{
    if (resetFlag.exchange (false))
    {
        stableTonic = -1; pendingKey = 0; haveTune = false; stableBpm = 0.f; pendingBpm = 0;
        validFrom.store (totalWritten.load());
        const juce::ScopedLock sl (resultLock);
        result = {};
        ++resultVersion;
        return;
    }
    if (pFreeze->load() > 0.5f) return;

    static const double windows[3] = { 6.0, 12.0, 20.0 };
    const int wi = juce::jlimit (0, 2, (int) std::lround (pWindow->load()));

    double sr = 44100.0;
    int64_t want = 0;
    {
        const juce::ScopedLock sl (ringLock);
        if (ring.empty()) return;
        sr = sampleRate;
        const int64_t total = totalWritten.load (std::memory_order_acquire);
        const int64_t avail = std::min<int64_t> (total - validFrom.load(), ringCap - (int64_t) (4.0 * sr));
        want = std::min<int64_t> ((int64_t) (windows[wi] * sr), avail);
        if (want >= (int64_t) sr)
        {
            work.resize ((size_t) want);
            for (int64_t i = 0; i < want; ++i)
                work[(size_t) i] = ring[(size_t) ((total - want + i) % ringCap)];
        }
    }

    tk::AnalysisResult r;
    if (want >= (int64_t) sr)
    {
        double e = 0.0;
        for (auto v : work) e += (double) v * v;
        const double rms = std::sqrt (e / (double) work.size());
        r.signalPresent = rms > 3e-4;
        if (r.signalPresent)
        {
            tk::analyseKeyAudio (work.data(), (int) work.size(), sr, r);
            tk::analyseTempo    (work.data(), (int) work.size(), sr, r);
        }
    }

    // ---- MIDI hat Vorrang (exakte Noten statt Spektrum)
    std::array<float, 12> mc {};
    float msum = 0.f;
    for (size_t i = 0; i < 12; ++i) { mc[i] = midiChroma[i].load(); msum += mc[i]; }
    const bool midiPresent = msum > 1.0f;
    if (midiPresent)
    {
        tk::AnalysisResult m;
        tk::rankKeys (mc, m);
        if (m.hasKey)
        {
            r.hasKey = true; r.tonic = m.tonic; r.minor = m.minor; r.keyConfidence = m.keyConfidence;
            r.top = m.top; r.chroma = m.chroma; r.fromMidi = true;
        }
    }

    if (! r.signalPresent && ! midiPresent)
    {
        // Ruhe: letztes Ergebnis behalten ("Gedächtnis"), nur Status aktualisieren
        const juce::ScopedLock sl (resultLock);
        if (result.signalPresent) { result.signalPresent = false; result.heard.fill (0.f); ++resultVersion; }
        return;
    }

    stabilise (r);
    const juce::ScopedLock sl (resultLock);
    result = r;
    ++resultVersion;
}

//==============================================================================
juce::AudioProcessorEditor* TaubenKeyProcessor::createEditor() { return new TaubenKeyEditor (*this); }

void TaubenKeyProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml()) copyXmlToBinary (*xml, dest);
}

void TaubenKeyProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new TaubenKeyProcessor(); }
