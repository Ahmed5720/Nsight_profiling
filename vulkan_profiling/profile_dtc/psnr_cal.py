import os
import numpy as np
from skimage.io import imread
from skimage.metrics import peak_signal_noise_ratio as psnr
import re

models = []
with open("test_scenes", "r") as f:
    models = [line.strip() for line in f if line.strip()]

base_path = "/home/nisarg/data/DTC"
results_file = open("calibration_psnr_results.csv", "w")
results_file.write("model,light_strength,ambient_strength,psnr,psnr_std\n")
best_file = open("best_calibration_psnr.csv", "w")
best_file.write("model,light_strength,ambient_strength,psnr,psnr_std\n")

for model in models:
    model_path = os.path.join(base_path, model, "test")
    if not os.path.isdir(model_path):
        continue
    best_psnr = -np.inf
    best_cfg = None
    for d in os.listdir(model_path):
        m = re.match(r"pbr_cal_output_(\d+\.?\d*)_(\d+\.?\d*)", d)
        if m:
            light_strength = m.group(1)
            ambient_strength = m.group(2)
            cal_dir = os.path.join(model_path, d)
            gt_dir = os.path.join(base_path, model, "nerf_data", "test", "opaque")
            if not os.path.isdir(gt_dir) or not os.path.isdir(cal_dir):
                continue
            psnr_values = []
            for fname in os.listdir(gt_dir):
                gt_path = os.path.join(gt_dir, fname)
                cal_path = os.path.join(cal_dir, fname)
                if not os.path.isfile(gt_path) or not os.path.isfile(cal_path):
                    continue
                try:
                    gt_img = imread(gt_path)
                    cal_img = imread(cal_path)
                    if gt_img.shape != cal_img.shape:
                        continue
                    # Always ignore alpha channel if present
                    if gt_img.shape[-1] == 4:
                        gt_rgb = gt_img[:, :, :3]
                        cal_rgb = cal_img[:, :, :3]
                    else:
                        gt_rgb = gt_img
                        cal_rgb = cal_img
                    # Mask for non-black pixels in GT
                    mask = np.any(gt_rgb != 0, axis=-1)
                    if not np.any(mask):
                        continue
                    gt_masked = gt_rgb[mask]
                    cal_masked = cal_rgb[mask]
                    psnr_val = psnr(gt_masked, cal_masked, data_range=gt_masked.max() - gt_masked.min())
                    psnr_values.append(psnr_val)
                except Exception as e:
                    print(f"Error reading {fname}: {e}")
            if psnr_values:
                psnr_mean = np.mean(psnr_values)
                psnr_std = np.std(psnr_values)
                results_file.write(f"{model},{light_strength},{ambient_strength},{psnr_mean},{psnr_std}\n")
                print(f"{model} l={light_strength} a={ambient_strength} PSNR={psnr_mean:.2f}")
                if psnr_mean > best_psnr:
                    best_psnr = psnr_mean
                    best_cfg = (light_strength, ambient_strength, psnr_mean, psnr_std)
    if best_cfg is not None:
        best_file.write(f"{model},{best_cfg[0]},{best_cfg[1]},{best_cfg[2]},{best_cfg[3]}\n")

results_file.close()
best_file.close()
print("Calibration PSNR results saved to calibration_psnr_results.csv")
print("Best configurations saved to best_calibration_psnr.csv")