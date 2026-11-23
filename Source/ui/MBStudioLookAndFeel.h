#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

// Fase 1: solo lo schema di colori MBStudio91 (telaio nero, scritte bianche) e il Bebas Neue.
// L'interfaccia hardware completa arriva in Fase 4.
class MBStudioLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    MBStudioLookAndFeel();

    static const juce::Colour panicRed;
    static const juce::Colour ledGreen;

    juce::Typeface::Ptr getTypefaceForFont (const juce::Font&) override;
    static juce::Font titleFont (float height);

    // Manopola come sul pannello: scala a tacche serigrafata, maggiori ogni 25%,
    // cappuccio nero con la linea bianca. Spenta quando il controllo e' bloccato.
    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height, float sliderPos,
                           float rotaryStartAngle, float rotaryEndAngle, juce::Slider&) override;

private:
    juce::Typeface::Ptr bebas;
};

// Il marchio come sulla plastica di un controller: stampato (bianco) in alto a
// sinistra col badge del prodotto, impresso (inciso nel nero) in basso a destra.
// Tiene le copie tinte della maschera, cosi' il disegno non le ricalcola ogni volta.
class MBStudioBrand
{
public:
    MBStudioBrand();

    void paintPrinted (juce::Graphics&, juce::Rectangle<int> area, const juce::String& product) const;
    void paintDebossed (juce::Graphics&, juce::Rectangle<int> area) const;

    int widthFor (int height) const;   // quanto spazio prende il logo a quell'altezza

private:
    static juce::Image tint (const juce::Image& mask, juce::Colour);
    void drawMark (juce::Graphics&, const juce::Image&, juce::Rectangle<int>, float opacity) const;

    juce::Image printed, light, shadow, body;
};
