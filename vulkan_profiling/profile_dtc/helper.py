import numpy as np
import os
import json
import pathlib
cpath = pathlib.Path(__file__).parent.resolve()

class Helper:
  def __init__(self,trace_type="gpu_trace",base_path="/home/nisarg/data/DTC/",model='trex'):
    self.rast_path = "rast_pipeline/build/rasterizer"
    self.pbr_path = "pbr_pipeline/build/pbr_viewer"
    self.dgs_path = "vk_gaussian_splatting/bin_x64/Release/vk_gaussian_splatting_app"
    self.ngfx_path = "/tools/nvidia/NVIDIA-Nsight-Graphics-2024.2/host/linux-desktop-nomad-x64/ngfx"
    self.base_path = base_path
    self.ngfx_trace = trace_type
    self.model = model
    self.cameras = None

  def get_proj_mat(self,fov_rad=0.4710899591445923, aspect=16/9, near=0.1, far=1000):
    M = np.array([[2.777777671813965, 0, 0, 0],[0, 4.9382710456848145, 0, 0],[0, 0, -1.0001999139785767,-0.20002000033855438],[0, 0, -1, 0]], dtype=np.float32).transpose()

    return M
  
  def get_view_mat(self, M_cam):
    return np.linalg.inv(M_cam)
  
  def convert_matrices(self,run='test'):
    transform_path = self.base_path + self.model + "/nerf_data" + f"/transforms_{run}.json"
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
    vp_path = self.base_path + self.model + "/nerf_data" + f"/transforms_{run}_vp.json"
    with open(vp_path, 'w') as f:
      json.dump(matrices, f)

  def get_model_path(self,baked=False):
    if baked:
      return self.base_path + self.model + f"/{self.model}_baked.gltf"
    return self.base_path + self.model + f"/{self.model}.gltf"
  
  def get_ply_path(self):
    return self.base_path + self.model + "/point_cloud/iteration_30000/point_cloud.ply"	
  
  def get_cam_path(self,run='test'):
    return self.base_path + self.model + f"/nerf_data/transforms_{run}_vp.json"
  
  def read_cam(self):
    if self.cameras is None:
      cam_path = self.get_cam_path()
      with open(cam_path, 'r') as f:
        self.cameras = json.load(f)
    

  def get_proj(self):
    return np.array(self.cameras['proj'])
  
  def get_view(self):
    return self.cameras['view']
  
  def get_exec_cmd(self, app="3dgs", run='test', vm=np.array([]), pm=np.array([]), mm=np.array([]),fil="", light_strength=3.0, ambient_strength=0.01):
    pwd = os.path.abspath(os.path.join(os.path.dirname(__file__), os.path.pardir))
    cmd = f"{pwd}/"
    if app == "3dgs":
      cmd += self.dgs_path
      model_p = self.get_ply_path()
    elif app == "rast":
      cmd += self.rast_path
      model_p = self.get_model_path(baked=True)
    elif app == "pbr":
      cmd += self.pbr_path
      model_p = self.get_model_path()
    elif app == "pbr_pcf" or app == "pbr_cal":
      cmd += self.pbr_path
      model_p = self.get_model_path()
    elif app == "pbr_shadow":
      cmd += self.pbr_path
      model_p = self.get_model_path()
    else:
      raise Exception("Invalid application")
    
    cmd += f" -i {model_p}"
    if vm.size > 0:
      cmd += f" -v {' '.join(map(str, vm))}"
    if pm.size > 0:
      cmd += f" -p {' '.join(map(str, pm))}"
    if(app != "3dgs"):
      if mm.size > 0:
        cmd += f" -m {' '.join(map(str, mm))}"

    if app == "pbr_pcf" or app == "pbr_cal":
      cmd += " --shadow --pcf"
    elif app == "pbr_shadow":
      cmd += " --shadow"
    else:
      pass

    if app=="pbr" or app=="pbr_pcf" or app=="pbr_shadow":
      cmd += f" --light {light_strength} --ambient {ambient_strength}"
    
    if fil != "":
      if app == "pbr_cal":
        opath = self.base_path + self.model+ f"/{run}" + f"/{app}_output_{light_strength}_{ambient_strength}"
      else:
        opath = self.base_path + self.model+ f"/{run}" + f"/{app}_output"
      if not os.path.exists(opath):
        os.makedirs(opath)
      opath += f"/{fil}.png"
      cmd += f" -o {opath}"
    return cmd
    
  
  def get_ngfx_cmd(self, app="3dgs", vm=np.array([]), pm=np.array([]), mm=np.array([]),counter=0,light_strength=3.0, ambient_strength=0.01):
    pwd = os.path.abspath(os.path.join(os.path.dirname(__file__), os.path.pardir))
    cmd = self.ngfx_path
    if self.ngfx_trace == "gpu_trace":
      cmd += " --activity=\"GPU Trace Profiler\""
    else:
      raise Exception("Invalid trace type")
    
    model_p = ""
    if app == "3dgs":
      model_p = self.get_ply_path()
      cmd += f" --exe={pwd}/{self.dgs_path}"
    elif app == "rast":
      model_p = self.get_model_path(baked=True)
      cmd += f" --exe={pwd}/{self.rast_path}"
    elif app == "pbr":
      model_p = self.get_model_path()
      cmd += f" --exe={pwd}/{self.pbr_path}"
    elif app == "pbr_shadow":
      model_p = self.get_model_path()
      cmd += f" --exe={pwd}/{self.pbr_path}"
    elif app == "pbr_pcf":
      model_p = self.get_model_path()
      cmd += f" --exe={pwd}/{self.pbr_path}"
    else:
      raise Exception("Invalid application")
    
    cmd += f" --args=\"-i {model_p}"
    if vm.size > 0:
      cmd += f" -v {' '.join(map(str, vm))}"
    if pm.size > 0:
      cmd += f" -p {' '.join(map(str, pm))}"
    if(app != "3dgs"):
      if mm.size > 0:
        cmd += f" -m {' '.join(map(str, mm))}"

    if app == "pbr_pcf":
      cmd += " --shadow --pcf"
    elif app == "pbr_shadow":
      cmd += " --shadow"
    else:
      pass
    
    if app=="pbr" or app=="pbr_pcf" or app=="pbr_shadow":
      cmd += f" --light {light_strength} --ambient {ambient_strength}"
    cmd += "\""

    output_dir = f"/home/nisarg/prof_res/DTC/{self.model}_{app}/{counter}"
    if not os.path.exists(output_dir):
      os.makedirs(output_dir)
    cmd += f" --output-dir=\"{output_dir}\""
    cmd += " --start-after-frames 1000"
    cmd += " --limit-to-frames 100"
    cmd += " --auto-export"
    return cmd