import json
import os
import shutil
import numpy as np
import sys
import random

model = sys.argv[1]

with open(f'/home/nisarg/data/{model}/dataset/transforms_train.json', 'r') as f:
  data = json.load(f)
  print(data.keys())
  cam = data['camera_angle_x']
  frms = data['frames']
  random.shuffle(frms)
  train = frms[:int(0.8*len(frms))]
  test = frms[int(0.8*len(frms)):]
  os.rename(f'/home/nisarg/data/{model}/dataset/transforms_train.json', f'/home/nisarg/data/{model}/dataset/transforms_train_full.json')
  with open(f'/home/nisarg/data/{model}/dataset/transforms_train.json', 'w') as f:
    json.dump({'camera_angle_x': cam, 'frames': train}, f, indent=4)
  os.mkdir(f'/home/nisarg/data/{model}/dataset/test')
  for frm in test:
    img_name = frm['file_path'].split('/')[-1]
    shutil.move(f'/home/nisarg/data/{model}/dataset/train/{img_name}.png', f'/home/nisarg/data/{model}/dataset/test/{img_name}.png')
    frm['file_path'] = f'test/{img_name}'
  with open(f'/home/nisarg/data/{model}/dataset/transforms_test.json', 'w') as f:
    json.dump({'camera_angle_x': cam, 'frames': test}, f, indent=4)
  