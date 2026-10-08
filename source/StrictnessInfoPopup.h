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
#include "PluginTests.h"
#include "PluginvalLookAndFeel.h"

//==============================================================================
/**
    A component that displays an AttributedString with word wrapping.
*/
class AttributedStringComponent : public juce::Component
{
public:
    void setText (const juce::AttributedString& newText, int maxWidth)
    {
        text = newText;
        textLayout.createLayout (text, (float) maxWidth);
        setSize (maxWidth, (int) std::ceil (textLayout.getHeight()));
    }

    void paint (juce::Graphics& g) override
    {
        textLayout.draw (g, getLocalBounds().toFloat());
    }

private:
    juce::AttributedString text;
    juce::TextLayout textLayout;
};

//==============================================================================
/**
    A popup window with a vertical strictness slider and explanation panel.
*/
class StrictnessInfoPopup : public juce::Component
{
public:
    StrictnessInfoPopup (int currentLevel, std::function<void(int)> onLevelChanged)
        : levelChangedCallback (std::move (onLevelChanged))
    {
        addAndMakeVisible (slider);
        addAndMakeVisible (contentView);
        addAndMakeVisible (okButton);

        slider.setSliderStyle (juce::Slider::LinearVertical);
        slider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
        slider.setRange (1.0, 10.0, 1.0);
        slider.setValue (currentLevel, juce::dontSendNotification);
        slider.onValueChange = [this]
        {
            updateContent();
            if (levelChangedCallback)
                levelChangedCallback (static_cast<int> (slider.getValue()));
        };

        contentView.setViewedComponent (&contentComponent, false);
        contentView.setScrollBarsShown (true, false);

        okButton.setButtonText ("OK");
        okButton.setColour (juce::TextButton::buttonColourId, PluginvalLookAndFeel::getAccentColour());
        okButton.setColour (juce::TextButton::textColourOffId, juce::Colours::white);
        okButton.onClick = [this]
        {
            if (auto* dw = findParentComponentOfClass<juce::DialogWindow>())
                dw->closeButtonPressed();
        };

        updateContent();

        setSize (754, 650);
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (findColour (juce::ResizableWindow::backgroundColourId));

        // Draw title
        g.setColour (findColour (juce::Label::textColourId));
        g.setFont (juce::Font (juce::FontOptions (24.0f, juce::Font::bold)));
        g.drawText ("Set Strictness Level", titleBounds, juce::Justification::centredLeft);

        // Draw separator line (below title, above OK button)
        auto separatorX = 70;
        auto topY = titleBounds.getBottom() + 10;
        auto bottomY = okButton.getY() - 10;
        g.setColour (findColour (juce::Label::textColourId).withAlpha (0.2f));
        g.drawVerticalLine (separatorX, static_cast<float> (topY), static_cast<float> (bottomY));

        // Draw tick marks for slider
        auto sliderBounds = slider.getBounds();
        auto trackHeight = sliderBounds.getHeight() - 30;
        auto trackTop = sliderBounds.getY() + 15;

        g.setFont (juce::Font (juce::FontOptions (15.0f)));

        for (int i = 1; i <= 10; ++i)
        {
            float y = static_cast<float> (trackTop) + static_cast<float> (trackHeight) * (1.0f - static_cast<float> (i - 1) / 9.0f);
            auto tickX = sliderBounds.getX() - 20;

            bool isSelected = i <= static_cast<int> (slider.getValue());
            g.setColour (findColour (juce::Label::textColourId).withAlpha (isSelected ? 1.0f : 0.4f));
            g.drawText (juce::String (i), tickX, static_cast<int> (y) - 6, 18, 12, juce::Justification::centredRight);
        }
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced (20);

        // Title at top
        titleBounds = area.removeFromTop (32);
        area.removeFromTop (10);

        // OK button at bottom
        auto bottomArea = area.removeFromBottom (36);
        okButton.setBounds (bottomArea.removeFromRight (100).reduced (0, 4));
        area.removeFromBottom (10);

        // Slider on left
        auto leftPanel = area.removeFromLeft (50);
        slider.setBounds (leftPanel.reduced (5, 0));

        area.removeFromLeft (20); // Gap after separator

        contentView.setBounds (area);
        updateContent();
    }

private:
    juce::Rectangle<int> titleBounds;
    juce::Slider slider;
    juce::Viewport contentView;
    juce::Component contentComponent;
    juce::TextButton okButton;
    std::function<void(int)> levelChangedCallback;
    juce::OwnedArray<juce::Label> headerLabels;
    juce::OwnedArray<AttributedStringComponent> textComponents;

    void updateContent()
    {
        int level = static_cast<int> (slider.getValue());

        contentComponent.removeAllChildren();
        headerLabels.clear();
        textComponents.clear();

        int yPos = 0;
        const int sectionGap = 20;
        const int itemGap = 12;
        const int width = contentView.getWidth() - 20;

        auto textColour = findColour (juce::Label::textColourId);

        // Header
        auto header = std::make_unique<juce::Label>();
        header->setText ("Tests at Level " + juce::String (level), juce::dontSendNotification);
        header->setFont (juce::Font (juce::FontOptions (20.0f, juce::Font::bold)));
        header->setColour (juce::Label::textColourId, textColour);
        header->setBounds (0, yPos, width, 28);
        contentComponent.addAndMakeVisible (header.get());
        headerLabels.add (header.release());
        yPos += 36;

        // Get all tests and sort by strictness level
        auto& allTests = PluginTest::getAllTests();

        std::vector<PluginTest*> sortedTests;
        for (auto* test : allTests)
            sortedTests.push_back (test);

        std::sort (sortedTests.begin(), sortedTests.end(),
                   [] (const PluginTest* a, const PluginTest* b)
                   { return a->strictnessLevel < b->strictnessLevel; });

        // Display only included tests
        for (auto* test : sortedTests)
        {
            if (test->strictnessLevel > level)
                continue;

            auto descriptions = test->getDescription (level);

            for (const auto& desc : descriptions)
            {
                juce::AttributedString attrStr;

                // Bold title
                attrStr.append (desc.title + ": ",
                                juce::Font (juce::FontOptions (15.0f, juce::Font::bold)),
                                textColour);

                // Normal description
                if (desc.description.isNotEmpty())
                {
                    attrStr.append (desc.description,
                                    juce::Font (juce::FontOptions (15.0f)),
                                    textColour.withAlpha (0.85f));
                }

                attrStr.setWordWrap (juce::AttributedString::WordWrap::byWord);

                auto comp = std::make_unique<AttributedStringComponent>();
                comp->setText (attrStr, width);
                comp->setTopLeftPosition (0, yPos);
                contentComponent.addAndMakeVisible (comp.get());
                yPos += comp->getHeight() + itemGap;
                textComponents.add (comp.release());
            }
        }

        yPos += sectionGap;

        contentComponent.setSize (width, yPos);
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StrictnessInfoPopup)
};

//==============================================================================
/**
    A dialog window for the strictness popup.
    Caller owns this via unique_ptr and should clear it when onClose is called.
*/
class StrictnessInfoDialog : public juce::DialogWindow
{
public:
    StrictnessInfoDialog (int currentLevel,
                          std::function<void(int)> onLevelChanged,
                          std::function<void()> onClose)
        : juce::DialogWindow ("", juce::Colours::black, true, true),
          closeCallback (std::move (onClose))
    {
        auto content = std::make_unique<StrictnessInfoPopup> (currentLevel, std::move (onLevelChanged));
        setBackgroundColour (content->findColour (juce::ResizableWindow::backgroundColourId));
        setContentOwned (content.release(), true);
        setUsingNativeTitleBar (true);
        setResizable (false, false);
        centreWithSize (getWidth(), getHeight());
        setVisible (true);
    }

    void closeButtonPressed() override
    {
        setVisible (false);
        if (closeCallback)
            closeCallback();
    }

private:
    std::function<void()> closeCallback;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StrictnessInfoDialog)
};
