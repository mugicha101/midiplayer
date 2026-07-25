import os
import sys
import random
import subprocess

def main():
  script_path = "./a.exe" if sys.platform == "win32" else "./a.out"
  if not os.path.exists(script_path):
    print(f"Error: {script_path} not found. Please compile main.cpp first.")
    sys.exit(1)
    
  # find files
  path = "."
  songs = []
  if len(sys.argv) > 1:
    path = sys.argv[1]
  for root, _, files in os.walk(path, topdown=False):
    for name in files:
      file_path = os.path.join(root, name)
      with open(file_path, 'rb') as f:
        if f.read(4) == b'MThd':
          songs.append(file_path)
          
  # shuffle
  while True:
    random.shuffle(songs)
    for song in songs:
      print(f"Playing {os.path.basename(song)}")
      subprocess.run([script_path, song])

if __name__ == "__main__":
    main()