import numpy as np
from scipy.spatial.transform import Rotation as R

# Define the rotation angle in degrees
# Generate 10 random rotation matrices

# Define the initial 4x4 matrix
initial_matrix = np.array([
	1, 0, 0, 0,
	0, 1, 0, 0,
	0, 0, 1, 0,
	0, 0, 0, 1
]).reshape(4, 4)

def randomise_model():

	# Store the resulting matrices
	result_matrices = []
	for _ in range(10):
		# Generate a random angle between 0 and 90 degrees
		angle_degrees = np.random.uniform(0, 90)
		
		# Convert the angle to radians
		angle_radians = np.radians(angle_degrees)
		
		# Generate a random axis (x, y, or z)
		axis = np.random.choice(['x', 'y', 'z'])
		
		# Create a rotation matrix using the from_euler method
		rotation_matrix_3x3 = R.from_euler(axis, angle_radians).as_matrix()
		
		# Convert the 3x3 rotation matrix to a 4x4 matrix
		rotation_matrix = np.eye(4)
		rotation_matrix[:3, :3] = rotation_matrix_3x3

		# Multiply the initial matrix by the rotation matrix
		# result_matrix = rotation_matrix @ initial_matrix
		# result_matrices.append(result_matrix.flatten())
		result_matrices.append(rotation_matrix.flatten())
		
		# Print the rotation matrix
	return result_matrices

# print(randomise_model(initial_matrix))