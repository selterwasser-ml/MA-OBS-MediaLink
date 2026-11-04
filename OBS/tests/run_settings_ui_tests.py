"""Compile the actual Qt availability-binding lambda against strict widget doubles."""
from pathlib import Path
import os,subprocess,tempfile
root=Path(__file__).resolve().parent
code=(root.parent/'src/video_control.h').read_text()
start=code.index('    auto update_rule_state=[&] {')
end=code.index('\n    auto populate_rule=',start)
body=code[start:end].replace('    auto update_rule_state=[&] {','    void refresh() {',1)
include=root/'settings_ui_under_test.inc'
try:
    include.write_text(body)
    with tempfile.TemporaryDirectory() as temp:
        binary=str(Path(temp)/'settings_ui_test')
        subprocess.run(['g++','-std=c++17','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',
                        str(root/'settings_ui_test.cpp'),'-o',binary],check=True)
        env=dict(os.environ);env.setdefault('ASAN_OPTIONS','detect_leaks=0')
        subprocess.run([binary],check=True,env=env)
finally:
    include.unlink(missing_ok=True)
print('PASS: actual settings UI bindings, disabled labels/reasons, start-dependent fields, validation feedback, preserved inactive values, signal-blocked presentation changes, FFmpeg/VLC/live-only controls and no-source handling')
