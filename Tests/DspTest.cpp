// Offline-test av DSP-kjernen: statisk kurve, stabilitet og auto-release.
// Bygg: g++ -O2 -std=c++17 Tests/DspTest.cpp -o dsptest && ./dsptest
#include "../Source/VariMuCompressor.h"
#include <cstdio>
#include <vector>

static const double sr = 96000.0;

static float rmsDb (const std::vector<float>& v, size_t from, size_t to)
{
    double s = 0; for (size_t i = from; i < to; ++i) s += v[i] * v[i];
    return (float) (10.0 * std::log10 (s / double (to - from) + 1e-20));
}

static std::vector<float> sine (float dbfs, double secs, double f = 1000.0)
{
    std::vector<float> v ((size_t) (secs * sr));
    const float a = std::pow (10.0f, dbfs / 20.0f);
    for (size_t i = 0; i < v.size(); ++i) v[i] = a * (float) std::sin (2 * M_PI * f * i / sr);
    return v;
}

static float runStatic (float inDb, float thresh, float color = 0.0f)
{
    varimu::Compressor c; c.prepare (sr);
    varimu::Compressor::Params p; p.threshold = thresh; p.timeConstant = 1; p.color = color; p.link = false;
    c.setParams (p);
    auto v = sine (inDb, 2.0);
    float* d[1] = { v.data() };
    c.process (d, 1, (int) v.size());
    for (float x : v) if (! std::isfinite (x)) return 999.0f;
    return rmsDb (v, v.size() / 2, v.size()) + 3.01f; // sinus-RMS -> topp-dBFS
}

// tid (s) før GR faller under 1 dB etter at et signal stopper
static double releaseTime (int tc, double burstSecs)
{
    varimu::Compressor c; c.prepare (sr);
    varimu::Compressor::Params p; p.threshold = 8; p.timeConstant = tc; p.color = 0; p.link = false;
    c.setParams (p);
    auto v = sine (-6, burstSecs);
    float* d[1] = { v.data() };
    c.process (d, 1, (int) v.size());
    // stillhet: les GR blokkvis til den har sluppet
    const int block = 96;
    std::vector<float> q (block, 0.0f);
    for (int b = 0; b < (int) (60 * sr / block); ++b)
    {
        std::fill (q.begin(), q.end(), 0.0f);
        float* dq[1] = { q.data() };
        if (c.process (dq, 1, block) < 1.0f)
            return b * block / sr;
    }
    return 60.0;
}

int main()
{
    std::printf ("== Statisk kurve (threshold 5 => -20 dBFS), color 0 ==\n");
    std::printf (" inn dBFS | ut dBFS | GR dB | lokal ratio\n");
    float prevIn = 0, prevOut = 0; bool first = true;
    for (float in = -36; in <= 0.1f; in += 4)
    {
        float out = runStatic (in, 5);
        float ratio = first ? 0 : (in - prevIn) / std::max (0.001f, out - prevOut);
        std::printf ("  %6.1f  | %6.1f  | %5.1f | %s%.1f:1\n", in, out, in - out, first ? "  -" : "", first ? 0 : ratio);
        prevIn = in; prevOut = out; first = false;
    }

    std::printf ("\n== Stabilitet: maks color, ekstremt nivå ==\n");
    std::printf ("  +12 dBFS inn, color 1: ut %.2f dBFS (999 = NaN/inf)\n", runStatic (12, 10, 1));

    std::printf ("\n== Release (tid til GR < 1 dB) ==\n");
    for (int tc = 1; tc <= 6; ++tc)
        std::printf ("  TC%d: kort topp (50 ms): %5.2f s   vedvarende (8 s): %5.2f s\n",
                     tc, releaseTime (tc, 0.05), releaseTime (tc, 8.0));

    std::printf ("\n== Harmonisk innhold, 1 kHz, -10 dBFS inn, threshold 6 ==\n");
    for (float col : { 0.0f, 0.4f, 0.7f, 1.0f })
    {
        varimu::Compressor c; c.prepare (sr);
        varimu::Compressor::Params p; p.threshold = 6; p.color = col; p.link = false; c.setParams (p);
        auto v = sine (-10, 2.0); float* d[1] = { v.data() }; c.process (d, 1, (int) v.size());
        auto goertzel = [&] (double f) { double re = 0, im = 0; size_t s0 = v.size() / 2;
            for (size_t i = s0; i < v.size(); ++i) { re += v[i] * std::cos (2 * M_PI * f * i / sr); im += v[i] * std::sin (2 * M_PI * f * i / sr); }
            return 20 * std::log10 (std::sqrt (re * re + im * im) + 1e-12); };
        const double h1 = goertzel (1000), h2 = goertzel (2000), h3 = goertzel (3000);
        std::printf ("  Color %3.0f %%: H2 %6.1f dB  H3 %6.1f dB (relativt grunntone)\n", col * 100, h2 - h1, h3 - h1);
    }
    return 0;
}
