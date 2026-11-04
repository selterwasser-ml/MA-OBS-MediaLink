"""Exercise production gain filters/lifecycle against an OBS API test double."""
from pathlib import Path
import os, subprocess, tempfile
root=Path(__file__).resolve().parent
with tempfile.TemporaryDirectory() as temp:
    binary=str(Path(temp)/'gain_test')
    subprocess.run(['g++','-std=c++17','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',str(root/'audio_gain_test.cpp'),'-o',binary],check=True)
    env=dict(os.environ);env.setdefault('ASAN_OPTIONS','detect_leaks=0')
    subprocess.run([binary],check=True,env=env)
print('PASS: saved zero starts neutral, live zero/fade/full gain, save leaves live gain intact, stale/duplicate cleanup, user filters preserved, ownership balanced')
