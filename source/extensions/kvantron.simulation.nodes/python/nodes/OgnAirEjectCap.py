from kvantron.simulation.nodes.ogn.OgnAirEjectCapDatabase import OgnAirEjectCapDatabase
import omni.physx
import omni.physx.bindings._physx
import carb


class OgnAirEjectCap:


    @staticmethod
    def compute(db: OgnAirEjectCapDatabase):
        origin = db.inputs.overlapOrigin
        boxSize = db.inputs.boxSize
        extent = carb.Float3(boxSize / 2, boxSize / 2, boxSize / 2)
        rotation = carb.Float4(0.0, 0.0, 0.0, 1.0)
        impulse = carb.Float3(*db.inputs.impulse)
        forceOrigin = carb.Float3(*db.inputs.forceOrigin)
        last_rigid_body = None

        def overlap_callback(hit: omni.physx.bindings._physx.OverlapHit):
            nonlocal last_rigid_body
            if last_rigid_body != hit.rigid_body_encoded[0]:
                physx_simulation = omni.physx.get_physx_simulation_interface()
                physx_simulation.apply_force_at_pos(
                    physx_simulation.get_attached_stage(),
                    hit.rigid_body_encoded[0],
                    impulse,
                    forceOrigin,
                    mode='Impulse'
                )
                last_rigid_body = hit.rigid_body_encoded[0]

            return True

        physx_scene_query = omni.physx.get_physx_scene_query_interface()
        physx_scene_query.overlap_box(
            extent,
            origin,
            rotation,
            overlap_callback
        )

        return True
