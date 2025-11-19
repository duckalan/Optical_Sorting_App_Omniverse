#include "carb/events/EventsUtils.h"
#include "OgnRaycastProductSensorDatabase.h"
#include "omni/timeline/ITimeline.h"
#include "omni/timeline/TimelineTypes.h"
#include <carb/events/EventsTypes.h>
#include <carb/events/IEvents.h>
#include <carb/InterfaceUtils.h>
#include <carb/IObject.h>
#include <carb/Types.h>
#include <omni/graph/action/IActionGraph.h>
#include <omni/graph/core/Handle.h>
#include <omni/graph/core/iComputeGraph.h>
#include <omni/physx/IPhysxSceneQuery.h>

namespace kvantron
{
namespace simulation
{
namespace nodes
{
class OgnRaycastProductSensor
{
private:
    bool isHitOnPreviousCall{ false };
    carb::ObjectPtr<carb::events::ISubscription> timelineStopSubscription;

public:
    // TODO: Create separate class which implements this reset logic
    // and inherit from it.
    // Subscribe to Timeline Stop Event to reset the state on stop.
    static void initInstance(const NodeObj& node, GraphInstanceID instanceID)
    {
        auto timeline = omni::timeline::getTimeline();
        auto timelineEventStream = timeline.get()->getTimelineEventStream();
        auto& state = OgnRaycastProductSensorDatabase::sPerInstanceState<OgnRaycastProductSensor>(node, instanceID);

        // Using here Events 1.0 because in Isaac Sim 5.0.0 Kit SDK 107.3.1 is
        // used and "omni/timeline/TimelineTypes.h" here still isn't adapted
        // to Events 2.0. In Kit SDK 108.0 this problem is solved.
        state.timelineStopSubscription = carb::events::createSubscriptionToPopByType(
            timelineEventStream, static_cast<carb::events::EventType>(omni::timeline::TimelineEventType::eStop),
            [&state](carb::events::IEvent* event)
            {
                state.isHitOnPreviousCall = false;
            });
    }

    // Release subscription to Timeline Stop Event.
    static void releaseInstance(const NodeObj& node, GraphInstanceID instanceID)
    {
        auto& state = OgnRaycastProductSensorDatabase::sPerInstanceState<OgnRaycastProductSensor>(node, instanceID);
        state.timelineStopSubscription = nullptr;
    }


    static bool compute(OgnRaycastProductSensorDatabase& db)
    {
        // TODO: Validate these parameters:
        // origin is a normalized vector;
        // raycastRange must be greater that 0.
        const carb::Float3 rayOrigin = db.inputs.origin();
        const carb::Float3 rayDirection = db.inputs.direction();
        const float raycastRange = db.inputs.raycastRange();
        omni::physx::RaycastHit hitData{};
        auto physxSceneQuery = carb::getCachedInterface<omni::physx::IPhysxSceneQuery>();

        bool isHit = physxSceneQuery->raycastClosest(
            rayOrigin,
            rayDirection,
            raycastRange,
            hitData,
            false
        );

        auto iActionGraph = omni::graph::action::getInterface();
        auto& state = db.perInstanceState<OgnRaycastProductSensor>();
        if (isHit)
        {
            // Enable the regular tick output if the previous product was detected.
            if (state.isHitOnPreviousCall)
            {
                db.outputs.isNewProductDetected() = false;
                db.outputs.isProductDetected() = true;
                db.outputs.detectedProductPrim().resize(1);
                db.outputs.detectedProductPrim()[0] = static_cast<omni::graph::core::TargetPath>(hitData.rigidBody);
                iActionGraph->setExecutionEnabled(
                    outputs::execOut.token(),
                    db.getInstanceIndex()
                );
            }
            // Enable the newProductDetected output if a new product is detected.
            else
            {
                state.isHitOnPreviousCall = true;
                db.outputs.isNewProductDetected() = true;
                db.outputs.isProductDetected() = true;
                db.outputs.detectedProductPrim().resize(1);
                db.outputs.detectedProductPrim()[0] = static_cast<omni::graph::core::TargetPath>(hitData.collision);
                iActionGraph->setExecutionEnabled(
                    outputs::newProductDetected.token(),
                    db.getInstanceIndex()
                );
            }
        }
        // Enable the regular tick output if nothing was detected.
        else
        {
            state.isHitOnPreviousCall = false;
            db.outputs.isNewProductDetected() = false;
            db.outputs.isProductDetected() = false;
            db.outputs.detectedProductPrim().resize(0);
            iActionGraph->setExecutionEnabled(
                outputs::execOut.token(),
                db.getInstanceIndex()
            );
        }

        return true;
    }
};

REGISTER_OGN_NODE()

} // nodes
} // simulation
} // kvantron
