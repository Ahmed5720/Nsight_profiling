import os
import cv2
import numpy as np
from math import log10
from pprint import pprint
from skimage.metrics import structural_similarity as ssim
from skimage.metrics import peak_signal_noise_ratio as psnr
import lpips
import torch
import sys
from rich.progress import Progress
# from skimage.metrics import 

# model = sys.argv[1]

def calculate_psnr(img1, img2):
    # mask = np.all(img1 != [0, 0, 0, 0], axis=2)
    background_color = img1[-1, -1, :3]
    mask = np.any(img1[:, :, :3] != background_color, axis=-1)
    # print(img1[0,0],img2[0,0])
    masked_img1 = img1[:, :, :3][mask]
    masked_img2 = img2[:, :, :3][mask]

    return psnr(masked_img1, masked_img2)

def calculate_ssim(img1, img2):
    # mask = np.any(img1 != [0, 0, 0], axis=-1)
    background_color = img1[-1, -1, :3]
    mask = np.any(img1[:, :, :3] != background_color, axis=-1)
    masked_img1 = img1[:, :, :3] * mask[:, :, np.newaxis]
    masked_img2 = img2[:, :, :3] * mask[:, :, np.newaxis]

    ssim_value, _ = ssim(masked_img1, masked_img2, channel_axis=2, full=True)
    return ssim_value

def calculate_lpips(img1_path, img2_path):
    use_gpu = torch.cuda.is_available()
    loss_fn = lpips.LPIPS(net='alex', version='0.1')
    if use_gpu:
        loss_fn.cuda()

    img0 = lpips.load_image(img1_path)
    img1 = lpips.load_image(img2_path)

    # Create a mask to discard black pixels
    black_mask = np.any(img0 != [0, 0, 0], axis=-1)

    # Apply mask to discard black pixels
    img0[~black_mask] = 0
    img1[~black_mask] = 0

    img0_tensor = lpips.im2tensor(img0)
    img1_tensor = lpips.im2tensor(img1)

    if use_gpu:
        img0_tensor = img0_tensor.cuda()
        img1_tensor = img1_tensor.cuda()

    d = loss_fn.forward(img0_tensor, img1_tensor)
    return d.item()
    

def compute_psnr_for_dir(baseline_dir, rast_dir):
  filenames = [f for f in os.listdir(baseline_dir) if os.path.isfile(os.path.join(baseline_dir, f))]
  psnr = []
  ssim = []
  lpips = []

  for filename in filenames:
    if 'rgb' in filename:
      continue
    baseline_img = cv2.imread(os.path.join(baseline_dir, filename),cv2.IMREAD_UNCHANGED)
    rast_img = cv2.imread(os.path.join(rast_dir, filename), cv2.IMREAD_UNCHANGED)
    
    if baseline_img is None or rast_img is None:
      print(f"Skipping {filename}, image not found in one of the directories.")
      continue
    
    if baseline_img.shape[2] == 4:
      baseline_img = baseline_img[:, :, :3]

    if rast_img.shape[2] == 4:
      rast_img = rast_img[:, :, :3]

    psnr_test = calculate_psnr(baseline_img, rast_img)

    ssim_test = calculate_ssim(baseline_img, rast_img)

    lpips_test = calculate_lpips(os.path.join(baseline_dir, filename), os.path.join(rast_dir, filename))
    psnr.append(psnr_test)
    ssim.append(ssim_test)
    lpips.append(lpips_test)
    psnr_mean = np.mean(psnr)
    ssim_mean = np.mean(ssim)
    lpips_mean = np.mean(lpips)
    psnr_std = np.std(psnr)
    ssim_std = np.std(ssim)
    lpips_std = np.std(lpips)
    return psnr_mean, psnr_std, ssim_mean, ssim_std, lpips_mean, lpips_std

# Example usage:
# baseline_dir_rast = f'/home/nisarg/data/DTC/{model}/test/ours_30000/gt'
# baseline_dir_3dgs = f'/home/nisarg/data/DTC/{model}/nerf_data/test'
# test_rast = f'/home/nisarg/data/DTC/{model}/test/rast_output'
# test_3dgs = f'/home/nisarg/data/DTC/{model}/test/ours_30000/renders'

ofil = open(f"/home/nisarg/prof_res/quality/results_DTC.csv", "w")
ofil.write("model,psnr_rast,psnr_std_rast,psnr_3dgs,psnr_std_3dgs,psnr_pbr,psnr_std_pbr,psnr_pbr_shadow,psnr_std_pbr_shadow,ssim_rast,ssim_std_rast,ssim_3dgs,ssim_std_3dgs,ssim_pbr,ssim_std_pbr,ssim_pbr_shadow,ssim_std_pbr_shadow,lpips_rast,lpips_std_rast,lpips_3dgs,lpips_std_3dgs,lpips_pbr,lpips_std_pbr,lpips_pbr_shadow,lpips_std_pbr_shadow\n")

models = []
try:
  models = open("test_scenes", "r").readlines()
except Exception as e:
  print(f"Error reading test_scenes: {e}")
  models = []

with Progress() as progress:
  task = progress.add_task("[cyan]Processing nodes...", total=len(models))
  for node in models:
    if node == "UtilityCart":
      continue
    node = node.strip()
    print(node)
    if os.path.isdir(os.path.join("/home/nisarg/data/DTC",node)):
      baseline_dir_3dgs = os.path.join("/home/nisarg/data/DTC",node,'test','ours_30000','gt')
      baseline_dir_rast = os.path.join("/home/nisarg/data/DTC",node,'nerf_data','test','opaque')
      test_rast = os.path.join("/home/nisarg/data/DTC",node,'test','rast_output')
      test_pbr = os.path.join("/home/nisarg/data/DTC",node,'psnr','pbr_base_output')
      test_pbr_shadow = os.path.join("/home/nisarg/data/DTC",node,'test','pbr_output')
      test_3dgs = os.path.join("/home/nisarg/data/DTC",node,'test','ours_30000','renders')
      if not os.path.exists(baseline_dir_rast) or not os.path.exists(test_rast):
        print(f"Skipping {node}, {test_rast} does not exist.")
        continue
      if not os.path.exists(baseline_dir_3dgs) or not os.path.exists(test_3dgs):
        print(f"Skipping {node}, {test_3dgs} does not exist.")
        continue
      psnr_rast, psnr_std_rast, ssim_rast, ssim_std_rast, lpips_rast, lpips_std_rast = compute_psnr_for_dir(baseline_dir_rast, test_rast)
      psnr_3dgs, psnr_std_3dgs, ssim_3dgs, ssim_std_3dgs, lpips_3dgs, lpips_std_3dgs = compute_psnr_for_dir(baseline_dir_3dgs, test_3dgs)
      psnr_pbr, psnr_std_pbr, ssim_pbr, ssim_std_pbr, lpips_pbr, lpips_std_pbr = compute_psnr_for_dir(baseline_dir_rast, test_pbr)
      psnr_pbr_shadow, psnr_std_pbr_shadow, ssim_pbr_shadow, ssim_std_pbr_shadow, lpips_pbr_shadow, lpips_std_pbr_shadow = compute_psnr_for_dir(baseline_dir_rast, test_pbr_shadow)
      ofil.write(f"{node},{psnr_rast},{psnr_std_rast},{psnr_3dgs},{psnr_std_3dgs},{psnr_pbr},{psnr_std_pbr},{psnr_pbr_shadow},{psnr_std_pbr_shadow},{ssim_rast},{ssim_std_rast},{ssim_3dgs},{ssim_std_3dgs},{ssim_pbr},{ssim_std_pbr},{ssim_pbr_shadow},{ssim_std_pbr_shadow},{lpips_rast},{lpips_std_rast},{lpips_3dgs},{lpips_std_3dgs},{lpips_pbr},{lpips_std_pbr},{lpips_pbr_shadow},{lpips_std_pbr_shadow}\n")
      ofil.flush()
    progress.update(task, advance=1)