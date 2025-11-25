from pathlib import Path
import omni.usd
import omni.replicator.core as rep
from pxr import Usd, Sdf, UsdShade
import omni.graph.core as og
import omni.kit.material.library as mat_lib

def _get_node_type(node: og.Node):
    stage = omni.usd.get_context().get_stage()
    node_type_attr_path = Sdf.Path(node.get_prim_path()).AppendProperty("node:type")
    node_type = stage.GetAttributeAtPath(node_type_attr_path).Get()
    return node_type

def _get_nodes_to_delete(
    stage: Usd.Stage,
    projector_prims: rep.utils.ReplicatorItem,
    modify_projection_material: rep.utils.ReplicatorItem
) -> set[og.Node]:
    projector_prims_node: og.Node = projector_prims.node
    cube_xforms_paths: list[Sdf.Path] = projector_prims_node.get_attribute("inputs:primsIn").get()
    cubes_paths = [path.AppendChild("Cube") for path in cube_xforms_paths]

    replicator_graph: og.Graph = projector_prims_node.get_graph()
    # Find OgnGroup which contains created cubes
    ogn_groups = (n for n in replicator_graph.get_nodes() if _get_node_type(n) == "omni.replicator.core.OgnGroup")
    ogn_group_to_delete = None
    for ogn_group in ogn_groups:
        primsIn: list[Sdf.Path] = ogn_group.get_attribute("inputs:primsIn").get()
        if primsIn == cubes_paths:
            ogn_group_to_delete = ogn_group

    # Find OgnWritePrimAttribute and OgnScatter2D which
    nodes_to_delete: set[og.Node] = set()
    nodes_to_delete.add(ogn_group_to_delete)
    nodes_to_upstream_traverse = (n for n in replicator_graph.get_nodes() if _get_node_type(n) in ("omni.replicator.core.OgnWritePrimAttribute", "omni.replicator.core.OgnScatter2D"))
    for n in nodes_to_upstream_traverse:
        prims: list[Sdf.Path] = n.get_attribute("inputs:prims").get()
        if prims == cube_xforms_paths:
            node_prim = stage.GetPrimAtPath(n.get_prim_path())
            nodes_to_delete = nodes_to_delete | og.traverse_upstream_graph([node_prim])

    # Traverse from start to end
    modify_projection_material_prim = stage.GetPrimAtPath(modify_projection_material.node.get_prim_path())
    projector_prims_prim = stage.GetPrimAtPath(projector_prims.node.get_prim_path())
    nodes_to_delete = nodes_to_delete | og.traverse_upstream_graph([modify_projection_material_prim])
    nodes_to_delete = nodes_to_delete | og.traverse_downstream_graph([projector_prims_prim])

    return nodes_to_delete

def _get_prims_to_delete(projector_prims: rep.utils.ReplicatorItem) -> list[Sdf.Path]:
    cube_xforms_paths: list[Sdf.Path] = projector_prims.node.get_attribute("inputs:primsIn").get()
    return cube_xforms_paths

def vanish_inclusions_defect(
    projector_prims: rep.utils.ReplicatorItem,
    modify_projection_material: rep.utils.ReplicatorItem
):
    stage: Usd.Stage = omni.usd.get_context().get_stage()
    nodes_to_delete = _get_nodes_to_delete(stage, projector_prims, modify_projection_material)
    prims_to_delete = _get_prims_to_delete(projector_prims)
    replicator_graph: og.Graph = projector_prims.node.get_graph()

    defered_delete_nodes = []
    for node in nodes_to_delete:
        # App crahes when trying to delete it here.
        # TODO: clear this crap somewhere later
        if _get_node_type(node) == "omni.graph.action.OnImpulseEvent":
            defered_delete_nodes.append(node.get_prim_path())
            continue
        replicator_graph.destroy_node(node.get_prim_path(), True)

    # for node in defered_delete_nodes:
        # replicator_graph.destroy_node(node.get_prim_path(), True)

    for prim_to_delete in prims_to_delete:
        rep.utils._remove_prim_spec(stage.GetSessionLayer(), prim_to_delete.pathString)
        rep.utils._remove_prim_spec(stage.GetRootLayer(), prim_to_delete.pathString)



# TODO: add count and scale parameters
def create_inclusions_defect(
    cap_prim: Usd.Prim
) -> tuple[rep.utils.ReplicatorItem, rep.utils.ReplicatorItem]:
    cap_inner_top_panel_xform: Usd.Prim = cap_prim.GetChild("InnerTopPanel_Xform")
    cap_inner_top_panel = cap_inner_top_panel_xform.GetChild("InnerTopPanel")

    projector_prims = rep.create.cube(
        count=rep.random.randint(1, 4),
        visible=False,
        parent=cap_inner_top_panel_xform.GetPath().pathString,
        rotation=(0, -90, 0),
        scale=rep.distribution.uniform(0.001, 0.002)
    )

    with projector_prims:
        rep.randomizer.scatter_2d(
            surface_prims=[cap_inner_top_panel.GetPath().pathString],
            check_for_collisions=True
        )

    # TODO: Clean PBR materials for each cap
    # Create Project PBR Material inside the cap prim
    # mtl_path =
    # omni.kit.commands.execute(
    #     "CreateMdlMaterialPrim",
    #     mtl_url=mtl_url,
    #     mtl_name="ProjectPBRMaterial",
    #     mtl_path=mtl_path
    # )
    # mat_prim = stage.GetPrimAtPath(mtl_path)
    # UsdShade.MaterialBindingAPI(cap_inner_top_panel_xform).Bind(
    #     UsdShade.Material(mat_prim), UsdShade.Tokens.weakerThanDescendants
    # )

    projection_materials = rep.create.projection_material(
        projector_prims,
        input_prims=[cap_inner_top_panel_xform.GetPath().pathString]
    )
    modify_projection_material = None
    with projection_materials:
        modify_projection_material = rep.modify.projection_material(
            # diffuse="./Assets/Textures/Inclusion_BaseColor.png"
            diffuse=Path(r"C:\Users\Alexey\optical-sorting-app\source\extensions\kvantron.simulation.nodes\data\optical_sorting_stage\Assets\Textures\Inclusion_BaseColor.png").as_posix(),
        )

    return projector_prims, modify_projection_material