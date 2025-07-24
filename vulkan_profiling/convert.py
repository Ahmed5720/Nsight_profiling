import os
from concurrent.futures import ProcessPoolExecutor
from PIL import Image
from rich.progress import Progress
FOLDER_PATH = "."

def process_image(filepath: str) -> None:
  """
  Reads a PNG image, multiplies each pixel's RGB by alpha, 
  sets alpha to fully opaque (255), then overwrites the file.
  """
  with Image.open(filepath).convert("RGBA") as img:
    pixels = img.getdata()
    new_pixels = []

    for (r, g, b, a) in pixels:
      alpha = a / 255.0
      nr = int(r * alpha)
      ng = int(g * alpha)
      nb = int(b * alpha)
      new_pixels.append((nr, ng, nb, 255))

    new_img = Image.new("RGBA", img.size)
    new_img.putdata(new_pixels)
    parent_dir = os.path.dirname(filepath)
    opaque_dir = os.path.join(parent_dir, "opaque")
    os.makedirs(opaque_dir, exist_ok=True)
    new_filepath = os.path.join(opaque_dir, os.path.basename(filepath))
    new_img.save(new_filepath)

def main():
  # Collect all PNG files in the specified folder
  with Progress() as progress:
    task = progress.add_task("[cyan]Processing nodes...", total=len(os.listdir("/home/nisarg/data/DTC")))
    for node in os.listdir("/home/nisarg/data/DTC"):
      node = node.strip()
      print(node)
      FOLDER_PATH = os.path.join("/home/nisarg/data/DTC", node)
      if os.path.isdir(FOLDER_PATH):
        test_dir = os.path.join(FOLDER_PATH,'nerf_data','test')
        png_files = [
          os.path.join(test_dir, f)
          for f in os.listdir(test_dir)
          if f.lower().endswith(".png")
        ]
        print(png_files,os.path.join(FOLDER_PATH,'nerf_data','test'))
        # Process images in parallel
        with ProcessPoolExecutor() as executor:
          executor.map(process_image, png_files)
      progress.update(task, advance=1)

if __name__ == "__main__":
        main()