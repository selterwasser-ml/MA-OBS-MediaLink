"""Test production discovery, rule integration and gain math without an OBS device."""
from pathlib import Path
import os,subprocess,tempfile
root=Path(__file__).resolve().parent
code=(root.parent/'src/video_control.h').read_text()
def section(start,end):
    a=code.index(start);return code[a:code.index(end,a)]
files={
    'blackmagic_catalog_under_test.inc':section('static bool ma_media_source(', '#include "source_settings.h"')+
        section('// Recursive group/embedded-scene walk;', 'static std::vector<std::pair<QString, QString>> ma_video_scenes()'),
    'blackmagic_rules_under_test.inc':section('    void update_live_audio(', '    void manual(')
        +section('    void exit_action(', '    void stop_pilot(')
        +section('    void enter(', '    void update_live_audio(')
}
try:
    for name,content in files.items():(root/name).write_text(content)
    with tempfile.TemporaryDirectory() as temp:
        binary=str(Path(temp)/'blackmagic_test')
        subprocess.run(['g++','-std=c++17','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',str(root/'blackmagic_audio_test.cpp'),'-o',binary],check=True)
        env=dict(os.environ);env.setdefault('ASAN_OPTIONS','detect_leaks=0')
        subprocess.run([binary],check=True,env=env)
finally:
    for name in files:(root/name).unlink(missing_ok=True)
print('PASS: Blackmagic/audio-only rules plus actual media enter/exit/gain: 70/30 retarget without gain jump, frozen layers, 100 interruptions, completed fades, shared decoders, unstarted sources, manual Goto guard, off states and balanced references')
