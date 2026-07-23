import os
import sys
import random
import subprocess

def main():
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
      subprocess.run(["./a.out", song])

if __name__ == "__main__":
    main()