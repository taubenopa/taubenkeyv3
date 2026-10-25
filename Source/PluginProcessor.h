#pragma once
#include <JuceHeader.h>
#include "Analysis/Analysis.h"

class TaubenKeyProcessor : public juce::AudioProcessor
{
public:
    TaubenKeyProcessor();
    ~TaubenKeyProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "TaubenKey"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // ---- Zugriff für die GUI
    tk::AnalysisResult getResult (uint32_t* version = nullptr) const;
    float getLevel() const { return level.load(); }
    float getHostBpm() const { return hostBpm.load(); }
    void  requestReset();

    juce::AudioProcessorValueTreeState apvts;
    juce::MidiKeyboardState keyboardState;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    // ---------- Hintergrund-Analyse
    struct Worker : juce::Thread
    {
        explicit Worker (TaubenKeyProcessor& o) : juce::Thread ("TaubenKey Analyse"), owner (o) {}
        void run() override
        {
            while (! threadShouldExit())
            {
                wait (300);
                if (threadShouldExit()) break;
                owner.analyseOnce();
            }
        }
        TaubenKeyProcessor& owner;
    };
    void analyseOnce();
    void stabilise (tk::AnalysisResult&);

    // ---------- Ringpuffer (Audio-Thread schreibt lock-free, Analyse-Thread liest)
    juce::CriticalSection ringLock;          // schützt nur Neuallokation (prepareToPlay)
    std::vector<float> ring;
    int64_t ringCap = 0;
    std::atomic<int64_t> totalWritten { 0 };
    std::atomic<int64_t> validFrom { 0 };
    double sampleRate = 44100.0;
    std::vector<float> work;

    // ---------- Ergebnis
    mutable juce::CriticalSection resultLock;
    tk::AnalysisResult result;
    uint32_t resultVersion = 0;

    // Stabilisierung (nur Analyse-Thread)
    std::atomic<bool> resetFlag { false };
    int stableTonic = -1; bool stableMinor = false; int pendingKey = 0;
    bool haveTune = false; float smoothDetune = 0.f;
    float stableBpm = 0.f; int pendingBpm = 0;

    // ---------- MIDI
    std::array<std::atomic<float>, 12> midiChroma;
    std::array<bool, 128> held {};
    std::atomic<float> level { 0.f };
    std::atomic<float> hostBpm { 0.f };

    // ---------- Test-Synth ("Gurr-Ton")
    struct Voice { int note = -1; double phase = 0; float env = 0, vel = 0, att = 0; bool held = false; };
    std::array<Voice, 12> voices;
    void noteOn (int note, float vel);
    void noteOff (int note);
    void renderVoices (juce::AudioBuffer<float>&, int numSamples, float detuneCents);

    std::atomic<float>* pWindow = nullptr;
    std::atomic<float>* pFreeze = nullptr;
    std::atomic<float>* pTest   = nullptr;
    std::atomic<float>* pDetune = nullptr;

    Worker worker;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TaubenKeyProcessor)
};
