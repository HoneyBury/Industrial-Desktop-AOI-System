# AI Pipeline

The AI subsystem is designed around a practical industrial workflow:

1. collect defect images from the workstation
2. label and curate datasets
3. train YOLO models in Python
4. export ONNX for desktop deployment
5. load the ONNX model in the AOI workstation
6. persist AI results with inspection records

The repository currently includes script placeholders and a lightweight `AiInferencer` interface so
the deployment contract is visible even before a real runtime such as ONNX Runtime is added.

