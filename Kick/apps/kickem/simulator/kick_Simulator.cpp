#include "kick_Simulator.h"

#include "input_events/input_events.h"
#include "input_events/kick_SimulatorInterface.h"
#include "contracts/dcl/dcl_contracts.h"

namespace sim
{
    using DCC = contracts::DeviceControlContracts;

    namespace
    {
        const juce::Colour panelColour{ 0xff2b2b2b };
        const juce::Colour sectionColour{ 0xff1d1d1d };
        const juce::Colour shiftTextColour{ 0xffe8c547 };
        const juce::Colour ledColour{ 0xffff8a00 };
        const juce::Colour ledShiftColour{ 0xffe8c547 };

        constexpr int numPads      = 16;
        constexpr int numPadColour = DCC::PadColourNeutralGrey + 1;

        bool isLitColour( int id ) { return id >= 0 && id < numPadColour && id != DCC::PadColourTrans; }

        // Direction convention of the hardware: data = { knob, direction, |steps|, 0 }.
        int clampSteps( int steps ) { return juce::jlimit( 1, 127, std::abs( steps ) ); }
    }

    //==============================================================================
    HwButton::HwButton( const juce::String& label, int id, const juce::String& shiftLabel, int shiftId )
        : juce::TextButton( label )
        , primaryId( id )
        , secondaryId( shiftId )
        , secondaryLabel( shiftLabel )
    {
        setWantsKeyboardFocus( false );
    }

    void HwButton::setLed( bool on, juce::Colour colour )
    {
        if( ledOn == on && ledColour == colour ) return;
        ledOn     = on;
        ledColour = colour;
        repaint();
    }

    void HwButton::buttonStateChanged()
    {
        const bool down = isDown();
        if( down == wasDown ) return;
        wasDown = down;
        if( onHwStateChange ) onHwStateChange( *this, down );
    }

    void HwButton::paintButton( juce::Graphics& g, bool isOver, bool isDown )
    {
        auto r = getLocalBounds().toFloat().reduced( 1.0f );
        g.setColour( isDown ? juce::Colour( 0xff5a5a5a ) : ( isOver ? juce::Colour( 0xff474747 ) : juce::Colour( 0xff3a3a3a ) ) );
        g.fillRoundedRectangle( r, 4.0f );
        if( ledOn )
        {
            g.setColour( ledColour.withAlpha( 0.35f ) );
            g.fillRoundedRectangle( r, 4.0f );
        }
        g.setColour( ledOn ? ledColour : juce::Colour( 0xff6a6a6a ) );
        g.drawRoundedRectangle( r, 4.0f, ledOn ? 2.0f : 1.0f );
        g.setColour( juce::Colours::white );
        g.setFont( juce::Font( 12.5f, juce::Font::bold ) );
        g.drawFittedText( getButtonText(), getLocalBounds().reduced( 4, 2 ), juce::Justification::centred, 2 );
    }

    //==============================================================================
    Encoder::Encoder( const juce::String& l )
        : label( l )
    {
        setRepaintsOnMouseActivity( true );
    }

    void Encoder::paint( juce::Graphics& g )
    {
        auto       area   = getLocalBounds().toFloat();
        auto       text   = area.removeFromBottom( 16.0f );
        const auto size   = juce::jmin( area.getWidth(), area.getHeight() ) - 4.0f;
        auto       circle = juce::Rectangle<float>( size, size ).withCentre( area.getCentre() );

        g.setColour( juce::Colour( 0xff111111 ) );
        g.fillEllipse( circle );
        g.setColour( isMouseOverOrDragging() ? juce::Colour( 0xff9a9a9a ) : juce::Colour( 0xff6a6a6a ) );
        g.drawEllipse( circle, 2.0f );

        const auto c      = circle.getCentre();
        const auto radius = size * 0.5f - 5.0f;
        g.setColour( juce::Colours::white );
        g.drawLine( c.x, c.y, c.x + radius * std::sin( angle ), c.y - radius * std::cos( angle ), 2.5f );

        g.setFont( juce::Font( 12.0f, juce::Font::bold ) );
        g.setColour( juce::Colours::lightgrey );
        g.drawText( label, text, juce::Justification::centred );
    }

    void Encoder::mouseDown( const juce::MouseEvent& e )
    {
        lastY   = e.y;
        dragged = false;
    }

    void Encoder::mouseDrag( const juce::MouseEvent& e )
    {
        constexpr int pixelsPerStep = 6;
        const int     steps         = ( lastY - e.y ) / pixelsPerStep;
        if( steps != 0 )
        {
            lastY -= steps * pixelsPerStep;
            dragged = true;
            turn( steps );
        }
    }

    void Encoder::mouseUp( const juce::MouseEvent& e )
    {
        if( !dragged && e.mouseWasClicked() && onPush ) onPush();
    }

    void Encoder::mouseWheelMove( const juce::MouseEvent&, const juce::MouseWheelDetails& wheel )
    {
        wheelAcc += ( wheel.isReversed ? -wheel.deltaY : wheel.deltaY ) * 20.0f;
        const int steps = (int)wheelAcc;
        if( steps != 0 )
        {
            wheelAcc -= (float)steps;
            turn( steps );
        }
    }

    void Encoder::turn( int steps )
    {
        angle += 0.15f * (float)steps;
        repaint();
        if( onTurn ) onTurn( steps );
    }

    //==============================================================================
    Pad::Pad( int i )
        : index( i )
    {
    }

    void Pad::setState( int newState, int newColourId, int newOffColourId )
    {
        state       = newState;
        colourId    = newColourId;
        offColourId = newOffColourId;
        repaint();
    }

    bool Pad::isBlinking() const { return state == DCC::StateOnBlink || state == DCC::StateOffBlink; }

    void Pad::setBlinkPhase( bool phase )
    {
        blinkPhase = phase;
        if( isBlinking() ) repaint();
    }

    void Pad::paint( juce::Graphics& g )
    {
        auto r = getLocalBounds().toFloat().reduced( 1.0f );
        g.setColour( juce::Colour( 0xff262626 ) );
        g.fillRoundedRectangle( r, 6.0f );

        int shownColour = colourId;
        if( isBlinking() && !blinkPhase ) shownColour = offColourId;

        if( state != DCC::StateOff && isLitColour( shownColour ) )
        {
            auto c = ui::Theme::padButtonColour( shownColour );
            if( state == DCC::StateSampleNotAssigned ) c = c.withAlpha( 0.3f );
            g.setColour( c );
            g.fillRoundedRectangle( r.reduced( 3.0f ), 5.0f );
        }

        g.setColour( pressed ? juce::Colours::white : juce::Colour( 0xff555555 ) );
        g.drawRoundedRectangle( r, 6.0f, pressed ? 3.0f : 1.0f );

        g.setColour( juce::Colours::white.withAlpha( 0.8f ) );
        g.setFont( juce::Font( 11.0f, juce::Font::bold ) );
        g.drawText( "PAD " + juce::String( index + 1 ), getLocalBounds().reduced( 7, 5 ), juce::Justification::bottomLeft );
    }

    void Pad::mouseDown( const juce::MouseEvent& )
    {
        pressed = true;
        repaint();
        if( onPad ) onPad( index, true );
    }

    void Pad::mouseUp( const juce::MouseEvent& )
    {
        pressed = false;
        repaint();
        if( onPad ) onPad( index, false );
    }

    //==============================================================================
    Simulator::Simulator()
    {
        // Left column: Q-link encoders, level/transport buttons and the pads.
        for( int i = 0; i < 16; ++i )
        {
            auto* e   = qlinks.add( new Encoder( "Q" + juce::String( i + 1 ) ) );
            e->onTurn = [this, i]( int steps ) { sendKnob( DCC::EN_KNOB_1 + i, DCC::EN_KNOB_QLINK, steps ); };
            addAndMakeVisible( e );
        }

        addButton( "LINK", DCC::EN_LINK );
        addButton( "FULL LEVEL", DCC::EN_FULL_LEVEL, "HALF LEVEL", DCC::EN_HALF_LEVEL );
        addButton( "16 LEVELS", DCC::EN_16LEVELS, "NOTES", DCC::EN_NOTES );
        addButton( "ERASE", DCC::EN_ERASE );

        addButton( "REC", DCC::EN_RECORD );
        addButton( "OVERDUB", DCC::EN_OVERDUB );
        addButton( "STOP", DCC::EN_STOP );
        addButton( "PLAY", DCC::EN_PLAY );
        addButton( "PLAY START", DCC::EN_PLAYSTART );

        for( int i = 0; i < numPads; ++i )
        {
            auto* p  = pads.add( new Pad( i ) );
            p->onPad = [this]( int pad, bool pressed ) { sendPad( pad, pressed ); };
            addAndMakeVisible( p );
        }

        // Below the screen: mode buttons, banks, navigation.
        addButton( "MAIN", DCC::EN_MAINMODE, "GRID", DCC::EN_GRID );
        addButton( "BROWSE", DCC::EN_BROWSE, "SAVE", DCC::EN_SAVE );
        addButton( "TRACK MIX", DCC::EN_TRACK_MIX, "PAD MIX", DCC::EN_PAD_MIX );
        addButton( "TRACK MUTE", DCC::EN_TRACK_MUTE, "PAD MUTE", DCC::EN_PAD_MUTE );
        addButton( "NEXT SEQ", DCC::EN_NEXT_SEQ, "FX", DCC::EN_FX );
        addButton( "TAP TEMPO", DCC::EN_TAP_TEMPO, "MASTER", DCC::EN_MASTER );
        addButton( "NOTE REPEAT", DCC::EN_NOTE_REPEAT, "LATCH", DCC::EN_LATCH );

        addButton( "STEP SEQ", DCC::EN_STEP_SEQ, "AUTOMATION", DCC::EN_AUTOMATION );
        addButton( "TC", DCC::EN_TC, "ON/OFF", DCC::EN_ON_OFF );
        addButton( "SAMPLER", DCC::EN_SAMPLER, "LOOPER", DCC::EN_LOOPER );
        addButton( "SAMPLE EDIT", DCC::EN_SAMPLE_EDIT, "LINK EDIT", DCC::EN_LINK_EDIT );
        addButton( "PROG EDIT", DCC::EN_PROG_EDIT, "MIDI CONTROL", DCC::EN_MIDI_CONTROL );
        addButton( "MENU", DCC::EN_MENU, "PREFS", DCC::EN_PREF );
        shiftButton = &addButton( "SHIFT", DCC::EN_SHIFT );

        addButton( "A", DCC::EN_BANK_A, "E", DCC::EN_BANK_E );
        addButton( "B", DCC::EN_BANK_B, "F", DCC::EN_BANK_F );
        addButton( "C", DCC::EN_BANK_C, "G", DCC::EN_BANK_G );
        addButton( "D", DCC::EN_BANK_D, "H", DCC::EN_BANK_H );

        addButton( "BACK", DCC::EN_BACK );
        addButton( "COPY", DCC::EN_COPY );
        addButton( "-", DCC::EN_MINUS );
        addButton( "+", DCC::EN_PLUS );

        centreKnob.onTurn = [this]( int steps ) { sendKnob( DCC::EN_KNOB_CENTER, DCC::EN_KNOB_ROTARY, steps ); };
        centreKnob.onPush = [this] {
            sendButton( DCC::EN_KNOB_ENTER, true );
            sendButton( DCC::EN_KNOB_ENTER, false );
        };
        addAndMakeVisible( centreKnob );

        // The real 1024x600 KICK screen.
        addAndMakeVisible( kick_ui::KickHome::Instance() );

        setSize( panelWidth, panelHeight ); // last: resized() lays out the controls created above

        SimulatorInterface::messageToHardware() = [safeThis = juce::Component::SafePointer<Simulator>( this )]( const CPktMgr::HWMsg_t& msg ) {
            juce::MessageManager::callAsync( [safeThis, msg] {
                if( safeThis != nullptr ) safeThis->handleMessageFromApp( msg );
            } );
        };

        startTimer( 350 );
    }

    Simulator::~Simulator()
    {
        SimulatorInterface::messageToHardware() = nullptr;
        removeChildComponent( &kick_ui::KickHome::Instance() );
    }

    HwButton& Simulator::addButton( const juce::String& label, int id, const juce::String& shiftLabel, int shiftId )
    {
        auto* b            = buttons.add( new HwButton( label, id, shiftLabel, shiftId ) );
        b->onHwStateChange = [this]( HwButton& button, bool pressed ) {
            if( &button == shiftButton )
            {
                if( pressed ) setShiftLatched( !shiftLatched );
                return;
            }
            if( pressed )
            {
                button.activeId = ( isShiftActive() && button.secondaryId >= 0 ) ? button.secondaryId : button.primaryId;
                sendButton( button.activeId, true );
            }
            else if( button.activeId >= 0 )
            {
                sendButton( button.activeId, false );
                // SHIFT latches for one shifted press (hold the keyboard Shift key for repeated use).
                if( shiftLatched && button.activeId == button.secondaryId ) setShiftLatched( false );
                button.activeId = -1;
            }
        };
        addAndMakeVisible( b );
        return *b;
    }

    bool Simulator::isShiftActive() const { return shiftLatched || juce::ModifierKeys::currentModifiers.isShiftDown(); }

    void Simulator::setShiftLatched( bool latched )
    {
        shiftLatched = latched;
        sendButton( DCC::EN_SHIFT, latched );
        shiftButton->setLed( latched, ledShiftColour );
        repaint();
    }

    //==============================================================================
    void Simulator::send( CPktMgr::HWMsg_t& msg )
    {
        msg.pktDir = CPktMgr::EN_PKT_REQUEST;
        Controls::InputEvents::dispatchHardwareEvent( msg );
    }

    void Simulator::sendButton( int id, bool pressed )
    {
        CPktMgr::HWMsg_t msg;
        msg.pktType = CPktMgr::EN_PKT_TYPE_BUTTON;
        msg.pktCmd  = DCC::EN_BTN_STATUS;
        msg.data    = { id, pressed ? DCC::BTN_PRESSED : DCC::BTN_RELEASED, 0, 0 };
        send( msg );
    }

    void Simulator::sendPad( int pad, bool pressed )
    {
        constexpr int velocity = 100;
        CPktMgr::HWMsg_t msg;
        msg.pktType = CPktMgr::EN_PKT_TYPE_PAD;
        msg.pktCmd  = DCC::EN_PAD_STATE;
        msg.data    = { DCC::EN_PAD1 + pad, pressed ? DCC::BTN_PRESSED : DCC::BTN_RELEASED, velocity, 0 };
        send( msg );
    }

    void Simulator::sendKnob( int knobId, int knobCmd, int steps )
    {
        CPktMgr::HWMsg_t msg;
        msg.pktType = CPktMgr::EN_PKT_TYPE_KNOBS;
        msg.pktCmd  = knobCmd;
        msg.data    = { knobId, steps > 0 ? DCC::EN_KNOB_CLOCKWISE : DCC::EN_KNOB_ANTICLOCKWISE, clampSteps( steps ), 0 };
        send( msg );
    }

    //==============================================================================
    void Simulator::handleMessageFromApp( const CPktMgr::HWMsg_t& msg )
    {
        const auto& d = msg.data;
        const int   n = (int)d.size();

        if( msg.pktType == CPktMgr::EN_PKT_TYPE_PAD )
        {
            switch( msg.pktCmd )
            {
                case DCC::EN_PAD_SET_SINGLE_PAD: // { pad, state, colour }
                case DCC::EN_PAD_SET_STEP_SINGLE_PAD: // { pad, state, colour, velocity }
                    if( n >= 3 ) setPad( d[0], d[1], d[2] );
                    break;
                case DCC::EN_PAD_SET_BLINKING: // { pad, state, onColour, offColour }
                    if( n >= 4 ) setPad( d[0], d[1], d[2], d[3] );
                    break;
                case DCC::EN_PAD_SET_ALL_SINGLE_COLOUR: // { state, colour }
                    if( n >= 2 )
                        for( int i = 0; i < numPads; ++i ) setPad( i, d[0], d[1] );
                    break;
                case DCC::EN_PAD_SET_ALL_MULTI_COLOUR: // { state, colour x16 }
                case DCC::EN_PAD_SET_STEP_ALL_PAD: // { state, colour x16, velocity x16 }
                    for( int i = 0; i < numPads && i + 1 < n; ++i ) setPad( i, d[0], d[i + 1] );
                    break;
                case DCC::EN_PAD_SET_COLOUR_STATE: // { colour x N, state x N }
                    for( int i = 0; i < n / 2 && i < numPads; ++i ) setPad( i, d[n / 2 + i], d[i] );
                    break;
                default: break; // settings, colour mode, aftertouch: nothing to show
            }
        }
        else if( msg.pktType == CPktMgr::EN_PKT_TYPE_LED )
        {
            if( ( msg.pktCmd == DCC::EN_LED_SET_SINGLE_LED || msg.pktCmd == DCC::EN_LED_BLINK_SINGLE ) && n >= 2 )
                setLed( d[0], d[1] != DCC::StateOff );
            else if( msg.pktCmd == DCC::EN_LED_SET_ALL && n >= 1 )
                for( int id = 0; id < DCC::SIZE; ++id ) setLed( id, d[0] != DCC::StateOff );
        }
    }

    void Simulator::setPad( int pad, int state, int colourId, int offColourId )
    {
        if( juce::isPositiveAndBelow( pad, numPads ) ) pads[pad]->setState( state, colourId, offColourId );
    }

    void Simulator::setLed( int id, bool on )
    {
        // Assumption about the control-surface firmware: bank LEDs and PLAY/STOP/PLAY START behave
        // as radio groups, so lighting one clears the others.
        auto clearOthers = [this]( int first, int last, int except ) {
            for( int other = first; other <= last; ++other )
                if( other != except ) setLed( other, false );
        };
        if( on && id >= DCC::EN_BANK_A && id <= DCC::EN_BANK_H ) clearOthers( DCC::EN_BANK_A, DCC::EN_BANK_H, id );
        if( on && ( id == DCC::EN_PLAY || id == DCC::EN_STOP || id == DCC::EN_PLAYSTART ) )
            for( int other : { (int)DCC::EN_PLAY, (int)DCC::EN_STOP, (int)DCC::EN_PLAYSTART } )
                if( other != id ) setLed( other, false );

        for( auto* b : buttons )
        {
            if( b == shiftButton ) continue;
            if( b->primaryId == id ) b->setLed( on, ledColour );
            else if( b->secondaryId == id ) b->setLed( on, ledShiftColour );
        }
    }

    void Simulator::timerCallback()
    {
        blinkPhase = !blinkPhase;
        for( auto* p : pads ) p->setBlinkPhase( blinkPhase );
    }

    //==============================================================================
    void Simulator::resized()
    {
        constexpr int margin = 20, gap = 10;

        screenArea = { 440, margin, ui::layout::screen.getWidth(), ui::layout::screen.getHeight() };
        kick_ui::KickHome::Instance().setBounds( screenArea );

        auto layoutRow = []( juce::Rectangle<int> row, int count, int itemGap, auto&& place ) {
            const int w = ( row.getWidth() - itemGap * ( count - 1 ) ) / count;
            for( int i = 0; i < count; ++i ) place( i, row.removeFromLeft( w ) ), row.removeFromLeft( itemGap );
        };

        // Q-links 4x4
        qlinkArea = { margin, margin, 400, 300 };
        for( int row = 0; row < 4; ++row )
            layoutRow( qlinkArea.withHeight( 75 ).withY( qlinkArea.getY() + row * 75 ), 4, 0, [&]( int col, juce::Rectangle<int> r ) { qlinks[row * 4 + col]->setBounds( r.reduced( 6, 2 ) ); } );

        // Button order follows the addButton() calls in the constructor.
        int next = 0;
        layoutRow( { margin, 330, 400, 40 }, 4, 8, [&]( int, juce::Rectangle<int> r ) { buttons[next++]->setBounds( r ); } );
        layoutRow( { margin, 396, 400, 40 }, 5, 8, [&]( int, juce::Rectangle<int> r ) { buttons[next++]->setBounds( r ); } );

        // Pads 4x4, PAD 1 bottom left (as on the hardware and the keyboard map).
        padArea = { margin, 456, 400, 424 };
        const int padW = ( padArea.getWidth() - 3 * gap ) / 4, padH = ( padArea.getHeight() - 3 * gap ) / 4;
        for( int i = 0; i < numPads; ++i )
        {
            const int row = 3 - i / 4, col = i % 4;
            pads[i]->setBounds( padArea.getX() + col * ( padW + gap ), padArea.getY() + row * ( padH + gap ), padW, padH );
        }

        modeArea = { screenArea.getX(), screenArea.getBottom() + 20, 700, 240 };
        for( int row = 0; row < 2; ++row )
            layoutRow( { modeArea.getX(), modeArea.getY() + row * 76, 700, 46 }, 7, 9, [&]( int, juce::Rectangle<int> r ) { buttons[next++]->setBounds( r ); } );
        layoutRow( { modeArea.getX(), modeArea.getY() + 152, 4 * 92 + 3 * 9, 46 }, 4, 9, [&]( int, juce::Rectangle<int> r ) { buttons[next++]->setBounds( r ); } );

        navArea = { modeArea.getRight() + 20, modeArea.getY(), screenArea.getRight() - modeArea.getRight() - 20, 240 };
        buttons[next++]->setBounds( navArea.getX(), navArea.getY(), 80, 40 ); // BACK
        buttons[next++]->setBounds( navArea.getRight() - 80, navArea.getY(), 80, 40 ); // COPY
        buttons[next++]->setBounds( navArea.getX(), navArea.getBottom() - 46, 80, 40 ); // -
        buttons[next++]->setBounds( navArea.getRight() - 80, navArea.getBottom() - 46, 80, 40 ); // +
        centreKnob.setBounds( juce::Rectangle<int>( 140, 180 ).withCentre( navArea.getCentre() ) );
    }

    void Simulator::paint( juce::Graphics& g )
    {
        g.fillAll( panelColour );

        g.setColour( sectionColour );
        g.fillRoundedRectangle( qlinkArea.expanded( 6 ).toFloat(), 6.0f );
        g.fillRoundedRectangle( padArea.expanded( 6 ).toFloat(), 6.0f );
        g.fillRoundedRectangle( navArea.expanded( 6 ).toFloat(), 6.0f );
        g.setColour( juce::Colours::black );
        g.fillRect( screenArea.expanded( 4 ) );

        // Secondary (SHIFT) functions printed under each button, highlighted while shift is active.
        g.setFont( juce::Font( 11.0f, juce::Font::bold ) );
        g.setColour( isShiftActive() ? shiftTextColour : shiftTextColour.withAlpha( 0.55f ) );
        for( auto* b : buttons )
            if( b->secondaryLabel.isNotEmpty() )
                g.drawText( b->secondaryLabel, b->getBounds().translated( 0, b->getHeight() + 1 ).withHeight( 14 ), juce::Justification::centred );

        g.setColour( juce::Colours::grey );
        g.drawText( "KT_KickEM - KICK simulator", getLocalBounds().removeFromBottom( 18 ).withTrimmedRight( 20 ), juce::Justification::centredRight );
    }
}
