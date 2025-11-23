from collections import deque
from enum import StrEnum
from math import fabs
from pathlib import Path
from pxr import Usd, Sdf, Gf, UsdGeom

import omni.ext
import omni.usd
import omni.timeline
import carb
import omni.replicator.core as rep

from isaacsim.core.api.world import World
from isaacsim.core.api.physics_context import PhysicsContext
from isaacsim.core.simulation_manager import SimulationManager

from .generate_inclusions import (
    create_inclusions_defect,
    vanish_inclusions_defect
)

# ASSETS HERE
# manager = omni.kit.app.get_app().get_extension_manager()
# ext_id = manager.get_enabled_extension_id("omni.replicator.core")
# ext_path = manager.get_extension_path(ext_id)
# MDL_FOLDER = Path(ext_path).joinpath("mdl").as_posix()


CAP_SPAWN_SCOPE_PATH = Sdf.Path("/World/Caps")
PAINT_COLOR_VARIANT_SET_NAME = "Polypropelene_Paint_Color"

class CapPaint(StrEnum):
    Blue = "Blue"
    Dark_Orange = "Dark_Orange"
    Dark_Green = "Dark_Green"

class CapInstance:
    def __init__(
        self,
        cap_prim: Usd.Prim,
        spawn_time: float,
        projector_prims,
        modify_projection_material,
    ):
        self.cap_prim: Usd.Prim = cap_prim
        self.spawn_time: float = spawn_time
        self.projector_prims = projector_prims
        self.modify_projection_material = modify_projection_material

    def __str__(self):
        return f"{self.cap_prim} (spawned at {self.spawn_time})"


class CapSpawner:
    def __init__(
        self,
        cap_asset_path: str
    ):
        self.cap_prims_deque: deque[CapInstance] = deque()
        self.cap_alive_time: float = 4.0  # seconds
        self.distance_between_pair_caps: float = 30
        self.spawn_origin = Gf.Vec3d(0.0, 0.0, 162.4)
        self.cap_instance_path_template: str = "cap_"
        self.cap_asset_path: str = Path(r"C:\Users\Alexey\optical-sorting-app\source\extensions\kvantron.simulation.nodes\data\optical_sorting_stage\Assets\PolypropyleneBottleCapSimReady.usd").as_posix()
        self.cap_paint_color: CapPaint = CapPaint.Blue
        self.timeline_stop_sub = None

    def spawn_cap(self) -> None:
        stage: Usd.Stage = omni.usd.get_context().get_stage()
        # Ensure caps have the specified spawn origin
        if not UsdGeom.Xform.Get(stage, CAP_SPAWN_SCOPE_PATH):
            xform: UsdGeom.Xform = UsdGeom.Xform.Define(stage, CAP_SPAWN_SCOPE_PATH)
            xform.AddTranslateOp(opSuffix="SpawnOrigin").Set(self.spawn_origin)
        else:
            xform = UsdGeom.Xform.Get(stage, CAP_SPAWN_SCOPE_PATH)
            current_spawn_position = xform.GetTranslateOp(opSuffix="SpawnOrigin")
            if current_spawn_position.Get() != self.spawn_origin:
                current_spawn_position.Set(self.spawn_origin)

        # Using random number as the cap number because OgnScatter2D
        # is doing something with mesh so we can't create it with the same name
        cap_name = f"{self.cap_instance_path_template}{rep.random.randint(0, 9999999999):09d}"
        cap_path = CAP_SPAWN_SCOPE_PATH.AppendPath(cap_name)
        cap_prim: Usd.Prim = stage.DefinePrim(cap_path)
        cap_xform = UsdGeom.Xform.Get(stage, cap_path)
        deformed_scale = Gf.Vec3d(1, rep.random.uniform(0.8, 1.2), 1.11)
        cap_xform.AddScaleOp().Set(deformed_scale)

        # Add the cap asset as a reference
        cap_prim_refs: Usd.References = cap_prim.GetReferences()
        cap_prim_refs.AddReference(self.cap_asset_path)

        # Set the Color variant
        paint_color_vs: Usd.VariantSet = cap_prim.GetVariantSet(PAINT_COLOR_VARIANT_SET_NAME)
        paint_color_vs.SetVariantSelection(self.cap_paint_color.value)

        # Create inclusion defects
        projector_prims, modify_projection_material = create_inclusions_defect(cap_prim)

        cap_instance = CapInstance(
            cap_prim,
            SimulationManager.get_simulation_time(),
            projector_prims,
            modify_projection_material,
        )
        self.cap_prims_deque.append(cap_instance)

    def _delete_cap_instance(self, cap_instance: CapInstance) -> None:
        vanish_inclusions_defect(
            cap_instance.projector_prims,
            cap_instance.modify_projection_material
        )
        self._remove_prim_from_stage(cap_instance.cap_prim.GetPath())
        del cap_instance

    def delete_oldest_cap(self) -> None:
        if len(self.cap_prims_deque) == 0:
            return
        oldest_cap = self.cap_prims_deque.popleft()
        self._delete_cap_instance(oldest_cap)

    def reset(self) -> None:
        # Clear Replicator's scope and graph
        self._remove_prim_from_stage("/Replicator")
        # Clear all caps by deleting their parent xform
        self._remove_prim_from_stage(CAP_SPAWN_SCOPE_PATH)
        # Clear created ProjectPBR materials
        self._remove_prim_from_stage("/World/Looks")

        self.cap_prims_deque.clear()

    def _remove_prim_from_stage(self, prim_path: str):
        stage = omni.usd.get_context().get_stage()
        rep.utils._remove_prim_spec(stage.GetSessionLayer(), prim_path)
        rep.utils._remove_prim_spec(stage.GetRootLayer(), prim_path)

    @property
    def is_empty(self) -> True:
        return len(self.cap_prims_deque) == 0

class Extension(omni.ext.IExt):
    def on_startup(self, ext_id: str):
        # It must be done only when a stage is loaded
        # TODO: add UI to init CapSpawner
        self.world: World = World.instance() if World.instance() else World()
        self.world._physics_context = PhysicsContext()

        # TODO: find asset and texture folders paths with ext_id
        self.cap_spawner = CapSpawner("assetpath")

        # Physics callback
        self._physics_callback_name = "ogn_spawn_bottle_cap_physics"
        self.world.add_physics_callback(
                self._physics_callback_name,
                callback_fn=self._on_physics_step
            )
        carb.log_warn("Callback added")

        # Add the reset logic on Timeline Stop Event
        timeline = omni.timeline.get_timeline_interface()
        self.timeline_stop_sub = timeline.get_timeline_event_stream().create_subscription_to_pop_by_type(
            omni.timeline.TimelineEventType.STOP.value,
            lambda e: self.cap_spawner.reset()
        )

    def on_shutdown(self):
        self.cap_spawner.reset()
        self.world.clear_physics_callbacks()
        self.timeline_stop_sub = None

    # Physics-step callback: called every simulation physics step
    def _on_physics_step(self, step_size: float) -> None:
        # Delete the oldest cap if it is timed out
        if not self.cap_spawner.is_empty:
            oldest_cap = self.cap_spawner.cap_prims_deque[0]
            simulation_time = SimulationManager.get_simulation_time()
            if simulation_time - oldest_cap.spawn_time > self.cap_spawner.cap_alive_time:
                self.cap_spawner.delete_oldest_cap()

        # Spawn a new cap if none exist
        if self.cap_spawner.is_empty:
            self.cap_spawner.spawn_cap()
        # Spawn a new cap if last one reached the required distance from origin
        else:
            newest_cap = self.cap_spawner.cap_prims_deque[-1]

            newest_cap_world_matrix: Gf.Matrix4d = omni.usd.get_world_transform_matrix(newest_cap.cap_prim)
            newest_cap_position: Gf.Vec3d = newest_cap_world_matrix.ExtractTranslation()
            # TODO: distance between their distances from boundaries
            distance_to_newest_cap = fabs(newest_cap_position.GetLength() - self.cap_spawner.spawn_origin.GetLength())
            if distance_to_newest_cap >= self.cap_spawner.distance_between_pair_caps:
                self.cap_spawner.spawn_cap()
