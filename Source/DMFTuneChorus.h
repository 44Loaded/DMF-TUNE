#pragma once

#include <algorithm>
#include <cmath>
#include <vector>

namespace dmftune
{
//==============================================================================
// Stereo chorus: two short modulated delay lines driven by slow, opposite LFOs.
// amount 0 = dry signal untouched, 1 = full chorus.
class Chorus
{
public:
    void prepare (double sampleRate, int numChannels)
    {
        sr_ = sampleRate > 1000.0 ? sampleRate : 48000.0;
        nCh_ = std::clamp (numChannels, 1, 2);

        const int need = (int) std::ceil (0.08 * sr_);  // 80 ms holds the 12 ms base delay plus depth
        size_ = 1;
        while (size_ < need)
            size_ <<= 1;
        mask_ = size_ - 1;

        for (auto& b : buf_)
            b.assign ((size_t) size_, 0.0f);

        write_ = 0;
        phase_ = 0.0;
        amountSm_ = 0.0f;
    }

    void process (float* const* io, int numCh, int numSamples, float amount)
    {
        if (io == nullptr || numSamples <= 0 || numCh <= 0)
            return;

        const int nc = std::min (numCh, nCh_);
        const float smoothK = 1.0f - std::exp (-1.0f / (0.03f * (float) sr_));
        const double baseD = 0.012 * sr_;   // 12 ms centre delay
        const double depthD = 0.004 * sr_;  // +/- 4 ms sweep
        const double inc = 0.6 / sr_;       // 0.6 Hz LFO

        for (int i = 0; i < numSamples; ++i)
        {
            amountSm_ += smoothK * (amount - amountSm_);

            const double lfoL = std::sin (kTwoPi * phase_);
            const double lfoR = std::sin (kTwoPi * (phase_ + 0.25));
            phase_ += inc;
            if (phase_ >= 1.0)
                phase_ -= 1.0;

            for (int c = 0; c < nc; ++c)
            {
                float* b = buf_[(size_t) c].data();
                const float dry = io[c][i];
                b[write_] = dry;

                const double lfo = c == 0 ? lfoL : lfoR;
                const double rpos = (double) write_ + (double) size_ - (baseD + depthD * lfo);
                const long n0 = (long) std::floor (rpos);
                const float frac = (float) (rpos - (double) n0);
                const float a = b[n0 & mask_];
                const float bn = b[(n0 + 1) & mask_];
                const float wet = a + (bn - a) * frac;

                io[c][i] = dry * (1.0f - 0.5f * amountSm_) + wet * amountSm_;
            }

            write_ = (write_ + 1) & mask_;
        }
    }

private:
    static constexpr double kTwoPi = 6.283185307179586;

    double sr_ = 48000.0;
    int nCh_ = 2;
    int size_ = 4096, mask_ = 4095, write_ = 0;
    std::vector<float> buf_[2];
    double phase_ = 0.0;
    float amountSm_ = 0.0f;
};
} // namespace dmftune
