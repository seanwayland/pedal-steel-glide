#include "PluginProcessor.h"
#include "PluginEditor.h"

PedalSteelGlideProcessor::PedalSteelGlideProcessor()
    // A MIDI effect wants no audio I/O, but some hosts still probe for a bus -
    // a disabled stereo output keeps everyone happy.
    : juce::AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), false)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
    pPoly       = apvts.getRawParameterValue ("poly");
    pMaster     = apvts.getRawParameterValue ("master");
    pLegato     = apvts.getRawParameterValue ("legato");
    pGlideMin   = apvts.getRawParameterValue ("glideMin");
    pGlideMax   = apvts.getRawParameterValue ("glideMax");
    pCurve      = apvts.getRawParameterValue ("curve");
    pBendRange  = apvts.getRawParameterValue ("bendRange");
    pWheelRange = apvts.getRawParameterValue ("wheelRange");
    pVibRate    = apvts.getRawParameterValue ("vibRate");
    pVibDepth   = apvts.getRawParameterValue ("vibDepth");
    pVibRandom  = apvts.getRawParameterValue ("vibRandom");
    pVibSource  = apvts.getRawParameterValue ("vibSource");
    pCC7        = apvts.getRawParameterValue ("cc7");
    pAutoRange  = apvts.getRawParameterValue ("autoRange");
}

juce::AudioProcessorValueTreeState::ParameterLayout PedalSteelGlideProcessor::createLayout()
{
    using F = juce::AudioParameterFloat;
    using I = juce::AudioParameterInt;
    using C = juce::AudioParameterChoice;
    using B = juce::AudioParameterBool;
    using Rn = juce::NormalisableRange<float>;
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;

    p.push_back (std::make_unique<I> (juce::ParameterID { "poly", 1 }, "Polyphony", 1, 8, 6));
    p.push_back (std::make_unique<I> (juce::ParameterID { "master", 1 }, "Master Channel", 1, 15, 1));
    p.push_back (std::make_unique<F> (juce::ParameterID { "legato", 1 }, "Legato Interval",
        Rn (1.0f, 12.0f, 0.5f), 2.0f, juce::AudioParameterFloatAttributes().withLabel ("st")));
    p.push_back (std::make_unique<F> (juce::ParameterID { "glideMin", 1 }, "Glide Fast (hard)",
        Rn (2.0f, 300.0f, 1.0f, 0.5f), 40.0f, juce::AudioParameterFloatAttributes().withLabel ("ms")));
    p.push_back (std::make_unique<F> (juce::ParameterID { "glideMax", 1 }, "Glide Slow (soft)",
        Rn (50.0f, 4000.0f, 1.0f, 0.4f), 1200.0f, juce::AudioParameterFloatAttributes().withLabel ("ms")));
    p.push_back (std::make_unique<F> (juce::ParameterID { "curve", 1 }, "Glide Curve",
        Rn (1.0f, 4.0f, 0.05f), 2.0f));
    p.push_back (std::make_unique<I> (juce::ParameterID { "bendRange", 1 }, "Bend Range", 1, 24, 12));
    p.push_back (std::make_unique<F> (juce::ParameterID { "wheelRange", 1 }, "Wheel Range",
        Rn (0.0f, 12.0f, 0.1f), 2.0f, juce::AudioParameterFloatAttributes().withLabel ("st")));
    p.push_back (std::make_unique<F> (juce::ParameterID { "vibRate", 1 }, "Vibrato Rate",
        Rn (0.1f, 10.0f, 0.01f, 0.5f), 3.0f, juce::AudioParameterFloatAttributes().withLabel ("Hz")));
    p.push_back (std::make_unique<F> (juce::ParameterID { "vibDepth", 1 }, "Vibrato Depth",
        Rn (0.0f, 1.0f, 0.001f), 0.30f, juce::AudioParameterFloatAttributes().withLabel ("st")));
    p.push_back (std::make_unique<F> (juce::ParameterID { "vibRandom", 1 }, "Vibrato Randomness",
        Rn (0.0f, 0.5f, 0.001f), 0.15f));
    p.push_back (std::make_unique<C> (juce::ParameterID { "vibSource", 1 }, "Vibrato Source",
        juce::StringArray { "Aftertouch", "Mod Wheel (CC1)", "Breath (CC2)" }, 0));
    p.push_back (std::make_unique<B> (juce::ParameterID { "cc7", 1 }, "Pass CC7 (Volume)", true));
    p.push_back (std::make_unique<B> (juce::ParameterID { "autoRange", 1 }, "Auto-Set Bend Range (RPN)", true));

    return { p.begin(), p.end() };
}

GlideParams PedalSteelGlideProcessor::currentParams() const
{
    GlideParams g;
    g.polyphony        = (int) pPoly->load();
    g.masterChannel    = (int) pMaster->load();
    g.legatoSemitones  = pLegato->load();
    g.glideMinMs       = pGlideMin->load();
    g.glideMaxMs       = juce::jmax (pGlideMin->load(), pGlideMax->load());
    g.glideCurve       = pCurve->load();
    g.bendRangeSemis   = (int) pBendRange->load();
    g.wheelRangeSemis  = pWheelRange->load();
    g.vibratoRateHz    = pVibRate->load();
    g.vibratoDepthSemis = pVibDepth->load();
    g.vibratoRandom    = pVibRandom->load();
    const int src = (int) pVibSource->load();
    g.vibratoSourceCC  = src == 1 ? 1 : (src == 2 ? 2 : -1);
    g.cc7Passthrough   = pCC7->load() > 0.5f;
    g.autoBendRange    = pAutoRange->load() > 0.5f;
    return g;
}

void PedalSteelGlideProcessor::prepareToPlay (double sampleRate, int)
{
    sr = sampleRate > 0.0 ? sampleRate : 48000.0;
    engine.prepare (sr);
}

void PedalSteelGlideProcessor::run (juce::MidiBuffer& midi, int numSamples)
{
    engine.process (midi, numSamples, currentParams());
}

void PedalSteelGlideProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals nd;
    buffer.clear();
    run (midi, buffer.getNumSamples());
}

void PedalSteelGlideProcessor::processBlock (juce::AudioBuffer<double>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals nd;
    buffer.clear();
    run (midi, buffer.getNumSamples());
}

juce::AudioProcessorEditor* PedalSteelGlideProcessor::createEditor()
{
    return new PedalSteelGlideEditor (*this);
}

void PedalSteelGlideProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, dest);
}

void PedalSteelGlideProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PedalSteelGlideProcessor();
}
