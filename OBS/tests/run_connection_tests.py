"""Check production fresh defaults and migration of existing settings."""
from pathlib import Path
import os
import subprocess
import tempfile

root = Path(__file__).resolve().parent
code = (root.parent / 'src/plugin.cpp').read_text(encoding='utf-8')

def section(start, end):
    offset = code.index(start)
    return code[offset:code.index(end, offset)]

include = root / 'connection_defaults_under_test.inc'
try:
    include.write_text(section('static void defaults(obs_data_t *d)', '// All live source pointers') +
                       section('static obs_data_t *native_defaults()', 'static bool native_save('), encoding='utf-8')
    with tempfile.TemporaryDirectory() as temp:
        binary = str(Path(temp) / 'connection_defaults_test')
        subprocess.run(['g++', '-std=c++17', '-Wall', '-Wextra', '-Werror',
                        '-fsanitize=address,undefined', str(root / 'connection_defaults_test.cpp'),
                        '-o', binary], check=True)
        env = dict(os.environ)
        env.setdefault('ASAN_OPTIONS', 'detect_leaks=0')
        subprocess.run([binary], check=True, env=env)
finally:
    include.unlink(missing_ok=True)
print('PASS: universe 1 and automatic multicast on first use; existing universe, interface, network and dimensions preserved; missing legacy fields supplied without rewriting stored values')
