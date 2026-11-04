"""Run the production refresh method with widget/OBS test doubles, no Qt runtime.
Lock guards are instrumented so every widget setter asserts that no lock is held.
"""
from pathlib import Path
import os, subprocess, tempfile
root=Path(__file__).resolve().parent
code=(root.parent/'src/monitor_ui.h').read_text()
a=code.index('    void refresh() {');b=code.index('\npublic:',a)
inc=root/'monitor_refresh_under_test.inc'
try:
    inc.write_text(code[a:b].replace('std::lock_guard<std::mutex>', 'TestGuard'))
    with tempfile.TemporaryDirectory() as temp:
        binary=str(Path(temp)/'monitor_test')
        subprocess.run(['g++','-std=c++17','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',str(root/'monitor_refresh_test.cpp'),'-o',binary],check=True)
        env=dict(os.environ);env.setdefault('ASAN_OPTIONS','detect_leaks=0')
        subprocess.run([binary],check=True,env=env)
    print('PASS: no intermediate off/zero during repeated/fading snapshots, unchanged values not rewritten, real off/wait/lost states, all setters outside OBS locks')
finally:
    inc.unlink(missing_ok=True)
