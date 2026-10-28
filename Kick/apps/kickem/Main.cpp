/*
  ==============================================================================

    KT_KickEM - desktop simulator of the KICK control surface.
    Runs the full KICK app (same start-up as apps/kick/Main.cpp) inside a panel
    that replaces the serial control surface with on-screen pads, buttons and encoders.

  ==============================================================================
*/

#include "kick_ui/kick_ui.h"
#include "audio_engine/audio_engine.h"
#include "kick_device_adapter.h"
#include "kick_audio_engine_adapter.h"
#include "kick_audio_interface.h"
#include "simulator/kick_Simulator.h"

// Scales the fixed-size simulator panel down to fit smaller displays.
class ScaledPanel : public juce::Component
{
public:
    ScaledPanel()
    {
        addAndMakeVisible( panel );
        panel.setTopLeftPosition( 0, 0 );
    }

    void setScale( float scale )
    {
        panel.setTransform( juce::AffineTransform::scale( scale ) );
        setSize( juce::roundToInt( sim::Simulator::panelWidth * scale ), juce::roundToInt( sim::Simulator::panelHeight * scale ) );
    }

private:
    sim::Simulator panel;
};

//==============================================================================
class KickEmApplication : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override { return "KT_KickEM"; }
    const juce::String getApplicationVersion() override { return JUCE_APPLICATION_VERSION_STRING; }
    bool               moreThanOneInstanceAllowed() override { return false; }

    void initialise( const juce::String& /*commandLine*/ ) override
    {
#if JUCE_MAC
        // kick_paths.cpp resolves ./assets, ./demos, ... against the working directory; `open` starts
        // apps in "/", so fall back to the folder holding the bundle (the build directory).
        if( !juce::File::getCurrentWorkingDirectory().getChildFile( "assets" ).isDirectory() )
            juce::File::getSpecialLocation( juce::File::currentApplicationFile ).getParentDirectory().setAsCurrentWorkingDirectory();
#endif
        audioInterface.reset( new KickAudioInterface() );
        engineAdapter.reset( new KickAudioEngineAdapter( audioInterface ) );
        adapter.reset( new KickDeviceAdapter() );

        mt::getSettings();
        mt::getAudioInterface();

        w = std::make_unique<MainWindow>( getApplicationName() );

        // Desktop debugging aid: KICK_SNAPSHOT=<file.png> renders the window to a PNG after
        // KICK_SNAPSHOT_DELAY_MS (default 4000) and quits.
        if( auto path = juce::SystemStats::getEnvironmentVariable( "KICK_SNAPSHOT", {} ); path.isNotEmpty() )
        {
            const int delay = juce::SystemStats::getEnvironmentVariable( "KICK_SNAPSHOT_DELAY_MS", "4000" ).getIntValue();
            juce::Timer::callAfterDelay( delay, [this, path] {
                auto* content = w->getContentComponent();
                auto  image   = content->createComponentSnapshot( content->getLocalBounds() );
                juce::File file( path );
                file.deleteFile();
                juce::FileOutputStream out( file );
                juce::PNGImageFormat().writeImageToStream( image, out );
                out.flush();
                quit();
            } );
        }
    }

    void shutdown() override
    {
        w       = nullptr;
        adapter = nullptr;
    }

    void systemRequestedQuit() override
    {
        auto mgr = juce::ModalComponentManager::getInstance();
        if( mgr->cancelAllModalComponents() )
            juce::Timer::callAfterDelay( 100, [this] { quit(); } );
        else
            quit();
    }

    void anotherInstanceStarted( const juce::String& ) override {}

    class MainWindow : public juce::DocumentWindow
    {
    public:
        explicit MainWindow( const juce::String& name )
            : juce::DocumentWindow( name, juce::Colours::black, juce::DocumentWindow::closeButton | juce::DocumentWindow::minimiseButton )
        {
            setUsingNativeTitleBar( true );

            // KICK_SIM_SCALE overrides the automatic fit-to-screen scale.
            float scale = juce::SystemStats::getEnvironmentVariable( "KICK_SIM_SCALE", "0" ).getFloatValue();
            if( scale <= 0.0f )
            {
                auto area = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay()->userArea;
                scale     = juce::jmin( 1.0f,
                                    (float)( area.getWidth() - 20 ) / (float)sim::Simulator::panelWidth,
                                    (float)( area.getHeight() - 40 ) / (float)sim::Simulator::panelHeight );
            }
            panel.setScale( scale );

            setContentNonOwned( &panel, true );
            setResizable( false, false );
            centreWithSize( getWidth(), getHeight() );
            setVisible( true );
        }

        ~MainWindow() override { clearContentComponent(); }

        void closeButtonPressed() override { juce::JUCEApplication::getInstance()->systemRequestedQuit(); }

        void inputAttemptWhenModal() override
        {
            auto numModal = getNumCurrentlyModalComponents();
            if( auto p = getCurrentlyModalComponent( juce::jmax( 0, numModal - 1 ) ) )
                p->toFront( true );
        }

    private:
        ScaledPanel panel;
    };

private:
    std::unique_ptr<MainWindow>                w;
    std::unique_ptr<KickDeviceAdapter>         adapter;
    std::unique_ptr<KickAudioEngineAdapter>    engineAdapter;
    std::shared_ptr<contracts::AudioInterface> audioInterface;
};

START_JUCE_APPLICATION( KickEmApplication )
