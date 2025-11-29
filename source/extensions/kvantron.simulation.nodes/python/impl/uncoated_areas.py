import os
from pathlib import Path
import omni.usd
from random import choice
from pxr import Usd, Sdf
import omni.kit.app
import carb

class UncoatedAreasGenerator:
    def __init__(self):
        ext_manager = omni.kit.app.get_app().get_extension_manager()
        ext_path = Path(ext_manager.get_extension_path_by_module("kvantron.simulation.nodes"))
        # ext_id = ext_manager.get_enabled_extension_id("kvantron.simulation.nodes")
        # ext_path = ext_manager.get_extension_path(ext_id)
        noises_folder = ext_path.joinpath(
            "data", "optical_sorting_stage", "Assets", "Textures", "UncoatedAreas"
        )
        # noises_folder = Path(r"C:\Users\Alexey\optical-sorting-app\source\extensions\kvantron.simulation.nodes\data\optical_sorting_stage\Assets\Textures\UncoatedAreas")
        self.noise_textures = []
        for file in os.listdir(noises_folder.as_posix()):
            self.noise_textures.append(noises_folder.joinpath(file).as_posix())

    def generate_uncoated_areas(self, cap_prim: Usd.Prim):
        stage: Usd.Stage =  omni.usd.get_context().get_stage()
        cap_prim_path: Sdf.Path = cap_prim.GetPath()

        mask_texture_node_path: Sdf.Path = cap_prim_path.AppendPath("Looks/Polypropylene_Masked/Mask_Texture")
        mask_texture_texture_path: Sdf.Path = mask_texture_node_path.AppendProperty("inputs:texture")

        texture_input: Usd.Attribute = stage.GetAttributeAtPath(mask_texture_texture_path)
        texture_input.Set(choice(self.noise_textures))