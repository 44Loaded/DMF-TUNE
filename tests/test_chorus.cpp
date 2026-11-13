// Quick self-check for the chorus: amount 0 must be bit-exact dry, amount 1 must stay finite and audible.
#include "../Source/DMFTuneChorus.h"
#include <cstdio>
#include <cmath>

int main()
{
    const double sr = 48000.0;
    const int n = 48000;
    std::vector<float> dry (n), l (n), r (n);
    for (int i = 0; i < n; ++i)
        dry[i] = 0.5f * std::sin (2.0 * 3.14159265358979 * 440.0 * i / sr);

    int fails = 0;

    // amount 0: dry in, dry out
    {
        dmftune::Chorus c;
        c.prepare (sr, 2);
        l = dry; r = dry;
        float* io[2] = { l.data(), r.data() };
        c.process (io, 2, n, 0.0f);
        for (int i = 0; i < n; ++i)
            if (l[i] != dry[i] || r[i] != dry[i]) { ++fails; break; }
    }

    // amount 1: finite and with energy
    {
        dmftune::Chorus c;
        c.prepare (sr, 2);
        l = dry; r = dry;
        float* io[2] = { l.data(), r.data() };
        c.process (io, 2, n, 1.0f);
        double e = 0.0;
        bool finite = true;
        for (int i = 0; i < n; ++i)
        {
            finite = finite && std::isfinite (l[i]) && std::isfinite (r[i]);
            e += (double) l[i] * l[i];
        }
        if (! finite || e <= 0.0) ++fails;
        std::printf ("chorus rms L = %.4f\n", std::sqrt (e / n));
    }

    std::printf (fails == 0 ? "CHORUS TESTS PASSED\n" : "CHORUS TESTS FAILED (%d)\n", fails);
    return fails == 0 ? 0 : 1;
}
