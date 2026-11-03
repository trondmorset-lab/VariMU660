// Minimal VST3-vert: laster plugin-binæren, kjører lyd, sjekker state og tar GUI-skjermbilde.
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <iostream>

using namespace juce;

static int failures = 0;
static void check (bool ok, const String& what)
{
    std::cout << (ok ? "  [OK]   " : "  [FEIL] ") << what << std::endl;
    if (! ok) ++failures;
}

int main (int argc, char** argv)
{
    ScopedJuceInitialiser_GUI gui;
    if (argc < 3) { std::cout << "bruk: hosttest <plugin.vst3> <screenshot.png>\n"; return 2; }

    AudioPluginFormatManager fm;
    fm.addFormat (new VST3PluginFormat());

    OwnedArray<PluginDescription> found;
    VST3PluginFormat vst3;
    vst3.findAllTypesForFile (found, argv[1]);
    check (found.size() == 1, "Fant plugin i .vst3-bundle");
    if (found.isEmpty()) return 1;
    std::cout << "  Navn: " << found[0]->name << "  Produsent: " << found[0]->manufacturerName
              << "  Kategori: " << found[0]->category << std::endl;

    String err;
    auto plugin = fm.createPluginInstance (*found[0], 48000.0, 512, err);
    check (plugin != nullptr, "Instansierte plugin " + err);
    if (! plugin) return 1;

    std::cout << "  Parametere:";
    for (auto* p : plugin->getParameters()) std::cout << " [" << p->getName (32) << "]";
    std::cout << std::endl;

    for (double sr : { 44100.0, 48000.0, 96000.0 })
    for (int bs : { 32, 512, 1024 })
    {
        plugin->setPlayConfigDetails (2, 2, sr, bs);
        plugin->prepareToPlay (sr, bs);
        AudioBuffer<float> buf (2, bs);
        MidiBuffer midi;
        double phase = 0; bool finite = true; float peak = 0;
        for (int b = 0; b < (int) (sr * 2 / bs); ++b)
        {
            for (int i = 0; i < bs; ++i)
            {
                const float v = 0.9f * (float) std::sin (phase); phase += 2 * MathConstants<double>::pi * 220.0 / sr;
                buf.setSample (0, i, v); buf.setSample (1, i, v * 0.5f);
            }
            plugin->processBlock (buf, midi);
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < bs; ++i)
                {
                    const float s = buf.getSample (ch, i);
                    finite = finite && std::isfinite (s);
                    peak = jmax (peak, std::abs (s));
                }
        }
        check (finite && peak > 0.01f && peak < 4.0f,
               "Prosessering " + String (sr, 0) + " Hz / " + String (bs) + " samples, topp ut "
               + String (Decibels::gainToDecibels (peak), 1) + " dBFS, latens " + String (plugin->getLatencySamples()));
        plugin->releaseResources();
    }

    // Automatisering av alle parametere mens lyd går
    plugin->prepareToPlay (48000.0, 256);
    {
        AudioBuffer<float> buf (2, 256); MidiBuffer midi; Random r (1); bool finite = true;
        for (int b = 0; b < 400; ++b)
        {
            for (auto* p : plugin->getParameters()) p->setValue (r.nextFloat());
            for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < 256; ++i) buf.setSample (ch, i, r.nextFloat() * 2 - 1);
            plugin->processBlock (buf, midi);
            for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < 256; ++i) finite = finite && std::isfinite (buf.getSample (ch, i));
        }
        check (finite, "Tilfeldig parameterautomasjon + støy: ingen NaN/inf");
    }

    // State lagre/gjenopprett: endre alt, lagre, endre igjen, gjenopprett, lagre på nytt => lik state
    {
        auto params = plugin->getParameters();
        for (auto* p : params) p->setValue (0.3f);
        MemoryBlock a; plugin->getStateInformation (a);
        for (auto* p : params) p->setValue (0.8f);
        MemoryBlock mid; plugin->getStateInformation (mid);
        plugin->setStateInformation (a.getData(), (int) a.getSize());
        MemoryBlock b; plugin->getStateInformation (b);
        check (a == b && a != mid, "State lagres og gjenopprettes (" + String ((int) a.getSize()) + " bytes)");
    }

    // Bypass: utgangen skal være inngangen forsinket med nøyaktig latensen
    {
        plugin->setPlayConfigDetails (2, 2, 48000.0, 256);
        plugin->prepareToPlay (48000.0, 256);
        const int lat = plugin->getLatencySamples();
        AudioBuffer<float> buf (2, 256); MidiBuffer midi; Random r (7);
        std::vector<float> in, out;
        for (int b = 0; b < 8; ++b)
        {
            for (int i = 0; i < 256; ++i) { const float v = r.nextFloat() * 2 - 1; buf.setSample (0, i, v); buf.setSample (1, i, v); in.push_back (v); }
            plugin->processBlockBypassed (buf, midi);
            for (int i = 0; i < 256; ++i) out.push_back (buf.getSample (0, i));
        }
        float maxErr = 0;
        for (size_t i = (size_t) lat; i < out.size(); ++i) maxErr = jmax (maxErr, std::abs (out[i] - in[i - (size_t) lat]));
        check (lat > 0 && maxErr < 1.0e-6f, "Bypass er forsinket med latensen (" + String (lat) + " samples), maks avvik " + String (maxErr));
        plugin->releaseResources();
    }

    // GUI + skjermbilde (sett realistiske verdier så VU-måleren viser noe)
    for (auto* p : plugin->getParameters()) p->setValue (p->getDefaultValue());
    {
        AudioBuffer<float> buf (2, 256); MidiBuffer midi;
        for (int b = 0; b < 200; ++b)
        {
            for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < 256; ++i)
                buf.setSample (ch, i, 0.5f * (float) std::sin ((b * 256 + i) * 0.03));
            plugin->processBlock (buf, midi);
        }
    }
    std::unique_ptr<AudioProcessorEditor> editor (plugin->createEditorIfNeeded());
    check (editor != nullptr, "Editor opprettet (" + String (editor ? editor->getWidth() : 0) + "x"
                              + String (editor ? editor->getHeight() : 0) + ")");
    if (editor)
    {
        editor->setVisible (true);
        for (int i = 0; i < 40; ++i) MessageManager::getInstance()->runDispatchLoopUntil (25);
        auto img = editor->createComponentSnapshot (editor->getLocalBounds(), true, 2.0f);
        File out (argv[2]);
        out.deleteFile();
        FileOutputStream os (out);
        PNGImageFormat().writeImageToStream (img, os);
        check (os.getStatus().wasOk(), "Skjermbilde lagret");
        editor.reset();
    }
    plugin->releaseResources();
    plugin.reset();

    std::cout << (failures == 0 ? "\nALLE TESTER BESTÅTT\n" : "\nNOEN TESTER FEILET\n");
    return failures == 0 ? 0 : 1;
}
