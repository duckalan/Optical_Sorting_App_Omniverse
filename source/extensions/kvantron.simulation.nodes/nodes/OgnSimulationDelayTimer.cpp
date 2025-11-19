#include "carb/events/EventsUtils.h"
#include "carb/events/IEvents.h"
#include "omni/graph/action/IActionGraph.h"
#include "omni/timeline/ITimeline.h"
#include "omni/timeline/TimelineTypes.h"
#include "OgnSimulationDelayTimerDatabase.h"
#include <carb/events/EventsTypes.h>
#include <carb/InterfaceUtils.h>
#include <carb/IObject.h>
#include <omni/graph/core/Handle.h>
#include <omni/graph/core/iComputeGraph.h>

namespace kvantron
{
namespace simulation
{
namespace nodes
{
class OgnSimulationDelayTimer
{
    size_t currentPhysicsStep{ 0 };
    carb::ObjectPtr<carb::events::ISubscription> timelineStopSubscription;

public:
    // Subscribe to Timeline Stop Event to reset currentPhysicsStep on stop.
    static void initInstance(const NodeObj& node, GraphInstanceID instanceID)
    {
        auto timeline = omni::timeline::getTimeline();
        auto timelineEventStream = timeline.get()->getTimelineEventStream();
        auto& state = OgnSimulationDelayTimerDatabase::sPerInstanceState<OgnSimulationDelayTimer>(node, instanceID);

        // Using here Events 1.0 because in Isaac Sim 5.0.0 Kit SDK 107.3.1 is
        // used and "omni/timeline/TimelineTypes.h" here still isn't adapted
        // to Events 2.0. In Kit SDK 108.0 this problem is solved.
        state.timelineStopSubscription = carb::events::createSubscriptionToPopByType(
            timelineEventStream, static_cast<carb::events::EventType>(omni::timeline::TimelineEventType::eStop),
            [&state](carb::events::IEvent* event)
            {
                state.currentPhysicsStep = 0;
            });
    }

    // Release subscription to Timeline Stop Event.
    static void releaseInstance(const NodeObj& node, GraphInstanceID instanceID)
    {
        auto& state = OgnSimulationDelayTimerDatabase::sPerInstanceState<OgnSimulationDelayTimer>(node, instanceID);
        state.timelineStopSubscription = nullptr;
    }

    static bool compute(OgnSimulationDelayTimerDatabase& db)
    {
        auto& state = db.perInstanceState<OgnSimulationDelayTimer>();
        double simulationTimeDelta = db.inputs.simulationTimeDelta();
        
        // Start to increment from the beginning because during the first node
        // execution one physics step has already occured.
        state.currentPhysicsStep++;
        db.outputs.isFinished() = false;

        double elapsedTime = static_cast<double>(state.currentPhysicsStep) * simulationTimeDelta;
        db.outputs.elapsedTime() = elapsedTime;

        // Reset state after delay.
        // Probably, it's better to calculate 
        // the delay in physics steps and
        // compare only integers rather than
        // doubles, but it still works preciesely.
        if (elapsedTime >= db.inputs.delayDuration())
        {
            state.currentPhysicsStep = 0;
            db.outputs.isFinished() = true;

            auto actionGraph = omni::graph::action::getInterface();
            actionGraph->setExecutionEnabled(outputs::finished.token(), kAccordingToContextIndex);
        }

        return true;
    }
};

REGISTER_OGN_NODE()

} // nodes
} // simulation
} // kvantron
