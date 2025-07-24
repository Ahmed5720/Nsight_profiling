import os
import json
from PIL import Image, ImageDraw, ImageFont

models = []
with open("test_scenes",'r') as f:
  for line in f:
    line = line.strip()
    if not line:
      continue
    models.append(line)

base_path = "/home/nisarg/data/DTC"

for model in models:
  transforms_path = os.path.join(base_path, model, "nerf_data", "transforms_test.json")
  with open(transforms_path, 'r') as f:
    data = json.load(f)
  frame_idx = 3
  frame_0 = data['frames'][frame_idx]
  rel_path = frame_0['file_path']
  rel_path = rel_path.split("/")[-1]+".png"
  gt_path = os.path.join(base_path, model, "nerf_data/test/opaque", rel_path)
  if not os.path.exists(gt_path):
    print(f"GT path does not exist: {gt_path}")
    continue
  pbr_path_pcf = os.path.join(base_path, model, "test/pbr_pcf_output", rel_path)
  if not os.path.exists(pbr_path_pcf):
    print(f"PBR path does not exist: {pbr_path_pcf}")
    continue
  pbr_path_shadow = os.path.join(base_path, model, "test/pbr_shadow_output", rel_path)
  if not os.path.exists(pbr_path_shadow):
    print(f"PBR path does not exist: {pbr_path_shadow}")
    continue
  pbr_path = os.path.join(base_path, model, "test/pbr_output", rel_path)
  if not os.path.exists(pbr_path):
    print(f"PBR path does not exist: {pbr_path}")
    continue
  rast_path = os.path.join(base_path, model, "test/rast_output", rel_path)
  if not os.path.exists(rast_path):
    print(f"Rast path does not exist: {rast_path}")
    continue
  gs_vk_path = os.path.join(base_path, model, "test/3dgs_output", rel_path)
  if not os.path.exists(gs_vk_path):
    print(f"GS path does not exist: {gs_vk_path}")
    continue
  gs_img = f"{frame_idx:05d}.png"
  gs_path = os.path.join(base_path, model, "test/ours_30000/renders", gs_img)
  if not os.path.exists(gs_path):
    print(f"GS path does not exist: {gs_path}")
    continue
  gt_image = Image.open(gt_path)
  pbr_image_pcf = Image.open(pbr_path_pcf)
  pbr_image_shadow = Image.open(pbr_path_shadow)
  pbr_image = Image.open(pbr_path)
  rast_image = Image.open(rast_path)
  gs_vk_image = Image.open(gs_vk_path)
  gs_image = Image.open(gs_path)
  width, height = gt_image.size

  #set label height
  label_height = 30
  new_width = width * 3
  new_height = height * 2 + label_height * 2
  new_image = Image.new('RGB', (new_width, new_height), (255, 255, 255))

  # Prepare to draw labels
  draw = ImageDraw.Draw(new_image)
  try:
    font = ImageFont.truetype("DejaVuSans-Bold.ttf", 30)
  except:
    font = ImageFont.load_default(size=30)

  # Top row labels
  labels_top = ["GT", "3DGS", "PBR+PCF"]
  for i, label in enumerate(labels_top):
    bbox = font.getbbox(label)
    text_width = bbox[2] - bbox[0]
    text_height = bbox[3] - bbox[1]
    x = i * width + (width - text_width) // 2
    y = 0 + (label_height - text_height) // 2
    draw.text((x, y), label, fill=(0, 0, 0), font=font)

  # Bottom row labels
  labels_bottom = ["PBR+Shadow", "PBR", "Baked Texture"]
  for i, label in enumerate(labels_bottom):
    bbox = font.getbbox(label)
    text_width = bbox[2] - bbox[0]
    text_height = bbox[3] - bbox[1]
    x = i * width + (width - text_width) // 2
    y = height + label_height + (label_height - text_height) // 2
    draw.text((x, y), label, fill=(0, 0, 0), font=font)

   # Paste images (shifted down by label_height)
  new_image.paste(gt_image, (0, label_height))
  new_image.paste(gs_vk_image, (width, label_height))
  new_image.paste(pbr_image_pcf, (width * 2, label_height))
  new_image.paste(pbr_image_shadow, (0, height + label_height * 2))
  new_image.paste(pbr_image, (width, height + label_height * 2))
  new_image.paste(rast_image, (width * 2, height + label_height * 2))
  new_image.save(os.path.join('./images', f"{model}.png"))
