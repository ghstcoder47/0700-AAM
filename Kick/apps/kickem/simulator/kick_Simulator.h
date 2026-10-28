#pragma once

#include <kick_ui/kick_ui.h>
#include "PktMgr/PktMgr.hpp"

// KT_KickEM: a desktop stand-in for the KICK control surface.
// Every control emits the same CPktMgr::HWMsg_t packets the hardware sends over the serial link,
// and pad colours / button LEDs are driven by the packets the app sends back.
namespace sim
{
    // A front-panel button. Sends BTN_PRESSED on mouse down and BTN_RELEASED on mouse up,
    // using the secondary (SHIFT) id when shift is active. Lights up when its LED is on.
    class HwButton : public juce::TextButton
    {
    public:
        HwButton( const juce::String& label, int id, const juce::String& shiftLabel = {}, int shiftId = -1 );

        int          primaryId;
        int          secondaryId;
        juce::String secondaryLabel;
        int          activeId = -1; // id sent on press, reused for the release

        std::function<void( HwButton&, bool pressed )> onHwStateChange;

        void setLed( bool on, juce::Colour colour );

    private:
        void buttonStateChanged() override;
        void paintButton( juce::Graphics&, bool, bool ) override;

        bool         wasDown = false;
        bool         ledOn   = false;
        juce::Colour ledColour;
    };

    // Endless rotary encoder: drag vertically or use the mouse wheel. A click without dragging
    // reports a push (used for the centre knob's ENTER).
    class Encoder : public juce::Component
    {
    public:
        explicit Encoder( const juce::String& label );

        std::function<void( int steps )> onTurn;
        std::function<void()>            onPush;

    private:
        void paint( juce::Graphics& ) override;
        void mouseDown( const juce::MouseEvent& ) override;
        void mouseDrag( const juce::MouseEvent& ) override;
        void mouseUp( const juce::MouseEvent& ) override;
        void mouseWheelMove( const juce::MouseEvent&, const juce::MouseWheelDetails& ) override;
        void turn( int steps );

        juce::String label;
        float        angle    = 0.0f;
        int          lastY    = 0;
        bool         dragged  = false;
        float        wheelAcc = 0.0f;
    };

    class Pad : public juce::Component
    {
    public:
        explicit Pad( int index );

        std::function<void( int pad, bool pressed )> onPad;

        void setState( int newState, int colourId, int offColourId = -1 );
        void setBlinkPhase( bool phase );
        bool isBlinking() const;

    private:
        void paint( juce::Graphics& ) override;
        void mouseDown( const juce::MouseEvent& ) override;
        void mouseUp( const juce::MouseEvent& ) override;

        int  index;
        int  state       = 0;
        int  colourId    = -1;
        int  offColourId = -1;
        bool pressed     = false;
        bool blinkPhase  = true;
    };

    class Simulator : public juce::Component
        , private juce::Timer
    {
    public:
        Simulator();
        ~Simulator() override;

        static constexpr int panelWidth  = 1484;
        static constexpr int panelHeight = 900;

    private:
        void paint( juce::Graphics& ) override;
        void resized() override;
        void timerCallback() override;

        void sendButton( int id, bool pressed );
        void sendPad( int pad, bool pressed );
        void sendKnob( int knobId, int knobCmd, int steps );
        static void send( CPktMgr::HWMsg_t& msg );

        void handleMessageFromApp( const CPktMgr::HWMsg_t& msg );
        void setPad( int pad, int state, int colourId, int offColourId = -1 );
        void setLed( int id, bool on );

        bool isShiftActive() const;
        void setShiftLatched( bool latched );

        HwButton& addButton( const juce::String& label, int id, const juce::String& shiftLabel = {}, int shiftId = -1 );

        juce::OwnedArray<HwButton> buttons;
        juce::OwnedArray<Pad>      pads;
        juce::OwnedArray<Encoder>  qlinks;
        Encoder                    centreKnob{ "ENTER" };
        HwButton*                  shiftButton = nullptr;
        bool                       shiftLatched = false;
        bool                       blinkPhase   = false;

        juce::Rectangle<int> screenArea, qlinkArea, padArea, modeArea, navArea;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR( Simulator )
    };
}
