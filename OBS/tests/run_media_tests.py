"""Exercise production media dispatcher + actual Engine.manual against an OBS test double."""
from pathlib import Path
import os
import subprocess
import tempfile
root=Path(__file__).resolve().parent
code=(root.parent/'src/video_control.h').read_text()
a=code.index('#include "media_actions.h"')
b=code.index('\nstruct MAVideoEngine',a)
manual_start=code.index('    void manual(')
manual_end=code.index('    void tick(',manual_start)
command=root/'media_command_under_test.inc'
manual=root/'manual_under_test.inc'
try:
    command.write_text(code[a:b])
    manual.write_text(code[manual_start:manual_end])
    with tempfile.TemporaryDirectory() as tmp:
        binary=str(Path(tmp)/'media_test')
        subprocess.run(['g++','-std=c++17','-Wall','-Wextra','-Werror',
            '-fsanitize=address,undefined','-I',str(root.parent/'src'),
            str(root/'media_actions_test.cpp'),'-o',binary],check=True)
        env=dict(os.environ)
        env.setdefault('ASAN_OPTIONS','detect_leaks=0')
        subprocess.run([binary],check=True,env=env)
    print('PASS: stopped/ended Play, Toggle, pause/resume without rewind, Stop, restart, manual auto-start suppression, next-cue reset, observed playing state, missing sources and reference balance')
finally:
    command.unlink(missing_ok=True)
    manual.unlink(missing_ok=True)
