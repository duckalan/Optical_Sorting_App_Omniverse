from kvantron.simulation.nodes.ogn.OgnAirEjectCapDatabase import OgnAirEjectCapDatabase
import omni.physx
import omni.physx.bindings._physx
import carb


class OgnAirEjectCap:


    @staticmethod
    def compute(db: OgnAirEjectCapDatabase):
        origin = db.inputs.overlapOrigin #carb.Float3(0.0, 0.0, 50.0)
        boxSize = db.inputs.boxSize
        extent = carb.Float3(boxSize / 2, boxSize / 2, boxSize / 2)
        rotation = carb.Float4(0.0, 0.0, 0.0, 1.0)
        last_rigid_body = None

        def overlap_callback(hit: omni.physx.bindings._physx.OverlapHit):
            nonlocal last_rigid_body
            if last_rigid_body != hit.rigid_body_encoded[0]:
                # carb.log_warn(f"OverlapPrim: {hit.rigid_body}, {hit.rigid_body_encoded}")
                physx_simulation = omni.physx.get_physx_simulation_interface()
                physx_simulation.apply_force_at_pos(
                    physx_simulation.get_attached_stage(),
                    hit.rigid_body_encoded[0],
                    carb.Float3(0, -0.25, 0.1),
                    carb.Float3(0.0, 0.0, 0.0),
                    mode='Impulse'
                )
                last_rigid_body = hit.rigid_body_encoded[0]

            return True

        physx_scene_query = omni.physx.get_physx_scene_query_interface()
        temp = physx_scene_query.overlap_box(
            extent,
            origin,
            rotation,
            overlap_callback
        )
        # carb.log_warn(f"Overlaped: {temp}")


        return True
