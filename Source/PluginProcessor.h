
#pragma once

#include <JuceHeader.h>

//==============================================================================
/**
*/
class VocoderAudioProcessor  : public AudioProcessor,
                               public juce::AudioProcessorValueTreeState::Listener
{
public:
    //==============================================================================
    VocoderAudioProcessor();
    ~VocoderAudioProcessor();

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    void processBlock (AudioBuffer<float>&, MidiBuffer&) override;

    //==============================================================================
    AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const String getProgramName (int index) override;
    void changeProgramName (int index, const String& newName) override;

    //==============================================================================
    void getStateInformation (MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    void updateFilter();

private:

    juce::AudioProcessorValueTreeState valueTree;

    int lastSampleRate{};

    using Filter = juce::dsp::ProcessorDuplicator<dsp::IIR::Filter<float>, dsp::IIR::Coefficients <float>>;
    constexpr static int numFilters = 24;
    std::array<Filter, numFilters> filters;

    

    std::atomic<float>* numBands_ = nullptr;
    std::atomic<float>* lowFreq_  = nullptr;
    std::atomic<float>* highFreq_ = nullptr;
    std::atomic<float>* Q_ = nullptr;
    std::atomic<float>* outGain_  = nullptr;

    void parameterChanged(const juce::String& parameterID, float newValue) override;
   
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VocoderAudioProcessor)
};
