#include "MBStudioLookAndFeel.h"
#include "BinaryData.h"

const juce::Colour MBStudioLookAndFeel::panicRed { 0xffd8261c };
const juce::Colour MBStudioLookAndFeel::ledGreen { 0xff3ddc4a };

MBStudioLookAndFeel::MBStudioLookAndFeel()
    : juce::LookAndFeel_V4 (juce::LookAndFeel_V4::ColourScheme {
          0xff000000,   // windowBackground
          0xff0b0b0b,   // widgetBackground
          0xff151515,   // menuBackground
          0xff5a5a5a,   // outline
          0xffffffff,   // defaultText
          0xff1e1e1e,   // defaultFill
          0xff000000,   // highlightedText
          0xffffffff,   // highlightedFill
          0xffffffff }) // menuText
{
    bebas = juce::Typeface::createSystemTypefaceFor (BinaryData::BebasNeueRegular_ttf,
                                                     BinaryData::BebasNeueRegular_ttfSize);
    setColour (juce::Slider::rotarySliderFillColourId, juce::Colours::white);
    setColour (juce::Slider::thumbColourId, juce::Colours::white);
    setColour (juce::TextButton::buttonColourId, juce::Colour (0xff121212));
    setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xff8a1c14));
}

juce::Typeface::Ptr MBStudioLookAndFeel::getTypefaceForFont (const juce::Font& f)
{
    if (bebas != nullptr)
        return bebas;
    return juce::LookAndFeel_V4::getTypefaceForFont (f);
}

juce::Font MBStudioLookAndFeel::titleFont (float height)
{
    return juce::Font (juce::FontOptions (height));
}

void MBStudioLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                         float sliderPos, float startAngle, float endAngle,
                                         juce::Slider& slider)
{
    const auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (2.0f);
    const auto centre = bounds.getCentre();
    const float outer = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    if (outer < 8.0f)
        return;

    const float tickLen = juce::jmax (5.0f, outer * 0.20f);
    const float ringR   = outer - 1.0f;
    const float arcR    = ringR - tickLen - 4.0f;
    const float capR    = arcR - 5.0f;
    const float angle   = startAngle + sliderPos * (endAngle - startAngle);
    const bool  live    = slider.isEnabled();

    auto pointAt = [centre] (float a, float r)
    {
        return centre.translated (std::sin (a) * r, -std::cos (a) * r);
    };

    // Serigrafia: tacche ogni 5%, le maggiori ogni 25%
    for (int i = 0; i <= 20; ++i)
    {
        const float p = (float) i / 20.0f;
        const bool major = (i % 5) == 0;
        const float a = startAngle + p * (endAngle - startAngle);
        const float len = major ? tickLen : tickLen * 0.55f;

        g.setColour ((major ? juce::Colour (0xfff3f3f1) : juce::Colour (0xffb9bbbf))
                         .withAlpha (live ? (major ? 1.0f : 0.65f) : 0.28f));
        g.drawLine ({ pointAt (a, ringR), pointAt (a, ringR - len) }, major ? 1.6f : 1.0f);
    }

    // Incasso sotto il cappuccio
    const auto well = juce::Rectangle<float> (capR * 2.0f + 7.0f, capR * 2.0f + 7.0f).withCentre (centre);
    g.setColour (juce::Colour (0xff050506));
    g.fillEllipse (well);
    g.setColour (juce::Colour (0xff26282b));
    g.drawEllipse (well, 1.0f);

    // Arco del valore, dentro la scala
    juce::Path arc;
    arc.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, startAngle, angle, true);
    g.setColour (live ? juce::Colour (0xff3d8bff) : juce::Colour (0xff4a4c50));
    g.strokePath (arc, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Cappuccio zigrinato
    const auto cap = juce::Rectangle<float> (capR * 2.0f, capR * 2.0f).withCentre (centre);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff1e1f21), centre.x, cap.getY(),
                                             juce::Colour (0xff121314), centre.x, cap.getBottom(), false));
    g.fillEllipse (cap);
    g.setColour (juce::Colour (0xff34363a));
    g.drawEllipse (cap, 1.0f);

    for (int i = 0; i < 24; ++i)
    {
        const float a = juce::MathConstants<float>::twoPi * (float) i / 24.0f;
        g.setColour (juce::Colours::white.withAlpha (0.05f));
        g.drawLine ({ pointAt (a, capR - 1.0f), pointAt (a, capR * 0.82f) }, 1.0f);
    }

    // Linea dell'indicatore
    g.setColour (live ? juce::Colour (0xfff3f3f1) : juce::Colour (0xff6a6c70));
    g.drawLine ({ pointAt (angle, capR - 1.5f), pointAt (angle, capR * 0.40f) }, 2.2f);
}

//==============================================================================
MBStudioBrand::MBStudioBrand()
{
    const auto mask = juce::ImageCache::getFromMemory (BinaryData::mbstudio91_wordmark_png,
                                                       BinaryData::mbstudio91_wordmark_pngSize);
    printed = tint (mask, juce::Colour (0xf3f3f1).withAlpha (0.96f));
    light   = tint (mask, juce::Colours::white.withAlpha (0.13f));
    shadow  = tint (mask, juce::Colours::black.withAlpha (0.90f));
    body    = tint (mask, juce::Colour (0xff09090a));
}

juce::Image MBStudioBrand::tint (const juce::Image& mask, juce::Colour c)
{
    if (! mask.isValid())
        return {};

    juce::Image out (juce::Image::ARGB, mask.getWidth(), mask.getHeight(), true);
    const juce::Image::BitmapData in (mask, juce::Image::BitmapData::readOnly);
    juce::Image::BitmapData dst (out, juce::Image::BitmapData::writeOnly);

    for (int y = 0; y < in.height; ++y)
        for (int x = 0; x < in.width; ++x)
            dst.setPixelColour (x, y, c.withMultipliedAlpha (in.getPixelColour (x, y).getFloatAlpha()));

    return out;
}

int MBStudioBrand::widthFor (int height) const
{
    if (! printed.isValid() || printed.getHeight() == 0)
        return 0;
    return juce::roundToInt ((float) height * (float) printed.getWidth() / (float) printed.getHeight());
}

void MBStudioBrand::drawMark (juce::Graphics& g, const juce::Image& im,
                              juce::Rectangle<int> area, float opacity) const
{
    if (! im.isValid())
        return;
    g.setOpacity (opacity);
    g.drawImage (im, area.toFloat(), juce::RectanglePlacement::centred | juce::RectanglePlacement::onlyReduceInSize);
    g.setOpacity (1.0f);
}

void MBStudioBrand::paintPrinted (juce::Graphics& g, juce::Rectangle<int> area,
                                  const juce::String& product) const
{
    const int h = juce::jmin (area.getHeight(), 30);
    auto row = area.withSizeKeepingCentre (area.getWidth(), h);
    auto markArea = row.removeFromLeft (widthFor (h));
    drawMark (g, printed, markArea, 1.0f);

    if (product.isEmpty())
        return;

    row.removeFromLeft (10);
    const auto font = MBStudioLookAndFeel::titleFont ((float) h * 0.74f);
    const int w = juce::roundToInt (juce::GlyphArrangement::getStringWidth (font, product)) + 18;
    auto badge = row.removeFromLeft (juce::jmin (w, row.getWidth())).reduced (0, 1);

    g.setColour (juce::Colour (0xff34363a));
    g.drawRoundedRectangle (badge.toFloat(), 2.0f, 1.0f);
    g.setColour (juce::Colour (0xfff3f3f1));
    g.setFont (font);
    g.drawText (product, badge, juce::Justification::centred);
}

void MBStudioBrand::paintDebossed (juce::Graphics& g, juce::Rectangle<int> area) const
{
    const int h = juce::jmin (area.getHeight(), 18);
    const int w = widthFor (h);
    auto markArea = juce::Rectangle<int> (area.getRight() - w, area.getCentreY() - h / 2, w, h);

    drawMark (g, light,  markArea.translated (0, 1), 1.0f);
    drawMark (g, shadow, markArea.translated (0, -1), 1.0f);
    drawMark (g, body,   markArea, 0.55f);
}
