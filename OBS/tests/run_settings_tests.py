"""Exercise production settings dependency/validation policy without Qt."""
from pathlib import Path
import os,subprocess,tempfile
root=Path(__file__).resolve().parent
with tempfile.TemporaryDirectory() as temp:
    binary=str(Path(temp)/'settings_test')
    subprocess.run(['g++','-std=c++17','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',
                    str(root/'settings_policy_test.cpp'),'-o',binary],check=True)
    env=dict(os.environ);env.setdefault('ASAN_OPTIONS','detect_leaks=0')
    subprocess.run([binary],check=True,env=env)
print('PASS: disabled/unselected/audio-only rules, all six start modes, unknown/known timing, active-only validation, FFmpeg/VLC capabilities, cached end-display preference and explicit Loop edits')
