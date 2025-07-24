import sys
import json
import os
import matplotlib.pyplot as plt

def get_accessor_count(accessor):
    return accessor.get("count", 0)

def load_gltf(filename):
    with open(filename, "r") as f:
        gltf = json.load(f)
    return gltf

def count_vertices(gltf_path):
    gltf = load_gltf(gltf_path)
    total_vertices = 0
    for mesh in gltf.get("meshes", []):
        for primitive in mesh.get("primitives", []):
            pos_accessor_idx = primitive["attributes"].get("POSITION")
            if pos_accessor_idx is not None:
                accessor = gltf["accessors"][pos_accessor_idx]
                count = get_accessor_count(accessor)
                total_vertices += count
    return total_vertices

def count_points_ply(ply_path):
    if not os.path.exists(ply_path):
        return 0
    num_points = 0
    with open(ply_path, "r", encoding="utf-8", errors="ignore") as f:
        for line in f:
            if line.startswith("element vertex"):
                num_points = int(line.split()[-1])
            if line.strip() == "end_header":
                break
    return num_points

if __name__ == "__main__":
    base_path = "/home/nisarg/data/DTC"
    test_scenes_file = "test_scenes"

    if not os.path.exists(test_scenes_file):
        print(f"Test scenes file '{test_scenes_file}' not found.")
        sys.exit(1)

    with open(test_scenes_file, "r") as f:
        scenes = [line.strip() for line in f if line.strip()]

    scene_names = []
    vertices_counts = []
    points_counts = []

    for scene in scenes:
        gltf_path = os.path.join(base_path, scene, f"{scene}.gltf")
        ply_path = os.path.join(base_path, scene, "point_cloud/iteration_30000/point_cloud.ply")
        if not os.path.exists(gltf_path):
            print(f"{gltf_path} not found.")
            continue
        num_vertices = count_vertices(gltf_path)
        num_points = count_points_ply(ply_path)
        scene_names.append(scene)
        vertices_counts.append(num_vertices)
        points_counts.append(num_points)
        print(f"{scene}: {num_vertices} vertices, {num_points} points")

    # Plotting
    plt.figure(figsize=(10, 5))
    plt.plot(scene_names, vertices_counts, marker='o', label='Vertices in Mesh')
    plt.plot(scene_names, points_counts, marker='s', label='Points in Point Cloud')
    plt.xticks(rotation=90)  # Make x tick labels vertical
    plt.xlabel('Scene')
    plt.ylabel('Count')
    plt.title('Vertices vs Point Cloud Points per Scene')
    plt.ylim(0, 500_000)  # Cap y axis to 500k
    plt.legend()
    plt.tight_layout()
    plt.savefig("vertices_vs_points.png")