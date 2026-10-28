#pragma once

#include <functional>
#include <PktMgr/PktMgr.hpp>

// Hook for the desktop simulator (KT_KickEM, compiled with SIM_BUILD=1).
// The simulator injects control-surface input via Controls::InputEvents::dispatchHardwareEvent
// and installs this handler to receive every packet the app would send to the control surface
// (pad colours, button LEDs, ...). The handler may be called from any thread.
struct SimulatorInterface
{
    using Handler = std::function<void( const CPktMgr::HWMsg_t& )>;

    static Handler& messageToHardware()
    {
        static Handler handler;
        return handler;
    }
};
