#include "Metering.h"

void Metering::reset() noexcept
{
    fifo.reset();
    pending = {};
}

void Metering::merge(MeterBlock& destination, const MeterBlock& source) noexcept
{
    for (int channel = 0; channel < 2; ++channel)
    {
        destination.sumSquares[channel] += source.sumSquares[channel];
        destination.peaks[channel] = juce::jmax(destination.peaks[channel], source.peaks[channel]);
    }

    destination.numSamples += source.numSamples;
    destination.clipped = destination.clipped || source.clipped;
}

void Metering::process(const juce::AudioBuffer<float>& buffer) noexcept
{
    const auto numSamples = buffer.getNumSamples();
    if (numSamples <= 0)
        return;

    MeterBlock current;
    current.numSamples = numSamples;
    const auto numChannels = juce::jmin(2, buffer.getNumChannels());

    for (int channel = 0; channel < numChannels; ++channel)
    {
        const auto* samples = buffer.getReadPointer(channel);
        double sum = 0.0;
        float peak = 0.0f;

        for (int sample = 0; sample < numSamples; ++sample)
        {
            const auto value = samples[sample];
            const auto magnitude = std::abs(value);
            sum += static_cast<double>(value) * value;
            peak = juce::jmax(peak, magnitude);
            current.clipped = current.clipped || magnitude >= 1.0f;
        }

        current.sumSquares[channel] = sum;
        current.peaks[channel] = peak;
    }

    if (numChannels == 1)
    {
        current.sumSquares[1] = current.sumSquares[0];
        current.peaks[1] = current.peaks[0];
    }

    merge(pending, current);

    int start1 = 0, size1 = 0, start2 = 0, size2 = 0;
    fifo.prepareToWrite(1, start1, size1, start2, size2);

    if (size1 > 0)
    {
        queue[static_cast<size_t>(start1)] = pending;
        fifo.finishedWrite(1);
        pending = {};
    }
}

MeterBlock Metering::consume() noexcept
{
    MeterBlock result;

    while (fifo.getNumReady() > 0)
    {
        int start1 = 0, size1 = 0, start2 = 0, size2 = 0;
        fifo.prepareToRead(fifo.getNumReady(), start1, size1, start2, size2);

        for (int index = 0; index < size1; ++index)
            merge(result, queue[static_cast<size_t>(start1 + index)]);

        for (int index = 0; index < size2; ++index)
            merge(result, queue[static_cast<size_t>(start2 + index)]);

        fifo.finishedRead(size1 + size2);
    }

    return result;
}
