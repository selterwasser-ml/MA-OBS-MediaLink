"""Test actual saved-rule publication and scene-membership reconciliation without OBS/Qt."""
from pathlib import Path
import os
import subprocess
import tempfile
root=Path(__file__).resolve().parent
code=(root.parent/'src/video_control.h').read_text()
def section(start,end):
    a=code.index(start)
    return code[a:code.index(end,a)]
content=section('    MAVideoRule get_committed(', '    void set(')
content+=section('    bool save() {','    bool shared_in_scene(')
content+=section('    void sync_scene_membership(', '    void enter(')
include=root/'membership_under_test.inc'
try:
    include.write_text(content)
    with tempfile.TemporaryDirectory() as temp:
        binary=str(Path(temp)/'membership_test')
        subprocess.run(['g++','-std=c++17','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',
            str(root/'membership_test.cpp'),'-o',binary],check=True)
        env=dict(os.environ)
        env.setdefault('ASAN_OPTIONS','detect_leaks=0')
        subprocess.run([binary],check=True,env=env)
    print('PASS: scene removal/addition, enabled rules only, draft/failed-save isolation, preserved running/manual state, duplicate/renamed entries, shared ownership and disabled rules')
finally:
    include.unlink(missing_ok=True)
