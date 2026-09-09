#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

class PedalSteelGlideEditor : public juce::AudioProcessorEditor
{
public:
    explicit PedalSteelGlideEditor (PedalSteelGlideProcessor&);
    ~PedalSteelGlideEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using APVTS = juce::AudioProcessorValueTreeState;

    struct Row
    {
        juce::String label;
        std::unique_ptr<juce::Component> control;
        std::unique_ptr<juce::Label> caption;
        std::unique_ptr<APVTS::SliderAttachment> sa;
        std::unique_ptr<APVTS::ComboBoxAttachment> ca;
        std::unique_ptr<APVTS::ButtonAttachment> ba;
    };

    Row& addSlider (const juce::String& id, const juce::String& label);
    Row& addCombo  (const juce::String& id, const juce::String& label, const juce::StringArray&);
    Row& addToggle (const juce::String& id, const juce::String& label);

    PedalSteelGlideProcessor& proc;
    std::vector<Row> rows;
    juce::Label title, blurb;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PedalSteelGlideEditor)
};
