#include "PluginProcessor.h"
#include "PluginEditor.h"

AudioProcessorValueTreeState::ParameterLayout createParameters(){
    
    NormalisableRange<float> cutoffRange (20.f, 20000.f, 1.f);
    cutoffRange.setSkewForCentre(3000.f);
    
    return {
        //OSC Frequency
        std::make_unique<AudioParameterFloat>("osc_freq",
                                              "OSC Frequency",
                                              cutoffRange,
                                              100.f),
        
        
        //Number of Bands
        std::make_unique<AudioParameterInt>("num_bands",
                                            "Num Bands",
                                            1, 48, 12),
        
        //Low F
        std::make_unique<AudioParameterFloat>("low_freq",
                                              "Low Frequency",
                                              cutoffRange,
                                              100.f),
        
        
        //High F
        std::make_unique<AudioParameterFloat>("high_freq",
                                              "High Frequency",
                                              cutoffRange,
                                              20000.f),

        //Q
        std::make_unique<AudioParameterFloat>("q",
                                              "Q",
                                              0.01f, 30.f, 5.f),
        
        //RMS Calc Gain
        std::make_unique<AudioParameterFloat>("rms_gain",
                                              "Modulator Gain",
                                              0.f, 12.f, 2.f),

        //Wide
        std::make_unique<AudioParameterFloat>("wide",
                                              "Freq Interval",
                                              1.1f, 5.f, 1.5f),
        
        //switch carrier & modulator
        std::make_unique<AudioParameterBool>("switch_car_mod",
                                             "Switch Carr/Mod",
                                             false),
        
        //Out Gain
        std::make_unique<AudioParameterFloat>("out_gain",
                                              "Output Gain",
                                              0.f, 12.f, 2.f)
        

        
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
valueTree(*this, nullptr, juce::Identifier("VSTurboVocoder"), createParameters())

#endif
{
    oscFreq_  = valueTree.getRawParameterValue("osc_freq");
    
    numBands_ = valueTree.getRawParameterValue("num_bands");
    lowFreq_  = valueTree.getRawParameterValue("low_freq");
    highFreq_ = valueTree.getRawParameterValue("high_freq");
    Q_        = valueTree.getRawParameterValue("q");
    rmsGain_  = valueTree.getRawParameterValue("rms_gain");
    wide_     = valueTree.getRawParameterValue("wide");
    switchCarrMod_ = valueTree.getRawParameterValue("switch_car_mod");
    outGain_  = valueTree.getRawParameterValue("out_gain");

    valueTree.addParameterListener("num_bands", this);
    valueTree.addParameterListener("low_freq", this);
    valueTree.addParameterListener("high_freq", this);
    valueTree.addParameterListener("q", this);
    valueTree.addParameterListener("wide", this);
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
    
    oscOutput.setSize(2, samplesPerBlock);
    modBuffer.setSize(2, samplesPerBlock);
    carBuffer.setSize(2, samplesPerBlock);

    for(auto& f : carrierFilters){f.prepare(spec); f.reset();}
    for(auto& f : modulatorFilters){f.prepare(spec); f.reset();}

    updateFilter();
    
    osc_.prepare(sampleRate);
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
    float wide = wide_->load();

    float freqStep = highFreq / static_cast<float>(numBands);
    float freqFactor = highFreq / (highFreq - freqStep) * wide;
    
    for(int i = 0; i < numBands; i++)
    {
        if (frequency < highFreq)
        {
            *carrierFilters[i].state = *juce::dsp::IIR::Coefficients<float>::makeBandPass(lastSampleRate, frequency, q);
            *modulatorFilters[i].state = *juce::dsp::IIR::Coefficients<float>::makeBandPass(lastSampleRate, frequency, q);
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
    
    int   const numSamples    = buffer.getNumSamples();
    int   const numChannels   = buffer.getNumChannels();
    int   const numBands      = numBands_->load();
    float const outGain       = outGain_->load();
    float const oscFreq       = oscFreq_->load();
    float const rmsGain       = rmsGain_->load();
    bool  const switchCarrMod = switchCarrMod_->load();
    

    osc_.setFrequency(oscFreq);
    osc_.processBuffer(oscOutput);
    
    
    //Calculate RMS Value for every band of the incoming signal and store it in the rmsValue-string
    for (int i = 0; i < numBands; i++)
    {
        modBuffer.clear();
        if(switchCarrMod){modBuffer.makeCopyOf(oscOutput);}
        else{modBuffer.makeCopyOf(buffer);}
        
        auto block   = juce::dsp::AudioBlock<float>(modBuffer);
        auto modulatorContext = juce::dsp::ProcessContextReplacing<float>(block);
        modulatorFilters[i].process(modulatorContext);
        
        float sum = 0;
        for (int channel = 0; channel < numChannels; channel++)
        {
            sum += modBuffer.getRMSLevel(channel, 0, numSamples) / numChannels;
        }
        
        rmsValues[i] = sum * rmsGain;
    }
    
    buffer.clear();
    
    
    //apply filter-bands on oscillator signal and apply rms-gains to each band accordingly
    for (int i = 0; i < numBands; i++)
        {
            carBuffer.clear();
            if(switchCarrMod){carBuffer.makeCopyOf(buffer);}
            else{carBuffer.makeCopyOf(oscOutput);}
            
            auto block   = juce::dsp::AudioBlock<float>(carBuffer);
            auto carrierContext = juce::dsp::ProcessContextReplacing<float>(block);
            carrierFilters[i].process(carrierContext);
            
            carBuffer.applyGain(rmsValues[i]);
            
            for (int channel = 0; channel < numChannels; channel++)
            {
                auto* filterCalcPointer = carBuffer.getWritePointer(channel);
                buffer.addFrom(channel, 0, filterCalcPointer, numSamples);
            }
        }

    buffer.applyGain(outGain);
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
