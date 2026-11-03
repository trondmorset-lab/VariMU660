// Rendrer plugin-editoren til PNG (for dokumentasjon/visuell sjekk)
#include "../Source/PluginProcessor.h"
#include <juce_gui_basics/juce_gui_basics.h>

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter();

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    std::unique_ptr<juce::AudioProcessor> proc (createPluginFilter());
    auto* p = dynamic_cast<VariMuAudioProcessor*> (proc.get());
    p->gainReductionDb.store (argc > 2 ? std::atof (argv[2]) : 6.0f);
    std::unique_ptr<juce::AudioProcessorEditor> ed (proc->createEditorIfNeeded());
    for (int i = 0; i < 120; ++i) juce::MessageManager::getInstance()->runDispatchLoopUntil (10);
    auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 2.0f);
    juce::File out (argc > 1 ? argv[1] : "snapshot.png");
    out.deleteFile();
    juce::FileOutputStream os (out);
    juce::PNGImageFormat().writeImageToStream (img, os);
    proc->editorBeingDeleted (ed.get());
    ed.reset();
    return 0;
}
