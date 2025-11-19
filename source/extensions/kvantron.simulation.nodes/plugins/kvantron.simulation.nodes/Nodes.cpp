#include <carb/PluginUtils.h>

#include <omni/core/ModuleInfo.h>
#include <omni/core/Omni.h>
#include <omni/graph/core/ogn/Registration.h>

OMNI_PLUGIN_IMPL_DEPS(omni::graph::core::IGraphRegistry, omni::fabric::IToken)
OMNI_MODULE_GLOBALS("kvantron.simulation.nodes.plugin", "Helpful text describing the plugin");

DECLARE_OGN_NODES();

namespace
{

void onModuleStarted()
{
    CARB_LOG_INFO("onModuleStarted");
    INITIALIZE_OGN_NODES();
}

bool onModuleCanUnload()
{
    CARB_LOG_INFO("onModuleCanUnload");
    return true;
}

void onModuleUnload()
{
    CARB_LOG_INFO("onModuleUnload");
    RELEASE_OGN_NODES();
}

} // namespace anonymous

// Hook up the above functions to be called at the right times
OMNI_MODULE_API omni::core::Result omniModuleGetExports(omni::core::ModuleExports* out)
{
    OMNI_MODULE_SET_EXPORTS(out);
    OMNI_MODULE_ON_MODULE_STARTED(out, onModuleStarted);
    OMNI_MODULE_ON_MODULE_CAN_UNLOAD(out, onModuleCanUnload);
    OMNI_MODULE_ON_MODULE_UNLOAD(out, onModuleUnload);

    return omni::core::kResultSuccess;
}
