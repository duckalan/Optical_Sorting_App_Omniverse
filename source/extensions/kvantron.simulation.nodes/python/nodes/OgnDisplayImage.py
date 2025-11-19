from kvantron.simulation.nodes.ogn.OgnDisplayImageDatabase import OgnDisplayImageDatabase
import numpy as np
import omni.ui as ui
from random import randint
from isaacsim.core.simulation_manager import SimulationManager

class DisplayImageState:
    def __init__(self):
        self.window: ui.Window = None         # ui.Window
        self.provider: ui.ByteImageProvider = None       # ui.ByteImageProvider
        self.image_widget: ui.ImageWithProvider = None   # ui.ImageWithProvider
        self.label: ui.Label = None          # ui.Label
        self.window_id: int = 0

class OgnDisplayImage:
    @staticmethod
    def internal_state() -> DisplayImageState:
        return DisplayImageState()
    
    @staticmethod
    def init_instance(node, graph_instance_id):
        state: DisplayImageState = OgnDisplayImageDatabase.get_internal_state(node, graph_instance_id)
        state.window_id += 1

        # Create a floating window (undocked) and set up the layout
        state.window = ui.Window(f"Image Output{randint(0, 10000)}", dockPreference=ui.DockPreference.DISABLED)
        with state.window.frame:
            with ui.ZStack():
                # Create a ByteImageProvider for RGBA image data:contentReference[oaicite:2]{index=2}
                state.provider = ui.ByteImageProvider()
                # Create the image widget with the provider; will set size on compute
                state.image_widget = ui.ImageWithProvider(state.provider, fill_policy=ui.IwpFillPolicy.IWP_PRESERVE_ASPECT_FIT)
                # Place a label at top-left with a margin of 10 px using a Placer
                with ui.Placer(offset_x=10, offset_y=-100):
                    state.label = ui.Label("", word_wrap=False)
                    

    @staticmethod
    def compute(db: OgnDisplayImageDatabase):
        img_data: np.ndarray = db.inputs.imageData
        width = db.inputs.imageWidth
        height = db.inputs.imageHeight
        format = ui.TextureFormat(db.inputs.imageFormat)
        
        state: DisplayImageState = db.per_instance_state
        # state.window.width = width + 30
        # state.window.height = height + 30 
        state.provider.set_bytes_data(img_data.tolist(), [width, height], format)
        simulation_time = SimulationManager.get_simulation_time()
        state.label.text = f"Captured at: {simulation_time} s"

        return True
    
    @staticmethod
    def release_instance(node, graph_instance_id):
        # Clean up UI resources when the node instance is released
        state: DisplayImageState = OgnDisplayImageDatabase.get_internal_state(node, graph_instance_id)
        if state and state.window:
            state.image_widget.destroy()
            state.provider.destroy()
            state.window.destroy()
            state.window = None
            state.provider = None
            state.image_widget = None
            state.label = None