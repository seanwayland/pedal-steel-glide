#include "PluginEditor.h"

namespace { const juce::Colour kBg { 0xff23201d }, kText { 0xffe9e2d4 }, kAccent { 0xffd9a441 }; }

PedalSteelGlideEditor::Row& PedalSteelGlideEditor::addSlider (const juce::String& id, const juce::String& label)
{
    Row r;
    auto s = std::make_unique<juce::Slider> (juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight);
    s->setTextBoxStyle (juce::Slider::TextBoxRight, false, 68, 20);
    s->setColour (juce::Slider::trackColourId, kAccent);
    s->setColour (juce::Slider::thumbColourId, kAccent);
    addAndMakeVisible (*s);
    r.sa = std::make_unique<APVTS::SliderAttachment> (proc.apvts, id, *s);
    r.control = std::move (s);
    r.label = label;
    rows.push_back (std::move (r));
    return rows.back();
}

PedalSteelGlideEditor::Row& PedalSteelGlideEditor::addCombo (const juce::String& id, const juce::String& label,
                                                            const juce::StringArray& items)
{
    Row r;
    auto c = std::make_unique<juce::ComboBox>();
    c->addItemList (items, 1);
    addAndMakeVisible (*c);
    r.ca = std::make_unique<APVTS::ComboBoxAttachment> (proc.apvts, id, *c);
    r.control = std::move (c);
    r.label = label;
    rows.push_back (std::move (r));
    return rows.back();
}

PedalSteelGlideEditor::Row& PedalSteelGlideEditor::addToggle (const juce::String& id, const juce::String& label)
{
    Row r;
    auto b = std::make_unique<juce::ToggleButton>();
    b->setColour (juce::ToggleButton::tickColourId, kAccent);
    addAndMakeVisible (*b);
    r.ba = std::make_unique<APVTS::ButtonAttachment> (proc.apvts, id, *b);
    r.control = std::move (b);
    r.label = label;
    rows.push_back (std::move (r));
    return rows.back();
}

PedalSteelGlideEditor::PedalSteelGlideEditor (PedalSteelGlideProcessor& p)
    : juce::AudioProcessorEditor (&p), proc (p)
{
    title.setText ("PEDAL STEEL GLIDE", juce::dontSendNotification);
    title.setFont (juce::FontOptions (18.0f, juce::Font::bold));
    title.setColour (juce::Label::textColourId, kAccent);
    addAndMakeVisible (title);

    blurb.setText ("MIDI FX - insert before an instrument. Set the instrument to MPE / MIDI Mono "
                   "mode (base channel = Master Channel) and its Pitch Bend range = Bend Range, "
                   "or use Polyphony = 1 for a single-channel mode that needs no MPE.",
                   juce::dontSendNotification);
    blurb.setJustificationType (juce::Justification::topLeft);
    blurb.setFont (juce::FontOptions (11.0f));
    blurb.setColour (juce::Label::textColourId, kText.withAlpha (0.7f));
    addAndMakeVisible (blurb);

    addSlider ("poly",       "Polyphony (1 = mono)");
    addSlider ("legato",     "Legato Interval");
    addSlider ("glideMin",   "Glide Fast (hard vel)");
    addSlider ("glideMax",   "Glide Slow (soft vel)");
    addSlider ("curve",      "Glide Curve");
    addSlider ("bendRange",  "Bend Range");
    addSlider ("wheelRange", "Wheel Range");
    addCombo  ("vibSource",  "Vibrato Source", { "Aftertouch", "Mod Wheel (CC1)", "Breath (CC2)" });
    addSlider ("vibRate",    "Vibrato Rate");
    addSlider ("vibDepth",   "Vibrato Depth");
    addSlider ("vibRandom",  "Vibrato Randomness");
    addSlider ("master",     "Master Channel");
    addToggle ("autoRange",  "Auto-Set Bend Range (RPN)");
    addToggle ("cc7",        "Pass CC7 (Volume)");

    for (auto& r : rows)
    {
        r.caption = std::make_unique<juce::Label>();
        r.caption->setText (r.label, juce::dontSendNotification);
        r.caption->setFont (juce::FontOptions (12.0f));
        r.caption->setColour (juce::Label::textColourId, kText);
        r.caption->setJustificationType (juce::Justification::centredRight);
        addAndMakeVisible (*r.caption);
    }

    setSize (440, 96 + (int) rows.size() * 30 + 16);
}

void PedalSteelGlideEditor::paint (juce::Graphics& g) { g.fillAll (kBg); }

void PedalSteelGlideEditor::resized()
{
    auto b = getLocalBounds().reduced (14);
    title.setBounds (b.removeFromTop (24));
    blurb.setBounds (b.removeFromTop (52));
    b.removeFromTop (6);

    const int labelW = 150;
    for (auto& r : rows)
    {
        auto row = b.removeFromTop (26);
        r.caption->setBounds (row.removeFromLeft (labelW));
        row.removeFromLeft (8);
        r.control->setBounds (row);
        b.removeFromTop (4);
    }
}
