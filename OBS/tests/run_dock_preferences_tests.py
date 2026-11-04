"""Check production dock preference migration, independent toggles and safe writes."""
from pathlib import Path
import os,subprocess,tempfile
root=Path(__file__).resolve().parent
code=(root.parent/'src/plugin.cpp').read_text()
a=code.index('static bool ma_read_monitor_preference(')
b=code.index('static QString ma_audio_diagnostic_text();',a)
monitor=(root.parent/'src/monitor_ui.h').read_text()
x=monitor.index('    void set_diagnostics_visible(')
y=monitor.index('    void refresh_scene_list()',x)
files={'dock_preferences_under_test.inc':code[a:b],
       'dock_visibility_under_test.inc':monitor[x:y]}
try:
    for name,content in files.items():(root/name).write_text(content)
    with tempfile.TemporaryDirectory() as temp:
        binary=str(Path(temp)/'dock_preferences')
        subprocess.run(['g++','-std=c++17','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',
                        str(root/'dock_preferences_test.cpp'),'-o',binary],check=True)
        env=dict(os.environ);env.setdefault('ASAN_OPTIONS','detect_leaks=0')
        subprocess.run([binary],check=True,env=env)
finally:
    for name in files:(root/name).unlink(missing_ok=True)
print('PASS: independent dock visibility in all four combinations, reopen persistence, old-file defaults, unchanged scroll state on toggle, unknown keys retained, diagnostics-only isolation, write/path failures and balanced ownership')
