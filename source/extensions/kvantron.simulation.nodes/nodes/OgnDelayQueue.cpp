#include "carb/events/EventsUtils.h"
#include "carb/events/IEvents.h"
#include "OgnDelayQueueDatabase.h"
#include "omni/graph/action/IActionGraph.h"
#include "omni/timeline/ITimeline.h"
#include "omni/timeline/TimelineTypes.h"
#include <carb/events/EventsTypes.h>
#include <carb/InterfaceUtils.h>
#include <carb/IObject.h>
#include <deque>
#include <isaacsim/core/simulation_manager/ISimulationManager.h>
#include <omni/graph/core/Handle.h>
#include <omni/graph/core/iComputeGraph.h>

namespace kvantron
{
namespace simulation
{
namespace nodes
{
class OgnDelayQueue
{
    std::deque<double> startTimes{};
    carb::ObjectPtr<carb::events::ISubscription> timelineStopSubscription;

public:
    // Subscribe to Timeline Stop Event to reset the state on stop.
    static void initInstance(const NodeObj& node, GraphInstanceID instanceID)
    {
        auto timeline = omni::timeline::getTimeline();
        auto timelineEventStream = timeline.get()->getTimelineEventStream();
        auto& state = OgnDelayQueueDatabase::sPerInstanceState<OgnDelayQueue>(node, instanceID);

        // Using here Events 1.0 because in Isaac Sim 5.0.0 Kit SDK 107.3.1 is
        // used and "omni/timeline/TimelineTypes.h" here still isn't adapted
        // to Events 2.0. In Kit SDK 108.0 this problem is solved.
        state.timelineStopSubscription = carb::events::createSubscriptionToPopByType(
            timelineEventStream, static_cast<carb::events::EventType>(omni::timeline::TimelineEventType::eStop),
            [&state](carb::events::IEvent* event)
            {
                state.startTimes.clear();
                /*while (!state.startTimes.empty())
                {
                    state.startTimes.pop();
                }*/
            });
        
    }

    // Release subscription to Timeline Stop Event.
    static void releaseInstance(const NodeObj& node, GraphInstanceID instanceID)
    {
        auto& state = OgnDelayQueueDatabase::sPerInstanceState<OgnDelayQueue>(node, instanceID);
        state.timelineStopSubscription = nullptr;
    }

    static bool compute(OgnDelayQueueDatabase& db)
    {
        auto iActionGraph = omni::graph::action::getInterface();
        auto simulationManager = carb::getCachedInterface<isaacsim::core::simulation_manager::ISimulationManager>();
        auto& state = db.perInstanceState<OgnDelayQueue>();
        std::deque<double>& startTimes = state.startTimes;
        double currentTime = simulationManager->getSimulationTime();

        // Push back the current simulation time if the DelayQueue was
        // activated.
        bool isActivationNeeded = iActionGraph->getExecutionEnabled(
            inputs::activate.token(),
            db.getInstanceIndex()
        );
        if (isActivationNeeded)
        {
            double startTime = currentTime;
            startTimes.push_back(startTime);
        }

        // Check the oldest delay for completion and trigger
        // the finished output if so.
        if (!startTimes.empty())
        {
            double delayDuration = db.inputs.delayDuration();
            double oldestStartTime = startTimes.front();
            double elapsedTime = currentTime - oldestStartTime;
            if (elapsedTime >= delayDuration)
            {
                startTimes.pop_front();
                iActionGraph->setExecutionEnabled(
                    outputs::finished.token(),
                    db.getInstanceIndex()
                );
            }

            // Fill the elapsedTime output array.
            db.outputs.elapsedTime.resize(startTimes.size());
            for (size_t i = 0; i < startTimes.size(); i++)
            {
                db.outputs.elapsedTime()[i] = currentTime - startTimes[i];
            }
        }

        return true;
    }
};

REGISTER_OGN_NODE()

} // nodes
} // simulation
} // kvantron
