import sys
import os
from helper_b import Helper
import numpy as np
from config_gen import randomise_model

# helper = Helper(base_path="/home/nisarg/data/",model='trex')

def run_profiler(helper:Helper,vm, pm, mm, app='vkgs'):
	# vms = open(vm_file, "r").readlines()
	# pms = open(pm_file, "r").readlines()
	# mms = open(mm_file, "r").readlines()
	fil = open(f"{helper.model}_{app}.sh", "w")
	
	# for i in range(len(mms)):
	# 	temp_mm = mm.reshape(4, 4)
	# 	temp_i = mms[i].reshape(4, 4)
	# 	mm_cur = temp_i @ temp_mm
	# 	cmd = helper.get_ngfx_cmd(model, app=app, vm=vm, pm=pm, mm=mm_cur.flatten())
	# 	print(cmd)
	# 	fil.write(cmd + '\n')
		# os.system(cmd)
	for key in vm.keys():
		cmd = helper.get_ngfx_cmd(app=app, vm=np.array(vm[key]), pm=pm, mm=mm)
		fil.write(cmd + '\n')
	fil.close()

def run_psnr_exec(helper:Helper, vm, pm, mm, app='vkgs'):
	for key in vm.keys():
		fname = key.split('/')[-1].split('.')[0]
		cmd = helper.get_exec_cmd(app=app, vm=np.array(vm[key]), pm=pm, mm=mm, fil=fname)
		# os.system(cmd)
		print(cmd)
	

def profile(model='trex',scale=1.0, app='vkgs',run='profile'):
	helper = Helper(model=model)
	helper.convert_matrices()
	helper.read_cam()
	proj = helper.get_proj()
	view = helper.get_view()
	mm = np.identity(4) * scale
	mm[3][3] = 1.0
	mm = mm.flatten()
	if run == 'profile':
		run_profiler(helper, view, proj, mm, app)
	elif run == 'psnr':
		run_psnr_exec(helper,view, proj, mm, app)
	else:
		raise Exception("Invalid run type")

def run():
	#save render results
	# profile(model='trex', app='rast', run='psnr')
	# profile(model='trex', app='vkgs', run='psnr')
	# profile(model='amber', app='rast', run='psnr')
	# profile(model='amber', app='vkgs', run='psnr')
	# profile(model='monitor',scale=0.01, app='rast', run='psnr')
	# profile(model='monitor',scale=0.01, app='vkgs', run='psnr')
	profile(model='viking_ship', scale=0.005, app='rast', run='psnr')
	# profile(model='viking_ship', scale=1, app='vkgs', run='psnr')

	#run the profiler
	# profile(model='trex', app='rast', run='profile')
	# profile(model='trex', app='vkgs', run='profile')
	# profile(model='amber', app='rast', run='profile')
	# profile(model='amber', app='vkgs', run='profile')
	# profile(model='monitor', app='rast', run='profile')	
	# profile(model='monitor', app='vkgs', run='profile')	
	# profile(model='viking_ship', scale=0.005, app='rast', run='profile')
	# profile(model='viking_ship',scale=1, app='vkgs', run='profile')

if __name__ == "__main__":
	run()