/*
** B-Step is Free and Open Source Software
**
** B-Step is made available under the Gnu General Public License, v3.0
** https://www.gnu.org/licenses/gpl-3.0.en.html; The authors of the code
** reserve the right to re-license their contributions under the MIT license in the
** future at the discretion of the project maintainers.
**
** Copyright 2014-2022 by various individuals as described by the git transaction log
**
** All source at: https://github.com/surge-synthesizer/b-step.git
**
** B-Step was a commercial product from 2014-2022, with copyright and ownership
** in that period held by Thomas Arndt at Monoplugs. Thomas made B-Step
** open source in March 2022.
*/

#ifndef __JUCE_HEADER_BCB71CADC7231BAE__
#define __JUCE_HEADER_BCB71CADC7231BAE__

#include "App.h"
#include "UIEditorToolbar.h"

//==============================================================================
class UiNotificationAnimation : public juce::Component, public juce::Timer
{
  public:
    //==============================================================================
    UiNotificationAnimation();
    ~UiNotificationAnimation();

    //==============================================================================
  private:
    int alpha;
    bool is_rising;
    juce::Colour root_color;
    void timerCallback();

  public:
    void set_text_and_run(const char *text_, int state_ /* 0=fail,1=ok,2=neutral*/);
    void set_text_and_run(juce::String &text_, int state_ /* 0=fail,1=ok,2=neutral*/);

  private:
    void paint(juce::Graphics &g);
    void resized();

  private:
    //==============================================================================
    std::unique_ptr<juce::Label> text;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(UiNotificationAnimation)
};

#endif // __JUCE_HEADER_BCB71CADC7231BAE__
