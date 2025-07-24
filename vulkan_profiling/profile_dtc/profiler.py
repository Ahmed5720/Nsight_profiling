import sys
import os
from helper import Helper
import numpy as np
import argparse
import pandas as pd
# from config_gen import randomise_model

# helper = Helper(base_path="/home/nisarg/data/",model='trex')

def run_profiler(helper:Helper,vm, pm, mm, app='3dgs',light_strength=3.0, ambient_strength=0.01):
	# vms = open(vm_file, "r").readlines()
	# pms = open(pm_file, "r").readlines()
	# mms = open(mm_file, "r").readlines()
	if not os.path.exists(f"run_scripts/{app}"):
		os.makedirs(f"run_scripts/{app}")
	fil = open(f"run_scripts/{app}/{helper.model}_{app}.sh", "w")
	counter = 0
	for key in vm.keys():
		cmd = helper.get_ngfx_cmd(app=app, vm=np.array(vm[key]), pm=pm, mm=mm,counter=counter,light_strength=light_strength, ambient_strength=ambient_strength)
		counter += 1
		fil.write(cmd + '\n')
	fil.close()

def run_psnr_exec(helper:Helper, vm, pm, mm, app='3dgs', print_commands=False, light_strength=3.0, ambient_strength=0.01):
	for key in vm.keys():
		fname = key.split('/')[-1].split('.')[0]
		cmd = helper.get_exec_cmd(app=app, vm=np.array(vm[key]), pm=pm, mm=mm, fil=fname, light_strength=light_strength, ambient_strength=ambient_strength)
		if (print_commands==False):
			os.system(cmd)
		else:
			print(cmd)
	

def profile(model='trex',scale=1.0, app='3dgs',run='test', print_commands=False,light_strength=3.0, ambient_strength=0.01):
	helper = Helper(model=model)
	helper.convert_matrices()
	helper.read_cam()
	proj = helper.get_proj()
	view = helper.get_view()
	mm = np.identity(4) * scale
	rotation_x = np.array([
		[1, 0, 0, 0],
		[0, 0, -1, 0],
		[0, 1, 0, 0],
		[0, 0, 0, 1]
	])
	mm = rotation_x @ mm
	mm = np.transpose(mm)
	mm = mm.flatten()
	if run == 'profile':
		run_profiler(helper, view, proj, mm, app,light_strength=light_strength, ambient_strength=ambient_strength)
	elif run == 'psnr':
		run_psnr_exec(helper,view, proj, mm, app, print_commands=print_commands,light_strength=light_strength, ambient_strength=ambient_strength)
	elif run == 'calibrate':
		run_psnr_exec(helper,view, proj, mm, app, print_commands=print_commands,light_strength=light_strength, ambient_strength=ambient_strength)
	else:
		raise Exception("Invalid run type")

def run(run='psnr', app='3dgs', model=None, print_commands=False):
	if run == 'calibrate':
		# randomise_model(model=model, app=app)
		print("Calibrating model...")
		l_strengths = [1.0, 1.5, 2.0, 2.5, 3.0,3.5, 4.0, 4.5, 5.0]
		a_strengths = [0.01, 0.02, 0.03, 0.04, 0.05, 0.1, 0.15, 0.2, 0.25]
		with open("test_scenes",'r') as f:
			nodes = f.readlines()
			nodes = [x.strip() for x in nodes]
			for node in nodes:
				node = node.strip()
				if os.path.isdir(os.path.join("/home/nisarg/data/DTC",node)):
					for l_strength in l_strengths:
						for a_strength in a_strengths:
							print(f"Calibrating {node} with light strength {l_strength} and ambient strength {a_strength}")
							profile(model=node, app='pbr_cal', run='calibrate', print_commands=print_commands, light_strength=l_strength, ambient_strength=a_strength)
		print("Calibration done!")
		return

	models_db = pd.read_csv("best_calibration_psnr.csv", index_col=0)
	# print(models_db)
	# print(models_db.index,models_db.columns)
	# exit()
	if model is not None:
		ambient_strength = 0.01
		light_strength = 3.0
		if model not in models_db.index:
			print(f"Model {model} not found in database. Using default values.")
		else:
			ambient_strength = models_db.loc[model, 'ambient_strength']
			light_strength = models_db.loc[model, 'light_strength']
			print(f"Using ambient strength {ambient_strength} and light strength {light_strength} for model {model}")
		profile(model=model, app=app, run=run, print_commands=print_commands, light_strength=light_strength, ambient_strength=ambient_strength)
		return
	#run the profiler
	done_test_writer = open(f"done_test_{app}_{run}.txt", "w")
	done_test = []
	try:
		done_test = open(f"done_test_{app}_{run}.txt", "r").readlines()
	except:
		done_test = []
	done_test = [x.strip() for x in done_test]
	# done_test = set(done_test)
	print(done_test)
	# exit()
	nodes = []
	with open("test_scenes",'r') as f:
		nodes = f.readlines()
	nodes = [x.strip() for x in nodes]
	for node in nodes:
		node = node.strip()
		ambient_strength = 0.01
		light_strength = 3.0
		if node in models_db.index:
			ambient_strength = models_db.loc[node, 'ambient_strength']
			light_strength = models_db.loc[node, 'light_strength']
		if os.path.isdir(os.path.join("/home/nisarg/data/DTC",node)) and node not in done_test:
			profile(model=node, app=app, run=run, print_commands=print_commands, light_strength=light_strength, ambient_strength=ambient_strength)
			# profile(model=node, app='rast', run=run)
			done_test_writer.write(node + '\n')
			# break
	done_test_writer.close()
	# profile(model='monitor',scale=0.01, app='rast', run='psnr')
	# profile(model='monitor',scale=0.01, app='3dgs', run='psnr')
	# profile(model='viking_ship', scale=0.005, app='rast', run='psnr')
	# profile(model='viking_ship', scale=1, app='3dgs', run='psnr')

	#run the profiler
	# profile(model='trex', app='rast', run='profile')
	# profile(model='trex', app='3dgs', run='profile')
	# profile(model='amber', app='rast', run='profile')
	# profile(model='amber', app='3dgs', run='profile')
	# profile(model='monitor', app='rast', run='profile')	
	# profile(model='monitor', app='3dgs', run='profile')	
	# profile(model='viking_ship', scale=0.005, app='rast', run='profile')
	# profile(model='viking_ship',scale=1, app='3dgs', run='profile')

if __name__ == "__main__":
	parser = argparse.ArgumentParser(description='Profiler for DTC')
	parser.add_argument('-r','--run', type=str, default='psnr', help='run type: profile or psnr or calibrate')
	parser.add_argument('-a','--app', nargs='+', help='application type: pbr, pbr_shadow, pbr_pcf, rast, 3dgs',required=True)
	parser.add_argument('-p','--print', action='store_true', help='print the command instead of executing it')
	parser.add_argument('-m','--model', type=str, help='model name',required=False)
	args = parser.parse_args()
	print(args.run)
	print(args.app)
	print(args.model)
	# exit()
	for app in args.app:
		run(run=args.run, app=app, model=args.model, print_commands=args.print)