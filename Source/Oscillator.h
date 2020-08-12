
#pragma once

class Oscillator
{
    
    public:
    
    Oscillator()
    {
        for(int i = 0; i < waveTableSize; i++)
        {
            waveTable[i] = 2.f / waveTableSize * i - 1.f;
        } 
    }

    
    void prepare(double sampleRate)
    {
        sampleRate_ = sampleRate;
        increment = frequency * waveTableSize / sampleRate;
        phase = fmod(phase + increment, waveTableSize);
        setWaveform(1);
    }
    
    
    void setFrequency(float f)
    {
        increment = f * waveTableSize / sampleRate_;
        frequency = f;
    }
    
    
    void setWaveform(int w)
    {
        switch(w)
        {
                
            case 0:
                for(int i = 0; i < waveTableSize; i++)
                {
                    waveTable[i] = std::sin(double_Pi * 2.f * i / waveTableSize);
                }
                break;
                
            case 1:
                for(int i = 0; i < waveTableSize; i++)
                {
                    waveTable[i] = 2.f / waveTableSize * i - 1.f;
                }
                break;
                
            case 2:
                for(int i = 0; i < waveTableSize; i++)
                {
                    waveTable[i] = i < waveTableSize / 2 ? 1.f : -1.f;
                }
                break;
                
            case 3:
                float wts = static_cast<float>(waveTableSize);
                
                for(int i = 0; i < waveTableSize; i++)
                {
                    
                    waveTable[i] = i < wts / 2.f ? i / wts * 4.f - 1.f : (wts - i) / wts * 4.f - 1.f;
                }
                break;
        }
    }
    
  
    float processSample()
    {
        phase = fmod(phase + increment, waveTableSize);
        return waveTable[static_cast<int>(phase)];
    }
    
    
    void processBuffer(juce::AudioBuffer<float>& buffer)
    {
        for(int i = 0; i < buffer.getNumSamples(); i++)
        {
            phase = fmod(phase + increment, waveTableSize);
            
            for(int channel = 0; channel < buffer.getNumChannels(); channel++)
            {
                auto* writePointer = buffer.getWritePointer(channel);
                writePointer[i] = waveTable[static_cast<int>(phase)];
            }
        }
    }
    
    
private:
    
    int constexpr static waveTableSize = 1024;
    std::array<float, waveTableSize> waveTable;
    
    float frequency{440};
    float phase{};
    float increment{};
    double sampleRate_{44100};
    
    
};
