#include "dsp/PerformanceEngine.h"
#include <juce_audio_utils/juce_audio_utils.h>

class AetherPianoAudioProcessor final : public juce::AudioProcessor {
public:
    AetherPianoAudioProcessor() : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)) {}
    const juce::String getName() const override { return "Aether Piano"; }
    void prepareToPlay(double rate, int block) override { (void) engine_.prepare(rate, static_cast<std::uint32_t>(block)); }
    void releaseResources() override { engine_.reset(); }
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override { return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo(); }
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) override {
        juce::ScopedNoDenormals noDenormals;
        const int n=buffer.getNumSamples();
        if (buffer.getNumChannels() >= 2) {
            auto* left = buffer.getWritePointer(0);
            auto* right = buffer.getWritePointer(1);
            int cursor = 0;
            for (const auto metadata : midiMessages) {
                const auto& message = metadata.getMessage();
                const int eventSample = juce::jlimit(cursor, n, metadata.samplePosition);
                if (eventSample > cursor) {
                    engine_.process(left + cursor, right + cursor,
                        static_cast<std::uint32_t>(eventSample - cursor));
                    cursor = eventSample;
                }
                if (message.isNoteOn() || message.isNoteOff() || message.isController()) {
                    const auto data2 = message.isController()
                        ? static_cast<std::uint8_t>(message.getControllerValue())
                        : (message.isNoteOn()
                        ? static_cast<std::uint8_t>(juce::jlimit(1, 127,
                            juce::roundToInt(message.getVelocity() * 127.0f)))
                        : static_cast<std::uint8_t>(0));
                    const aether::MidiEvent event{
                        static_cast<std::uint32_t>(eventSample),
                        static_cast<std::uint8_t>(message.getRawData()[0]),
                        message.isController()
                            ? static_cast<std::uint8_t>(message.getControllerNumber())
                            : static_cast<std::uint8_t>(message.getNoteNumber()),
                        data2};
                    engine_.handleEvent(event);
                }
            }
            if (cursor < n)
                engine_.process(left + cursor, right + cursor,
                    static_cast<std::uint32_t>(n - cursor));
        }
        else if (buffer.getNumChannels() == 1) buffer.clear();
        for (int ch=2; ch<buffer.getNumChannels(); ++ch) buffer.clear(ch,0,n);
    }
    juce::AudioProcessorEditor* createEditor() override { return new juce::GenericAudioProcessorEditor(*this); }
    bool hasEditor() const override { return true; }
    double getTailLengthSeconds() const override { return 1.5; }
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
    aether::PerformanceEngine engine_;
};

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new AetherPianoAudioProcessor(); }
