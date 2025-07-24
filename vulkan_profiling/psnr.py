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
# from skimage.metrics import 

model = sys.argv[1]

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

    # Load alpha channel to create mask
    alpha_mask = cv2.imread(img1_path, cv2.IMREAD_UNCHANGED)[:, :, 3] != 0

    # Apply mask to discard pixels with alpha 0
    img0[~alpha_mask] = 0
    img1[~alpha_mask] = 0

    img0_tensor = lpips.im2tensor(img0)
    img1_tensor = lpips.im2tensor(img1)

    if use_gpu:
        img0_tensor = img0_tensor.cuda()
        img1_tensor = img1_tensor.cuda()

    d = loss_fn.forward(img0_tensor, img1_tensor)
    return d.item()
    

def compute_psnr_for_directories(baseline_dir, test_dir1, test_dir2):
    filenames = [f for f in os.listdir(baseline_dir) if os.path.isfile(os.path.join(baseline_dir, f))]
    results_rast = []
    results_vkgs = []
    ssim_rast = []
    ssim_vkgs = []
    lpips_rast = []
    lpips_vkgs = []
    results = []

    for filename in filenames:
        if 'rgb' in filename:
            continue
        baseline_img = cv2.imread(os.path.join(baseline_dir, filename),cv2.IMREAD_UNCHANGED)
        test1_img = cv2.imread(os.path.join(test_dir1, filename), cv2.IMREAD_UNCHANGED)
        test2_img = cv2.imread(os.path.join(test_dir2, filename), cv2.IMREAD_UNCHANGED)
        
        background = np.zeros((baseline_img.shape[0], baseline_img.shape[1], 3), dtype=np.float32)
        # background.fill(1.0)

        modified = [False,False,False]
        
        if baseline_img is None or test1_img is None or test2_img is None:
            print(f"Skipping {filename}, image not found in one of the directories.")
            continue
        
        if baseline_img.shape[2] == 4:
            # alpha_channel = baseline_img[:, :, 3].astype(np.float32) / 255.0
            # baseline_img = baseline_img[:, :, :3].astype(np.float32) / 255.0
            # alpha_channel = np.repeat(alpha_channel[:, :, np.newaxis], 3, axis=2)
            # baseline_img = cv2.multiply(alpha_channel, baseline_img)
            # baseline_img = cv2.add(cv2.multiply(1 - alpha_channel, background), baseline_img)
            # baseline_img = (baseline_img * 255).astype(np.uint8)
            baseline_img = baseline_img[:, :, :3]
            # baseline_img = cv2.cvtColor(baseline_img, cv2.COLOR_BGR2RGB)
            modified[0] = True

        if test1_img.shape[2] == 4:
            # alpha_channel_test1 = test1_img[:, :, 3].astype(np.float32) / 255.0
            # test1_img = test1_img[:, :, :3].astype(np.float32) / 255.0
            # alpha_channel_test1 = np.repeat(alpha_channel_test1[:, :, np.newaxis], 3, axis=2)
            # test1_img = cv2.multiply(alpha_channel_test1, test1_img)
            # test1_img = cv2.add(cv2.multiply(1 - alpha_channel_test1, background), test1_img)
            # test1_img = (test1_img * 255).astype(np.uint8)
            test1_img = test1_img[:, :, :3]
            # test1_img = cv2.cvtColor(test1_img, cv2.COLOR_BGR2RGB)
            modified[1] = True

        if test2_img.shape[2] == 4:
            # alpha_channel_test2 = test2_img[:, :, 3].astype(np.float32) / 255.0
            # test2_img = test2_img[:, :, :3].astype(np.float32) / 255.0
            # alpha_channel_test2 = np.repeat(alpha_channel_test2[:, :, np.newaxis], 3, axis=2)
            # test2_img = cv2.multiply(alpha_channel_test2, test2_img)
            # test2_img = cv2.add(cv2.multiply(1 - alpha_channel_test2, background), test2_img)
            # test2_img = (test2_img * 255).astype(np.uint8)
            test2_img = test2_img[:, :, :3]
            # test2_img = cv2.cvtColor(test2_img, cv2.COLOR_BGR2RGB)
            modified[2] = True

        if modified[0]:
            cv2.imwrite(os.path.join(baseline_dir, f"rrgb_{filename}"), baseline_img)
        if modified[1]:
            cv2.imwrite(os.path.join(test_dir1, f"rrgb_{filename}"), test1_img)
        if modified[2]:
            cv2.imwrite(os.path.join(test_dir2, f"rrgb_{filename}"), test2_img)

        psnr_test1 = calculate_psnr(baseline_img, test1_img)
        psnr_test2 = calculate_psnr(baseline_img, test2_img)

        ssim_test1 = calculate_ssim(baseline_img, test1_img)
        ssim_test2 = calculate_ssim(baseline_img, test2_img)

        lpips_test1 = calculate_lpips(os.path.join(baseline_dir, filename), os.path.join(test_dir1, filename))
        lpips_test2 = calculate_lpips(os.path.join(baseline_dir, filename), os.path.join(test_dir2, filename))

        results.append((filename, psnr_test1, psnr_test2, ssim_test1, ssim_test2, lpips_test1, lpips_test2))
        results_rast.append(psnr_test1)
        results_vkgs.append(psnr_test2)
        ssim_rast.append(ssim_test1)
        ssim_vkgs.append(ssim_test2)
        lpips_rast.append(lpips_test1)
        lpips_vkgs.append(lpips_test2)

    return results, results_rast, results_vkgs, ssim_rast, ssim_vkgs, lpips_rast, lpips_vkgs

# Example usage:
baseline_dir = f'/home/nisarg/data/{model}/dataset_val/val'
test_rast = f'/home/nisarg/data/{model}/rast_output'
test_vkgs = f'/home/nisarg/data/{model}/vkgs_output'

psnr_results, psnrrast, psnrvkgs, ssimrast, ssimvkgs, lpips_rast, lpips_vkgs = compute_psnr_for_directories(baseline_dir, test_rast, test_vkgs)

ofil = open(f"/home/nisarg/prof_res/quality/results_{model}.csv", "w")
ofil.write("filename,psnr_rast,psnr_vkgs,ssim_rast,ssim_vkgs,lpips_rast,lpips_vkgs\n")
for filename, psnr1, psnr2, ssim1, ssim2, lpips1, lpips2 in psnr_results:
    ofil.write(f"{filename},{psnr1},{psnr2},{ssim1},{ssim2},{lpips1},{lpips2}\n")
    # print(f"{filename}: PSNR Test Case 1 = {psnr1:.2f} dB, PSNR Test Case 2 = {psnr2:.2f} dB, SSIM Test Case 1 = {ssim1:.4f}, SSIM Test Case 2 = {ssim2:.4f}, LPIPS Test Case 1 = {lpips1:.4f}, LPIPS Test Case 2 = {lpips2:.4f}")

# print(f"Average PSNR for Rasterizer: {np.mean(psnrrast):.2f} dB")
# print(f"Average PSNR for 3DGS: {np.mean(psnrvkgs):.2f} dB")
# print(f"Average SSIM for Rasterizer: {np.mean(ssimrast):.4f}")
# print(f"Average SSIM for 3DGS: {np.mean(ssimvkgs):.4f}")