#include "OgnOnPostProcessStepDatabase.h"
#include <carb/eventdispatcher/EventDispatcherTypes.h>
#include <carb/eventdispatcher/IEventDispatcher.h>
#include <carb/InterfaceUtils.h>
#include <carb/RString.h>
#include <carb/settings/ISettings.h>
#include <omni/graph/action/IActionGraph.h>
#include <omni/graph/core/Handle.h>
#include <omni/graph/core/iComputeGraph.h>
#include <omni/kit/AppTypes.h>
#include <omni/log/ILog.h>
#include <omni/kit/KitUpdateOrder.h>

namespace kvantron
{
namespace simulation
{
namespace nodes
{
class OgnOnPostProcessStep
{
private:
    carb::eventdispatcher::ObserverGuard kitUpdateEventSub{ };
    bool isSet{ false };

public:
    static void initInstance(const NodeObj& node, GraphInstanceID instanceID)
    {
        auto settings = carb::getCachedInterface<carb::settings::ISettings>();

        // Same as in post_process_graph_tick_order in SyntheticData.py 
        // of omni.syntheticdata.
        int postProcessTickOrder = settings
            ->getAsInt("/app/updateOrder/checkForHydraRenderComplete") + 1;
        int afterPostProcessTickOrder = postProcessTickOrder + 1;
        if (afterPostProcessTickOrder < omni::kit::update::eUsdContextUpdate) {
            OMNI_LOG_WARN(
                "The Replicator is set to acquire rendered frames only at the beginning of the next Kit update. If you need no delays, use the Isaac Sim's Zero Delay application layer or its settings in your Kit application.",
                afterPostProcessTickOrder
            );
        }

        auto& state = OgnOnPostProcessStepDatabase::sPerInstanceState<OgnOnPostProcessStep>(node, instanceID);

        auto ed = carb::getCachedInterface<carb::eventdispatcher::IEventDispatcher>();

        //
        state.kitUpdateEventSub = ed->observeEvent(
            carb::RStringKey("OnPostProcessStep evaluation."),
            afterPostProcessTickOrder,
            omni::kit::kGlobalEventUpdate,
            [&node, instanceID](const carb::eventdispatcher::Event& e)
            {
                auto& state = OgnOnPostProcessStepDatabase::sPerInstanceState<OgnOnPostProcessStep>(node, instanceID);
                state.isSet = true;

                auto iNode = carb::getCachedInterface<omni::graph::core::INode>();
                auto graphObj = iNode->getGraph(node);
                graphObj.iGraph->evaluate(graphObj);
            }
        );
    }

    static void releaseInstance(const NodeObj& node, GraphInstanceID instanceID)
    {
        auto& state = OgnOnPostProcessStepDatabase::sPerInstanceState<OgnOnPostProcessStep>(node, instanceID);
        state.kitUpdateEventSub.reset();
    }

    static bool compute(OgnOnPostProcessStepDatabase& db)
    {
        auto& state = db.perInstanceState<OgnOnPostProcessStep>();
        if (state.isSet) {
            state.isSet = false;

            auto actionGraph = omni::graph::action::getInterface();
            actionGraph->setExecutionEnabled(
                outputs::execOut.token(),
                kAccordingToContextIndex
            );
        }

        return true;
    }
};

REGISTER_OGN_NODE()

} // nodes
} // simulation
} // kvantron
