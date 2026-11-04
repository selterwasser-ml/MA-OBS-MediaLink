"""Compile the actual production audio callback against a minimal OBS test double."""
from pathlib import Path
import os, subprocess, tempfile
root=Path(__file__).resolve().parent
code=(root.parent/'src/plugin.cpp').read_text()
a=code.index('static bool ma_audio_render(')
b=code.index('\n}\n',a)+2
inc=root/'audio_render_under_test.inc'
try:
    inc.write_text(code[a:b])
    with tempfile.TemporaryDirectory() as tmp:
        binary=str(Path(tmp)/'audio_test')
        subprocess.run(['g++','-std=c++17','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',str(root/'audio_mix_test.cpp'),'-o',binary],check=True)
        env=dict(os.environ);env.setdefault('ASAN_OPTIONS','detect_leaks=0')
        subprocess.run([binary],check=True,env=env)
    print('PASS: mixed modes, shared/duplicate/hidden sources, unmanaged fade, parent gain/mute/tracks, pending audio, timestamps, cycles, reference balance')
finally:
    inc.unlink(missing_ok=True)
