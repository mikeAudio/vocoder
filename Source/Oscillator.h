
#pragma once

class Oscillator
{
    
    public:
    
    Oscillator()
    {
        for(int i = 0; i < waveTable.size(); i++)
        {
            waveTable[i] = 2.f / waveTable.size() * i - 1.f;
        }
    }

    
    void prepare(double sr)
    {
        sampleRate = sr;
        increment = frequency * waveTable.size() / sampleRate;
        phase = fmod(phase + increment, waveTable.size());
    }
    
    void setFrequency(float f)
    {
        increment = f * waveTable.size() / sampleRate;
        frequency = f;
    }
  
    float processSample()
    {
        phase = fmod(phase + increment, waveTable.size());
        return waveTable[static_cast<int>(phase)];
    }
    
    void processBuffer(juce::AudioBuffer<float>& buffer)
    {
        for(int i = 0; i < buffer.getNumSamples(); i++)
        {
            phase = fmod(phase + increment, waveTable.size());
            
            for(int channel = 0; channel < buffer.getNumChannels(); channel++)
            {
                auto* writePointer = buffer.getWritePointer(channel);
                writePointer[i] = waveTable[static_cast<int>(phase)];
            }
        }
    }
    
    
private:
    
    std::array<float, 1024> waveTable;
    float frequency{440};
    float phase{};
    float increment{};
    double sampleRate{};
    
    
};
