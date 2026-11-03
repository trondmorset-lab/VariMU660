// VariMuCompressor.h – Fairchild 660/670-inspirert variable-mu-kompressor.
// Ren C++ uten JUCE, så den kan testes for seg selv (Tests/DspTest.cpp).
//
//  * Feedback: sidechain leser utgangen av gain-trinnet, som i originalen.
//  * Progressiv ratio: ca. 2:1 ved terskel, øker mot ~20:1. Myk knee på 12 dB.
//  * Time Constant 1–6 med Fairchilds verdier. Auto-release (5 og 6) er et
//    RC-nettverk der de trege kondensatorene bare lades ved vedvarende kompresjon.
//  * Rørfarge: asymmetrisk metning (2. harmonisk) som øker med gain-reduksjonen,
//    pluss transformatormetning i bassen.
//
// Input/output-gain og dry/wet håndteres av prosessoren (med parameterglatting).

#pragma once

#include <algorithm>
#include <array>
#include <cmath>

namespace varimu
{

// Attack og releasetider (s). charge = hvor raskt en treg kondensator lades (0 = ikke i bruk).
struct TimeConstant
{
    float attack, release;
    float charge2, release2;
    float charge3, release3;
};

inline constexpr std::array<TimeConstant, 6> timeConstants {{
    { 0.0002f, 0.3f, 0.0f,  0.0f, 0.0f,  0.0f },   // 1
    { 0.0002f, 0.8f, 0.0f,  0.0f, 0.0f,  0.0f },   // 2
    { 0.0004f, 2.0f, 0.0f,  0.0f, 0.0f,  0.0f },   // 3
    { 0.0004f, 5.0f, 0.0f,  0.0f, 0.0f,  0.0f },   // 4
    { 0.0004f, 2.0f, 1.5f, 10.0f, 0.0f,  0.0f },   // 5: auto 2 s / 10 s
    { 0.0002f, 0.3f, 1.0f, 10.0f, 4.0f, 25.0f },   // 6: auto 0,3 s / 10 s / 25 s
}};

class Compressor
{
public:
    static constexpr int maxChannels = 2;

    struct Params
    {
        float threshold    = 4.0f;  // 0–10, høyere = mer kompresjon
        int   timeConstant = 1;     // 1–6
        float color        = 0.4f;  // 0–1
        bool  link         = true;
    };

    void prepare (double newSampleRate)
    {
        sampleRate = newSampleRate;
        dcCoeff  = onePole (8.0);    // DC-blokker
        lowCoeff = onePole (110.0);  // transformatorens bassområde
        setTimeConstant (params.timeConstant);
        reset();
    }

    void reset()
    {
        channels = {};
        linked = {};
    }

    void setParams (const Params& p)
    {
        if (p.timeConstant != params.timeConstant)
            setTimeConstant (p.timeConstant);
        params = p;
    }

    // Prosesserer på plass. Returnerer største gain-reduksjon i blokken (dB, positiv).
    float process (float* const* data, int numChannels, int numSamples)
    {
        numChannels = std::clamp (numChannels, 0, maxChannels);

        const float threshDb = -4.0f - std::clamp (params.threshold, 0.0f, 10.0f) * 3.2f; // -4 til -36 dBFS
        const float color    = std::clamp (params.color, 0.0f, 1.0f);
        const bool  isLinked = params.link && numChannels > 1;

        float maxGr = 0.0f;

        for (int i = 0; i < numSamples; ++i)
        {
            if (isLinked)
            {
                float level = 0.0f;
                for (int ch = 0; ch < numChannels; ++ch)
                    level = std::max (level, std::abs (channels[(size_t) ch].lastOut));
                linked.update (gainComputer (level, threshDb), coeffs);
            }

            for (int ch = 0; ch < numChannels; ++ch)
            {
                auto& s = channels[(size_t) ch];
                if (! isLinked)
                    s.detector.update (gainComputer (std::abs (s.lastOut), threshDb), coeffs);

                const float gr = isLinked ? linked.value() : s.detector.value();
                maxGr = std::max (maxGr, gr);

                // Variable-mu-trinnet. Sidechain (feedback) leser denne utgangen.
                const float x = data[ch][i] * std::exp (-gr * kDbToNeper);
                s.lastOut = x;

                data[ch][i] = colorStage (s, x, gr, color);
            }
        }

        return maxGr;
    }

private:
    static constexpr double kPi = 3.14159265358979323846;
    static constexpr float kDbToNeper = 0.11512925f; // ln(10)/20: gain = exp(dB * k)

    struct Coeffs { float attack = 0, release = 0, charge2 = 0, release2 = 0, charge3 = 0, release3 = 0; };

    // RC-nettverket. c1 følger attack/release; c2 og c3 er de trege
    // auto-release-kondensatorene, som bare lades mens kompressoren jobber.
    struct Detector
    {
        float c1 = 0, c2 = 0, c3 = 0;

        void update (float target, const Coeffs& k)
        {
            c1 = target > c1 ? target + (c1 - target) * k.attack : c1 * k.release;

            if (k.charge2 > 0.0f)
                c2 = target > c2 ? c2 + (std::min (target, c1) - c2) * (1.0f - k.charge2) : c2 * k.release2;
            if (k.charge3 > 0.0f)
                c3 = target > c3 ? c3 + (std::min (target, c2) - c3) * (1.0f - k.charge3) : c3 * k.release3;
        }

        float value() const { return std::max (c1, std::max (c2, c3)); }
    };

    struct ChannelState
    {
        Detector detector;
        float lastOut = 0, low = 0, dcX = 0, dcY = 0;
    };

    // Progressiv vari-mu-kurve med myk knee. I feedback gir slope k ratio (1 + k):1.
    static float gainComputer (float level, float threshDb)
    {
        constexpr float knee = 12.0f;
        const float over = 20.0f * std::log10 (level + 1.0e-9f) - threshDb;

        if (over <= -knee * 0.5f)
            return 0.0f;

        const float o = over >= knee * 0.5f ? over
                                            : (over + knee * 0.5f) * (over + knee * 0.5f) / (2.0f * knee);
        const float k = std::min (1.0f + 0.5f * o, 19.0f);
        return std::min (k * o, 40.0f);
    }

    // Rør + transformator + DC-blokker
    float colorStage (ChannelState& s, float x, float gr, float color) const
    {
        float y = x;

        if (color > 0.0f)
        {
            // Rør: biasen forskyves med gain-reduksjonen, som i et ekte vari-mu-rør.
            const float drive = 1.2f + 3.0f * color;
            const float bias  = 0.08f + 0.22f * color + 0.012f * std::min (gr, 20.0f) * color;
            const float tb    = std::tanh (bias);
            const float tube  = (std::tanh (drive * x + bias) - tb) / (drive * (1.0f - tb * tb));
            y = x + color * (tube - x);

            // Utgangstransformator: myk metning av lave frekvenser.
            const float xfDrive = 1.0f + 5.0f * color;
            s.low = lowCoeff * s.low + (1.0f - lowCoeff) * y;
            y += color * (std::tanh (s.low * xfDrive) / xfDrive - s.low);
        }

        // Asymmetrien gir DC, som fjernes her.
        const float out = y - s.dcX + dcCoeff * s.dcY;
        s.dcX = y;
        s.dcY = out;
        return out;
    }

    float onePole (double hz) const { return (float) std::exp (-2.0 * kPi * hz / sampleRate); }

    float rc (float seconds) const
    {
        return seconds > 0.0f ? (float) std::exp (-1.0 / (seconds * sampleRate)) : 0.0f;
    }

    void setTimeConstant (int position)
    {
        const auto& tc = timeConstants[(size_t) std::clamp (position, 1, 6) - 1];
        // Oppgitt releasetid er tid til nesten full gjenoppretting, så RC-tiden er ca. 1/3 av den.
        coeffs = { rc (tc.attack),  rc (tc.release / 3.0f),
                   rc (tc.charge2), rc (tc.release2 / 3.0f),
                   rc (tc.charge3), rc (tc.release3 / 3.0f) };
    }

    double sampleRate = 44100.0;
    Params params;
    Coeffs coeffs;
    float dcCoeff = 0, lowCoeff = 0;

    std::array<ChannelState, maxChannels> channels {};
    Detector linked;
};

} // namespace varimu
