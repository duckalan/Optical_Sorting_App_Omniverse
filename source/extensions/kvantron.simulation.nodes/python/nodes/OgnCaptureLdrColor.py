from kvantron.simulation.nodes.ogn.OgnCaptureLdrColorDatabase import OgnCaptureLdrColorDatabase
import omni.graph.core as og
import omni.replicator.core as rep
import numpy as np

class OgnCaptureLdrColor:
    @staticmethod
    def compute(db: OgnCaptureLdrColorDatabase):
        ldrcolor_annotator_node: og.Node = og.get_node_by_path(db.inputs.ldrcolorAnnotatorNodePath)

        if ldrcolor_annotator_node is None:
            db.log_error(f"Can't find an LdrColor annotator on the given path: {db.inputs.ldrcolorAnnotatorNodePath}. Skip execution.")
            return False
        ldrcolor_annotator_params = rep.AnnotatorRegistry._annotators.get("LdrColor")

        rgba8: np.ndarray = rep.annotators.annotator_utils.get_annotator_data(
                ldrcolor_annotator_node,
                ldrcolor_annotator_params,
                annotator_id=db.inputs.ldrcolorAnnotatorNodePath,
            )

        db.outputs.capturedLdrColorCPU_size = rgba8.size
        if db.outputs.capturedLdrColorCPU_size == 0:
            db.log_error("Captured none.")
            return False
        
        db.outputs.capturedLdrColorCPU = rgba8
        db.outputs.captureWidth = ldrcolor_annotator_node.get_attribute('outputs:width').get()
        db.outputs.captureHeight = ldrcolor_annotator_node.get_attribute('outputs:height').get()
        db.outputs.captureFormat = ldrcolor_annotator_node.get_attribute('outputs:format').get()
        return True
