import numpy as np
import os
import json

class Helper:
  def __init__(self,trace_type="gpu_trace",base_path="/home/nisarg/data/",model='trex'):
    self.rast_path = "rast_pipeline/build/rasterizer"
    self.vkgs_path = "vkgs/build/vkgs_viewer"
    self.ngfx_path = "/tools/nvidia/NVIDIA-Nsight-Graphics-2024.2/host/linux-desktop-nomad-x64/ngfx"
    self.base_path = base_path
    self.ngfx_trace = trace_type
    self.model = model
    self.cameras = {}

  def get_proj_mat(self,fov_rad=0.4710899591445923, aspect=16/9, near=0.1, far=1000):
    # f is 1/tan(theta/2)
    # f = 1.0 / np.tan(fov_rad / 2.0)
    
    # # Initialize 4x4 matrix to zeros
    # M = np.zeros((4, 4), dtype=np.float32)
    
    # # Fill in the perspective projection terms (row-major)
    # M[0, 0] = f / aspect
    # M[1, 1] = f
    # M[2, 2] = - (far + near) / (far - near)
    # M[2, 3] = -1.0
    # M[3, 2] = - (2 * far * near) / (far - near)
    M = np.array([[2.777777671813965, 0, 0, 0],[0, 4.9382710456848145, 0, 0],[0, 0, -1.0001999139785767,-0.20002000033855438],[0, 0, -1, 0]], dtype=np.float32).transpose()

    return M
  
  def get_view_mat(self, M_cam):
    return np.linalg.inv(M_cam)
  
  def convert_matrices(self):
    transform_path = self.base_path + self.model + "/dataset_val/transforms_val.json"
    with open(transform_path, 'r') as f:
      data = json.load(f)
      cams = data['frames']
    if cams is None:
      raise Exception("No camera data found")
    vms = {}
    proj = self.get_proj_mat()
    for cam in cams:
      name = cam['file_path']
      M_cam = np.array(cam['transform_matrix'], dtype=np.float32)
      view = self.get_view_mat(M_cam)
      vms[name] = view.flatten().tolist()
    matrices = {}
    matrices['proj']=proj.flatten().tolist()
    matrices['view']=vms
    vp_path = self.base_path + self.model + "/dataset_val/transforms_vp.json"
    with open(vp_path, 'w') as f:
      json.dump(matrices, f)

  def get_model_path(self):
    return self.base_path + self.model + "/scene.gltf"
  
  def get_ply_path(self):
    return self.base_path + self.model + "/point_cloud/iteration_30000/point_cloud.ply"	
  
  def get_cam_path(self):
    return self.base_path + self.model + "/dataset_val/transforms_vp.json"
  
  def read_cam(self):
    if self.cameras is None:
      cam_path = self.get_cam_path()
      with open(cam_path, 'r') as f:
        self.cameras = json.load(f)
    

  def get_proj(self):
    return np.array(self.cameras['proj'])
  
  def get_view(self):
    return self.cameras['view']
  
  def get_exec_cmd(self, app="vkgs", vm=np.array([]), pm=np.array([]), mm=np.array([]),fil=""):
    pwd = os.getcwd()
    cmd = f"{pwd}/"
    if app == "vkgs":
      cmd += self.vkgs_path
      model_p = self.get_ply_path()
    elif app == "rast":
      cmd += self.rast_path
      model_p = self.get_model_path()
    else:
      raise Exception("Invalid application")
    
    cmd += f" -i {model_p}"
    if vm.size > 0:
      cmd += f" -v {' '.join(map(str, vm))}"
    if pm.size > 0:
      cmd += f" -p {' '.join(map(str, pm))}"
    if mm.size > 0:
      cmd += f" -m {' '.join(map(str, mm))}"
    
    if fil != "":
      opath = self.base_path + self.model + f"/{app}_output"
      if not os.path.exists(opath):
        os.makedirs(opath)
      opath += f"/{fil}.png"
      cmd += f" -o {opath}"
    return cmd
    
  
  def get_ngfx_cmd(self, app="vkgs", vm=np.array([]), pm=np.array([]), mm=np.array([])):
    pwd = os.getcwd()
    cmd = self.ngfx_path
    if self.ngfx_trace == "gpu_trace":
      cmd += " --activity=\"GPU Trace Profiler\""
    else:
      raise Exception("Invalid trace type")
    
    model_p = ""
    if app == "vkgs":
      model_p = self.get_ply_path()
      cmd += f" --exe={pwd}/{self.vkgs_path}"
    elif app == "rast":
      model_p = self.get_model_path()
      cmd += f" --exe={pwd}/{self.rast_path}"
    else:
      raise Exception("Invalid application")
    
    cmd += f" --args=\"-i {model_p}"
    if vm.size > 0:
      cmd += f" -v {' '.join(map(str, vm))}"
    if pm.size > 0:
      cmd += f" -p {' '.join(map(str, pm))}"
    if mm.size > 0:
      cmd += f" -m {' '.join(map(str, mm))}"
    cmd += "\""

    output_dir = f"/home/nisarg/prof_res/{self.model}_{app}/"
    if not os.path.exists(output_dir):
      os.makedirs(output_dir)
    cmd += f" --output-dir=\"{output_dir}\""
    # cmd += " --output-dir=\"/home/nisarg/prof_res/\""
    cmd += " --start-after-frames 1000"
    cmd += " --limit-to-frames 100"
    return cmd