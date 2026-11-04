"""Run production stop/exit/event/timer code against a strict OBS/Qt lifecycle double."""
from pathlib import Path
import os, subprocess, tempfile
root=Path(__file__).resolve().parent
plugin=(root.parent/'src/plugin.cpp').read_text()
video=(root.parent/'src/video_control.h').read_text()
def section(code,start,end):
    a=code.index(start)
    return code[a:code.index(end,a)]
files={
    'stop_under_test.inc':section(video,'    void reset_runtime() {','    void load() {')+
        section(video,'    void stop_pilot() {','    void add_pending(')+
        section(video,'    void manual(', '    void tick('),
    'lifecycle_under_test.inc':section(plugin,'static void pilot_stop(void) {','static bool pilot_start(bool interactive) {')+
        section(plugin,'static void schedule_collection_ready(', '\n}\n\nbool obs_module_load')+'\n'+
        plugin[plugin.index('void obs_module_unload(void) {'):]
}
try:
    for name,content in files.items(): (root/name).write_text(content)
    with tempfile.TemporaryDirectory() as temp:
        binary=str(Path(temp)/'lifecycle_test')
        subprocess.run(['g++','-std=c++17','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',str(root/'control_lifecycle_test.cpp'),'-o',binary],check=True)
        env=dict(os.environ);env.setdefault('ASAN_OPTIONS','detect_leaks=0')
        subprocess.run([binary],check=True,env=env)
finally:
    for name in files: (root/name).unlink(missing_ok=True)
print('PASS: no off-mode commands, no exit-rule Stop on handover, showing refs released, startup recovery once, early shutdown, stale timers blocked, no OBS calls after core teardown')
