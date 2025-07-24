# Mesh And 3DGS Profiling

This repo contains the implementation to profile the mesh and 3dgs implementation. Start with building the vulkan models. Clone the repo by using the command
```bash
git clone https://github.com/horizon-research/vulkan_profiling.git --recursive
```

## Build Pipeline

### Interpolation Pipeline

```bash
cd rast_pipeline
cmake . -B build
cmake --build build --config Release -j
```

### PBR Pipeline

```bash
cd pbr_pipeline
cmake . -B build
cmake --build build --config Release -j
```

### 3DGS

```bash
cd vk_gaussian_splatting
cmake . -B build
cmake --build build --config Release -j
```

## Running the Code

Executable paths for the different pipelines are as follows:

- Interpolation: `./rast_pipeline/build/rasterizer`
- PBR: `./pbr_pipeline/build/pbr_viewer`
- 3dgs: `./vk_gaussian_splatting/bin_x64/Release/vk_gaussian_splatting_app`

Each pipeline takes the a set of common commandline arguments:


- `-i, --input`: Path to input model. Gltf file for mesh pipelines and ply file for 3dgs.
- `-v, --view`: Flattened 4x4 view matrix.
- `-p, --proj`: Flattened 4x4 projection matrix.
- `-o, --output`: Path to output image. Will save the image to disk and terminate the window. Optional argument.


Mesh pipelines also take a flattened 4x4 model matrix using flags `-m, --model`.

Pbr pipelines take the following extra commanfline arguments:

- `-S, --shadow`: Enable shadow mapping. This is an empty argument. Default value is false.
- `-P, --pcf`: Enable PCF in shadow mapping.This is an empty argument. Default value is false.
- `-L, --light`: Strength of the light sources. Floating point value.
- `-A, --ambient`: Strength of the Ambient light. Floating point value.

## Running the Profiler

The directory `./profile_dtc` contains python files to perform automated testing on models from DTC dataset. There are two main functions of these script:

- Run the pipelines on all or specified models and save rendered frame to disk.
- Generate shell script to run nsight graphics on the models.

Use the command `python profiler.py --help` to learn more.