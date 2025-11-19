#include "carb/events/EventsUtils.h"
#include "carb/events/IEvents.h"
#include "omni/graph/action/IActionGraph.h"
#include "omni/timeline/ITimeline.h"
#include "omni/timeline/TimelineTypes.h"
#include "OgnSimulationDelayDatabase.h"
#include <carb/events/EventsTypes.h>
#include <carb/InterfaceUtils.h>
#include <carb/IObject.h>
#include <omni/graph/core/Handle.h>
#include <omni/graph/core/iComputeGraph.h>
#include <isaacsim/core/simulation_manager/ISimulationManager.h>

namespace kvantron
{
namespace simulation
{
namespace nodes
{
class OgnSimulationDelay
{
    double startSimulationTime{ 0.0 };
    bool isStarted{ false };
    carb::ObjectPtr<carb::events::ISubscription> timelineStopSubscription;

public:
    // Subscribe to Timeline Stop Event to reset the state on stop.
    static void initInstance(const NodeObj& node, GraphInstanceID instanceID)
    {
        auto timeline = omni::timeline::getTimeline();
        auto timelineEventStream = timeline.get()->getTimelineEventStream();
        auto& state = OgnSimulationDelayDatabase::sPerInstanceState<OgnSimulationDelay>(node, instanceID);

        // Using here Events 1.0 because in Isaac Sim 5.0.0 Kit SDK 107.3.1 is
        // used and "omni/timeline/TimelineTypes.h" here still isn't adapted
        // to Events 2.0. In Kit SDK 108.0 this problem is solved.
        state.timelineStopSubscription = carb::events::createSubscriptionToPopByType(
            timelineEventStream, static_cast<carb::events::EventType>(omni::timeline::TimelineEventType::eStop),
            [&state](carb::events::IEvent* event)
            {
                state.startSimulationTime = 0.0;
                state.isStarted = false;
            });
    }

    // Release subscription to Timeline Stop Event.
    static void releaseInstance(const NodeObj& node, GraphInstanceID instanceID)
    {
        auto& state = OgnSimulationDelayDatabase::sPerInstanceState<OgnSimulationDelay>(node, instanceID);
        state.timelineStopSubscription = nullptr;
    }

    static bool compute(OgnSimulationDelayDatabase& db)
    {
        auto& state = db.perInstanceState<OgnSimulationDelay>();
        auto iActionGraph = omni::graph::action::getInterface();
        auto simulationManager = carb::getCachedInterface<isaacsim::core::simulation_manager::ISimulationManager>();

        // Enable the finished output if delay is finished.
        bool wasFinishedAtThisCompute = false;
        if (state.isStarted)
        {
            double startSimulationTime = state.startSimulationTime;
            double elapsedTime = simulationManager->getSimulationTime() - startSimulationTime;
            double delayDuration = db.inputs.delayDuration();

            // TODO: Use float tolerance compare
            if (elapsedTime >= delayDuration)
            {
                wasFinishedAtThisCompute = true;
                state.isStarted = false;
                state.startSimulationTime = 0.0;
                db.outputs.isFinished() = true;
                iActionGraph->setExecutionEnabled(
                    outputs::finished.token(),
                    db.getInstanceIndex()
                );
            }
            else
            {
                db.outputs.isFinished() = false;
            }

            db.outputs.elapsedTime() = elapsedTime;
        }

        // Enable the regular tick output
        // if it wasn't finished this compute evaluation. 
        if (!wasFinishedAtThisCompute)
        {
            iActionGraph->setExecutionEnabled(
                outputs::tick.token(),
                db.getInstanceIndex()
            );
        }

        // Check for activation after finish check because
        // the delay can be activated at the same tick.
        // We assume there will not be 0-second delays.
        bool isActivationNeeded = iActionGraph->getExecutionEnabled(
            inputs::activate.token(),
            db.getInstanceIndex()
        );
        if (isActivationNeeded)
        {
            if (state.isStarted)
            {
                db.logWarning("Attempt to start the already started SimulationDelay. The activation signal is ignored.");
            }
            else
            {
                state.startSimulationTime = simulationManager->getSimulationTime();
                state.isStarted = true;
            }
        }

        return true;
    }
};

REGISTER_OGN_NODE()

} // nodes
} // simulation
} // kvantron
