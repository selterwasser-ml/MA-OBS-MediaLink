"""Exercise production source option backup/recovery with an OBS data API double."""
from pathlib import Path
import os, subprocess, tempfile
root=Path(__file__).resolve().parent
code=(root.parent/'src/video_control.h').read_text()
include=root/'prepare_under_test.inc'
try:
    start=code.index('static bool ma_video_apply_ffmpeg(')
    include.write_text(code[start:code.index('#include "media_actions.h"',start)])
    with tempfile.TemporaryDirectory() as temp:
        binary=str(Path(temp)/'restore_test')
        subprocess.run(['g++','-std=c++17','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',str(root/'source_settings_test.cpp'),'-o',binary],check=True)
        env=dict(os.environ);env.setdefault('ASAN_OPTIONS','detect_leaks=0')
        subprocess.run([binary],check=True,env=env)
finally:
    include.unlink(missing_ok=True)
print('PASS: 256 original/default combinations, first snapshot kept, repeated off/on, persisted recovery, unrelated edits/filters preserved, balanced references')
