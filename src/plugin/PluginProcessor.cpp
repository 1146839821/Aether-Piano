#include "dsp/PianoEngine.h"
#include <juce_audio_utils/juce_audio_utils.h>

class AetherPianoAudioProcessor final : public juce::AudioProcessor {
public:
    AetherPianoAudioProcessor() : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)) {}
    const juce::String getName() const override { return "Aether Piano"; }
    void prepareToPlay(double rate, int block) override { engine_.prepare(rate, static_cast<std::uint32_t>(block)); }
    void releaseResources() override { engine_.reset(); }
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override { return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo(); }
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override {
        juce::ScopedNoDenormals noDenormals;
        const int n=buffer.getNumSamples();
        if (buffer.getNumChannels() >= 2) engine_.process(buffer.getWritePointer(0), buffer.getWritePointer(1), static_cast<std::uint32_t>(n));
        else if (buffer.getNumChannels() == 1) buffer.clear();
        for (int ch=2; ch<buffer.getNumChannels(); ++ch) buffer.clear(ch,0,n);
    }
    juce::AudioProcessorEditor* createEditor() override { return new juce::GenericAudioProcessorEditor(*this); }
    bool hasEditor() const override { return true; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override {}
    void setStateInformation(const void*, int) override {}
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
private:
    aether::PianoEngine engine_;
};

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new AetherPianoAudioProcessor(); }
