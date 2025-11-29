/*==============================================================================

  Copyright 2018 by Tracktion Corporation.
  For more information visit www.tracktion.com

   You may also use this code under the terms of the GPL v3 (see
   www.gnu.org/licenses).

   pluginval IS PROVIDED "AS IS" WITHOUT ANY WARRANTY, AND ALL WARRANTIES, WHETHER
   EXPRESSED OR IMPLIED, INCLUDING MERCHANTABILITY AND FITNESS FOR PURPOSE, ARE
   DISCLAIMED.

 ==============================================================================*/

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

//==============================================================================
/**
    A minimal monochrome LookAndFeel for pluginval.
*/
class PluginvalLookAndFeel : public juce::LookAndFeel_V4
{
public:
    // Custom colour IDs
    enum ColourIds
    {
        accentColourId = 0x2000001
    };

    // Static accessor for accent colour
    static juce::Colour getAccentColour()
    {
        return juce::Colour (0xff4a9eff);  // Nice blue accent
    }

    PluginvalLookAndFeel()
    {
        // Define our monochrome palette
        const auto backgroundDark   = juce::Colour (0xff1a1a1a);
        const auto backgroundMid    = juce::Colour (0xff2d2d2d);
        const auto backgroundLight  = juce::Colour (0xff3d3d3d);
        const auto textColour       = juce::Colour (0xffe0e0e0);
        const auto textDimmed       = juce::Colour (0xff909090);
        const auto accentColour     = juce::Colour (0xffffffff);
        const auto highlightColour  = getAccentColour().withAlpha (0.3f);

        // Window colours
        setColour (juce::ResizableWindow::backgroundColourId, backgroundDark);
        setColour (juce::DocumentWindow::textColourId, textColour);

        // Button colours
        setColour (juce::TextButton::buttonColourId, backgroundMid);
        setColour (juce::TextButton::buttonOnColourId, backgroundLight);
        setColour (juce::TextButton::textColourOffId, textColour);
        setColour (juce::TextButton::textColourOnId, accentColour);

        // Slider colours
        setColour (juce::Slider::backgroundColourId, backgroundDark);
        setColour (juce::Slider::trackColourId, backgroundLight);
        setColour (juce::Slider::thumbColourId, getAccentColour());
        setColour (juce::Slider::textBoxTextColourId, textColour);
        setColour (juce::Slider::textBoxBackgroundColourId, backgroundMid);
        setColour (juce::Slider::textBoxOutlineColourId, backgroundLight);

        // Label colours
        setColour (juce::Label::textColourId, textColour);
        setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);

        // Text editor colours
        setColour (juce::TextEditor::backgroundColourId, backgroundMid);
        setColour (juce::TextEditor::textColourId, textColour);
        setColour (juce::TextEditor::outlineColourId, backgroundLight);
        setColour (juce::TextEditor::focusedOutlineColourId, textDimmed);

        // Code editor colours
        setColour (juce::CodeEditorComponent::backgroundColourId, backgroundDark);
        setColour (juce::CodeEditorComponent::defaultTextColourId, textColour);
        setColour (juce::CodeEditorComponent::lineNumberBackgroundId, backgroundMid);
        setColour (juce::CodeEditorComponent::lineNumberTextId, textDimmed);

        // List box colours
        setColour (juce::ListBox::backgroundColourId, backgroundDark);
        setColour (juce::ListBox::textColourId, textColour);
        setColour (juce::ListBox::outlineColourId, backgroundLight);

        // Table header colours
        setColour (juce::TableHeaderComponent::backgroundColourId, backgroundMid);
        setColour (juce::TableHeaderComponent::textColourId, textColour);
        setColour (juce::TableHeaderComponent::outlineColourId, backgroundLight);

        // Tabbed component colours
        setColour (juce::TabbedButtonBar::tabOutlineColourId, juce::Colours::transparentBlack);
        setColour (juce::TabbedButtonBar::frontOutlineColourId, juce::Colours::transparentBlack);
        setColour (juce::TabbedComponent::backgroundColourId, backgroundDark);
        setColour (juce::TabbedComponent::outlineColourId, juce::Colours::transparentBlack);

        // Popup menu colours
        setColour (juce::PopupMenu::backgroundColourId, backgroundMid);
        setColour (juce::PopupMenu::textColourId, textColour);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, getAccentColour());
        setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);

        // Directory/file list colours (used by PluginListComponent)
        setColour (juce::DirectoryContentsDisplayComponent::highlightColourId, getAccentColour());
        setColour (juce::DirectoryContentsDisplayComponent::textColourId, textColour);
        setColour (juce::DirectoryContentsDisplayComponent::highlightedTextColourId, juce::Colours::white);

        // Alert window colours
        setColour (juce::AlertWindow::backgroundColourId, backgroundMid);
        setColour (juce::AlertWindow::textColourId, textColour);
        setColour (juce::AlertWindow::outlineColourId, backgroundLight);

        // Scrollbar colours
        setColour (juce::ScrollBar::thumbColourId, backgroundLight);
        setColour (juce::ScrollBar::trackColourId, backgroundDark);

        // Caret
        setColour (juce::CaretComponent::caretColourId, accentColour);
    }

    //==============================================================================
    void drawButtonBackground (juce::Graphics& g, juce::Button& button,
                               const juce::Colour& backgroundColour,
                               bool shouldDrawButtonAsHighlighted,
                               bool shouldDrawButtonAsDown) override
    {
        auto bounds = button.getLocalBounds().toFloat().reduced (0.5f);
        auto cornerSize = 4.0f;

        auto baseColour = backgroundColour;

        if (shouldDrawButtonAsDown)
            baseColour = baseColour.brighter (0.1f);
        else if (shouldDrawButtonAsHighlighted)
            baseColour = baseColour.brighter (0.05f);

        g.setColour (baseColour);
        g.fillRoundedRectangle (bounds, cornerSize);

        g.setColour (baseColour.brighter (0.1f));
        g.drawRoundedRectangle (bounds, cornerSize, 1.0f);
    }

    void drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
                           float sliderPos, float /*minSliderPos*/, float /*maxSliderPos*/,
                           juce::Slider::SliderStyle style, juce::Slider& slider) override
    {
        auto trackWidth = juce::jmin (6.0f, (float) (style == juce::Slider::LinearVertical ? width : height) * 0.25f);

        juce::Point<float> startPoint ((float) x + (float) width * 0.5f, (float) (height + y));
        juce::Point<float> endPoint (startPoint.x, (float) y);

        if (style == juce::Slider::LinearHorizontal)
        {
            startPoint = { (float) x, (float) y + (float) height * 0.5f };
            endPoint = { (float) (width + x), startPoint.y };
        }

        juce::Path backgroundTrack;
        backgroundTrack.startNewSubPath (startPoint);
        backgroundTrack.lineTo (endPoint);
        g.setColour (slider.findColour (juce::Slider::backgroundColourId));
        g.strokePath (backgroundTrack, { trackWidth, juce::PathStrokeType::curved, juce::PathStrokeType::rounded });

        juce::Path valueTrack;
        juce::Point<float> thumbPoint;

        if (style == juce::Slider::LinearVertical)
        {
            auto kx = (float) x + (float) width * 0.5f;
            thumbPoint = { kx, sliderPos };
            valueTrack.startNewSubPath ({ kx, (float) (height + y) });
            valueTrack.lineTo (thumbPoint);
        }
        else
        {
            auto ky = (float) y + (float) height * 0.5f;
            thumbPoint = { sliderPos, ky };
            valueTrack.startNewSubPath ({ (float) x, ky });
            valueTrack.lineTo (thumbPoint);
        }

        g.setColour (slider.findColour (juce::Slider::trackColourId));
        g.strokePath (valueTrack, { trackWidth, juce::PathStrokeType::curved, juce::PathStrokeType::rounded });

        auto thumbWidth = 16.0f;
        g.setColour (slider.findColour (juce::Slider::thumbColourId));
        g.fillEllipse (juce::Rectangle<float> (thumbWidth, thumbWidth).withCentre (thumbPoint));

        // Add a subtle border to the thumb
        g.setColour (slider.findColour (juce::Slider::thumbColourId).darker (0.3f));
        g.drawEllipse (juce::Rectangle<float> (thumbWidth, thumbWidth).withCentre (thumbPoint), 1.0f);
    }

    void drawTabButton (juce::TabBarButton& button, juce::Graphics& g, bool isMouseOver, bool isMouseDown) override
    {
        auto area = button.getActiveArea().toFloat();
        auto backgroundColour = findColour (juce::ResizableWindow::backgroundColourId);

        if (isMouseOver && ! button.isFrontTab())
            backgroundColour = backgroundColour.brighter (0.05f);

        g.setColour (backgroundColour);
        g.fillRect (area);

        // Draw coloured underline for selected tab
        if (button.isFrontTab())
        {
            g.setColour (getAccentColour());
            g.fillRect (area.removeFromBottom (2.0f));
        }

        auto textColour = button.isFrontTab() ? findColour (juce::Label::textColourId)
                                               : findColour (juce::Label::textColourId).withAlpha (0.6f);
        g.setColour (textColour);
        g.setFont (juce::Font (14.0f));
        g.drawText (button.getButtonText(), area.reduced (12.0f, 0.0f), juce::Justification::centred);
    }

    int getTabButtonBestWidth (juce::TabBarButton& button, int tabDepth) override
    {
        auto width = juce::Font (14.0f).getStringWidth (button.getButtonText()) + 40;  // Extra padding
        return juce::jmax (width, 80);
    }

    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override
    {
        return juce::Font (juce::jmin (14.0f, (float) buttonHeight * 0.6f));
    }

    juce::Font getLabelFont (juce::Label&) override
    {
        return juce::Font (14.0f);
    }

    void drawTableHeaderColumn (juce::Graphics& g, juce::TableHeaderComponent& header,
                                const juce::String& columnName, int columnId,
                                int width, int height, bool isMouseOver, bool isMouseDown,
                                int columnFlags) override
    {
        auto highlightColour = header.findColour (juce::TableHeaderComponent::highlightColourId);

        if (isMouseDown)
            g.fillAll (highlightColour);
        else if (isMouseOver)
            g.fillAll (highlightColour.withMultipliedAlpha (0.625f));

        juce::Rectangle<int> area (width, height);
        area.reduce (4, 0);

        g.setColour (header.findColour (juce::TableHeaderComponent::textColourId));
        g.setFont (juce::Font (juce::FontOptions (17.0f)));  // 14 + 3 = 17

        if ((columnFlags & (juce::TableHeaderComponent::sortedForwards | juce::TableHeaderComponent::sortedBackwards)) != 0)
        {
            juce::Path sortArrow;
            sortArrow.addTriangle (0.0f, 0.0f, 0.5f, (columnFlags & juce::TableHeaderComponent::sortedForwards) != 0 ? -0.8f : 0.8f, 1.0f, 0.0f);
            g.fillPath (sortArrow, sortArrow.getTransformToScaleToFit (area.removeFromRight (height / 2).reduced (2).toFloat(), true));
        }

        g.drawFittedText (columnName, area, juce::Justification::centredLeft, 1);
    }

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginvalLookAndFeel)
};