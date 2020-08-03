#include "PluginProcessor.h"
#include "PluginEditor.h"

AudioProcessorValueTreeState::ParameterLayout createParameters(){
    
    NormalisableRange<float> cutoffRange (20.f, 20000.f, 1.f);
    cutoffRange.setSkewForCentre(1000.f);
    
    return {
        
        //Number of Bands
        std::make_unique<AudioParameterInt>("num_bands",
                                            "Num Bands",
                                            1, 12, 1),
        
        //Low F
        std::make_unique<AudioParameterFloat>("low_freq",
                                            "Low Frequency",
                                            cutoffRange,
                                              20.f),
        
        
        //High F
        std::make_unique<AudioParameterFloat>("high_freq",
                                              "High Frequency",
                                              cutoffRange,
                                              20000.f),

        //Q
        std::make_unique<AudioParameterFloat>("q",
                                              "Q",
                                              0.1f, 5.f, 1.f),                                      
        
        //Out Gain
        std::make_unique<AudioParameterFloat>("out_gain",
                                              "Output Gain",
                                              0.f, 4.f, 1.f)
        

        
    };
    
}


VocoderAudioProcessor::VocoderAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", AudioChannelSet::stereo(), true)
                     #endif
                       ),
valueTree(*this, nullptr, juce::Identifier("VSTurbo Vocoder"), createParameters())

#endif
{
    numBands_ = valueTree.getRawParameterValue("num_bands");
    lowFreq_  = valueTree.getRawParameterValue("low_freq");
    highFreq_ = valueTree.getRawParameterValue("high_freq");
    Q_        = valueTree.getRawParameterValue("q");
    outGain_  = valueTree.getRawParameterValue("out_gain");

    valueTree.addParameterListener("num_bands", this);
    valueTree.addParameterListener("low_freq", this);
    valueTree.addParameterListener("high_freq", this);

}

VocoderAudioProcessor::~VocoderAudioProcessor()
{
}

//==============================================================================
const String VocoderAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool VocoderAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool VocoderAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool VocoderAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double VocoderAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int VocoderAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int VocoderAudioProcessor::getCurrentProgram()
{
    return 0;
}

void VocoderAudioProcessor::setCurrentProgram (int index)
{
}

const String VocoderAudioProcessor::getProgramName (int index)
{
    return {};
}

void VocoderAudioProcessor::changeProgramName (int index, const String& newName)
{
}

//==============================================================================
void VocoderAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    lastSampleRate = sampleRate;

    dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = samplesPerBlock;
    spec.numChannels = getTotalNumOutputChannels();

    for(auto& f : filters){f.prepare(spec); f.reset();}

    updateFilter();
}

void VocoderAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool VocoderAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    ignoreUnused (layouts);
    return true;
  #else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    if (layouts.getMainOutputChannelSet() != AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}
#endif

void VocoderAudioProcessor::updateFilter()
{
    int numBands = numBands_->load();
    float highFreq = highFreq_->load();
    float frequency = lowFreq_->load();
    float q = Q_->load();

    float freqStep = highFreq / static_cast<float>(numBands);
    float freqFactor = highFreq / (highFreq - freqStep) * 3.f; //Berechnung hier is noch falsch

    auto coeffs = std::array<juce::dsp::IIR::Coefficients<float>::Ptr, numFilters> {};
    
    for(int i = 0; i < numBands; i++)
    {
        if (frequency < highFreq)
        {
            coeffs[i] = juce::dsp::IIR::Coefficients<float>::makeBandPass(lastSampleRate, frequency, q);
        }

        frequency *= freqFactor;
    }
}


void VocoderAudioProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer& midiMessages)
{
    ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());


    auto block   = juce::dsp::AudioBlock<float>(buffer);
    auto context = juce::dsp::ProcessContextReplacing<float>(block);

    for (int i = 0; i < numBands_->load(); i++)
        {
           filters[i].process(context);
        }

    

}

//==============================================================================
bool VocoderAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

AudioProcessorEditor* VocoderAudioProcessor::createEditor()
{
    return new juce::GenericAudioProcessorEditor (*this);
}

//==============================================================================
void VocoderAudioProcessor::getStateInformation (MemoryBlock& destData)
{
    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.
}

void VocoderAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.
}

void VocoderAudioProcessor::parameterChanged(const juce::String& parameterID, float newValue)
{
    juce::ignoreUnused(parameterID, newValue);
    updateFilter();
}

//==============================================================================
// This creates new instances of the plugin..
AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new VocoderAudioProcessor();
}
