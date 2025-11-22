from collections import deque
from enum import StrEnum
from isaacsim.core.simulation_manager import SimulationManager
from kvantron.simulation.nodes.ogn.OgnSpawnBottleCapDatabase import OgnSpawnBottleCapDatabase
from math import fabs
from pxr import Usd, Sdf, Gf, UsdGeom
import omni.timeline
import omni.usd

CAP_SPAWN_SCOPE_PATH = Sdf.Path("/World/Caps")
PAINT_COLOR_VARIANT_SET_NAME = "Polypropelene_Paint_Color"
class CapPaint(StrEnum):
    Blue = "Blue"
    Dark_Orange = "Dark_Orange"
    Dark_Green = "Dark_Green"

class CapInstance:
    def __init__(self,
                 cap_prim: Usd.Prim,
                 spawn_time: float
    ):
        self._cap_prim: Usd.Prim = cap_prim
        self._spawn_time: float = spawn_time

    @property
    def cap_prim(self) -> Usd.Prim:
        return self._cap_prim

    @property
    def spawn_time(self) -> float:
        return self._spawn_time

    def __str__(self):
        return f"{self._cap_prim} (spawned at {self._spawn_time})"

class OgnSpawnBottleCapState:
    def __init__(self):
        self.current_cap_counter: int = 0
        self.last_spawn_time: float = 0.0
        self.cap_prims_deque: deque[CapInstance] = deque()
        self.timeline_stop_subeline_stop_sub = None

    def spawn_cap(
        self,
        stage: Usd.Stage,
        spawn_origin: Gf.Vec3d,
        cap_instance_path_template: str,
        cap_asset_path: str,
        cap_paint_color: CapPaint
    ) -> None:
        with Usd.EditContext(stage, stage.GetSessionLayer()):
            # Ensure caps have the specified spawn origin
            if not UsdGeom.Xform.Get(stage, CAP_SPAWN_SCOPE_PATH):
                xform: UsdGeom.Xform = UsdGeom.Xform.Define(stage, CAP_SPAWN_SCOPE_PATH)
                xform.AddTranslateOp(opSuffix="SpawnOrigin").Set(spawn_origin)
            else:
                xform = UsdGeom.Xform.Get(stage, CAP_SPAWN_SCOPE_PATH)
                current_position = xform.GetTranslateOp(opSuffix="SpawnOrigin")
                if current_position.Get() != spawn_origin:
                    current_position.Set(spawn_origin)

            self.current_cap_counter += 1
            cap_name = f"{cap_instance_path_template}{self.current_cap_counter:04d}"
            cap_path = CAP_SPAWN_SCOPE_PATH.AppendPath(cap_name)
            cap_prim: Usd.Prim = stage.DefinePrim(cap_path)

            # Add the cap asset as a reference
            cap_prim_refs: Usd.References = cap_prim.GetReferences()
            cap_prim_refs.AddReference(cap_asset_path)

            # Set the Color variant
            paint_color_vs: Usd.VariantSet = cap_prim.GetVariantSet(PAINT_COLOR_VARIANT_SET_NAME)
            paint_color_vs.SetVariantSelection(cap_paint_color.value)

            cap_instance = CapInstance(cap_prim, SimulationManager.get_simulation_time())

            self.cap_prims_deque.append(cap_instance)

    # TODO: delete the Replicator's nodes too.
    def _delete_cap_instance(
        self,
        stage: Usd.Stage,
        cap_instance: CapInstance
    ) -> None:
        with Usd.EditContext(stage, stage.GetSessionLayer()):
            stage.RemovePrim(cap_instance.cap_prim.GetPath())
        # After removing the cap there's still an empty prim in the root layer
        # so we have to remove it here too. Probably, it's better to use
        # omni.usd.DeletePrimsCommand.
        stage.RemovePrim(cap_instance.cap_prim.GetPath())
        del cap_instance

    def delete_oldest_cap(self, stage: Usd.Stage) -> None:
        if self.current_cap_counter <= 0:
            return
        oldest_cap = self.cap_prims_deque.popleft()
        self._delete_cap_instance(stage, oldest_cap)

    def reset(self, stage: Usd.Stage) -> None:
        self.current_cap_counter = 0
        self.last_spawn_time = 0.0
        with Usd.EditContext(stage, stage.GetSessionLayer()):
            stage.RemovePrim(CAP_SPAWN_SCOPE_PATH)
        stage.RemovePrim(CAP_SPAWN_SCOPE_PATH)
        self.cap_prims_deque.clear()

class OgnSpawnBottleCap:
    @staticmethod
    def internal_state() -> OgnSpawnBottleCapState:
        return OgnSpawnBottleCapState()

    # Subscribe to the Timeline Stop event to reset state on Stop.
    @staticmethod
    def init_itimeline_stop_subance(node, graph_instance_id):
        state: OgnSpawnBottleCapState = OgnSpawnBottleCapDatabase.get_internal_state(node, graph_instance_id)
        timeline = omni.timeline.get_timeline_interface()
        stage: Usd.Stage = omni.usd.get_context().get_stage()
        state.timeline_stop_sub = timeline.get_timeline_event_stream().create_subscription_to_pop_by_type(
            omni.timeline.TimelineEventType.STOP.value,
            lambda e: state.reset(stage)
        )

    # Release the subscription.
    @staticmethod
    def release_instance(node, graph_instance_id):
        state: OgnSpawnBottleCapState = OgnSpawnBottleCapDatabase.get_internal_state(node, graph_instance_id)
        state.timeline_stop_sub = None

    @staticmethod
    def compute(db: OgnSpawnBottleCapDatabase):
        state: OgnSpawnBottleCapState = db.per_instance_state
        cap_alive_time = db.inputs.capAliveTime
        distance_between_pair_caps = db.inputs.distanceBetweenPairCaps
        spawn_origin: Gf.Vec3d = Gf.Vec3d(db.inputs.spawnOrigin[0], db.inputs.spawnOrigin[1], db.inputs.spawnOrigin[2])
        cap_instance_path_template: str = db.inputs.capInstancePathTemplate
        cap_asset_path = db.inputs.capAssetPath
        cap_paint_color = CapPaint(db.inputs.capPaintColor)
        stage: Usd.Stage = omni.usd.get_context().get_stage()

        # Delete the oldest cap if it is timed out
        if len(state.cap_prims_deque) > 0:
            oldest_cap = state.cap_prims_deque[0]
            simulation_time = SimulationManager.get_simulation_time()
            if simulation_time - oldest_cap.spawn_time > cap_alive_time:
                state.delete_oldest_cap(stage)

        # Spawn a new cap if it is first
        if len(state.cap_prims_deque) == 0:
            state.spawn_cap(
                stage,
                spawn_origin,
                cap_instance_path_template,
                cap_asset_path,
                cap_paint_color
            )
        # TODO: calculate the distance between their boundaries.
        # Probably, use the conveyor speed and UsdGeomBoundable for it.
        # Spawn a new cap if the previous one has reached the caps pair distance
        else:
            newest_cap = state.cap_prims_deque[-1]
            newest_cap_world_matrix: Gf.Matrix4d = omni.usd.get_world_transform_matrix(newest_cap.cap_prim)
            newest_cap_position: Gf.Vec3d = newest_cap_world_matrix.ExtractTranslation()
            distance_to_newest_cap = fabs(newest_cap_position.GetLength() - spawn_origin.GetLength())
            if distance_to_newest_cap >= distance_between_pair_caps:
                state.spawn_cap(
                    stage,
                    spawn_origin,
                    cap_instance_path_template,
                    cap_asset_path,
                    cap_paint_color
                )

        return True