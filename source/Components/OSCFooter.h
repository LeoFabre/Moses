#pragma once

#ifdef JUCE_OSC_H_INCLUDED
    #include "../OSC/OSCStatus.h"
#endif

class Footer : public juce::Component
{
public:
    Footer() : juce::Component() {}

    void paint (juce::Graphics& g) override
    {
        juce::Rectangle<int> bounds = getLocalBounds();
        g.setColour (juce::Colours::white.withAlpha (0.5f));
        g.setFont (getLookAndFeel().getTypefaceForFont (juce::Font (12.0f, 0)));
        g.setFont (14.0f);
        juce::String versionString = "v";

#if JUCE_DEBUG
        versionString = "DEBUG - v";
#endif
        versionString.append (JucePlugin_VersionString, 6);

        g.drawText (versionString,
                    0,
                    0,
                    bounds.getWidth() - 8,
                    bounds.getHeight() - 2,
                    juce::Justification::bottomRight);
    }

};

#ifdef JUCE_OSC_H_INCLUDED
class OSCFooter : public juce::Component
{
public:
    OSCFooter (OSCParameterInterface& oscInterface) : oscStatus (oscInterface)
    {
        addAndMakeVisible (footer);
        addAndMakeVisible (oscStatus);
    }

    void resized() override
    {
        auto bounds = getLocalBounds();
        footer.setBounds (bounds);

        bounds.removeFromBottom (2);
        bounds = bounds.removeFromBottom (16);
        // bounds.removeFromLeft (50);
        oscStatus.setBounds (bounds);
    }

private:
    OSCStatus oscStatus;
    Footer footer;
};
#endif