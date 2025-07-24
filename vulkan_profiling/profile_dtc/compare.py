import cv2
import numpy as np

# Paths to the images
img1_path = '/home/nisarg/plane.png'
img2_path = '/home/nisarg/plane_pbr.png'

# Read images in RGB
img1 = cv2.imread(img1_path, cv2.IMREAD_COLOR)
img2 = cv2.imread(img2_path, cv2.IMREAD_COLOR)

if img1 is None or img2 is None:
	raise FileNotFoundError("One or both image files not found.")

if img1.shape != img2.shape:
	raise ValueError("Images must have the same dimensions.")

# Convert from BGR (OpenCV default) to RGB
img1 = cv2.cvtColor(img1, cv2.COLOR_BGR2RGB)
img2 = cv2.cvtColor(img2, cv2.COLOR_BGR2RGB)

# Subtract images
diff = cv2.absdiff(img1, img2)

# Get the middle pixel
mid_y = diff.shape[0] // 2
mid_x = diff.shape[1] // 2
middle_pixel = diff[mid_y, mid_x]

# Compare the sum of RGB values at the middle pixel for both images
sum_img1 = np.sum(img1[mid_y, mid_x])
sum_img2 = np.sum(img2[mid_y, mid_x])

if sum_img1 > sum_img2:
	bigger_img = "img1"
elif sum_img2 > sum_img1:
	bigger_img = "img2"
else:
	bigger_img = "equal"

print(f"Middle pixel is bigger in: {bigger_img}")

print("Middle pixel value of the difference (RGB):", middle_pixel)