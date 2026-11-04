"""Run all shipped OBS test runners and grandMA3 tests; fail on the first error.

Requirements: Python 3.12, g++ with Address/UndefinedBehavior Sanitizer, Lua 5.4
shared library (Ubuntu: g++ liblua5.4-0). No OBS or grandMA3 installation needed.
"""
from pathlib import Path
import os
import subprocess
import sys

ROOT = Path(__file__).resolve().parent.parent


def main():
    env = dict(os.environ, PYTHONUTF8="1")
    runners = sorted((ROOT / "OBS/tests").glob("run_*tests.py"))
    if not runners:
        raise SystemExit("No OBS tests found")
    runners += [ROOT / "grandMA3/tests/run_tests.py"]
    for runner in runners:
        print(f"\n=== {runner.relative_to(ROOT)} ===", flush=True)
        subprocess.run([sys.executable, "-X", "utf8", str(runner)],
                       cwd=ROOT, env=env, check=True)
    subprocess.run([sys.executable, "-X", "utf8",
                    str(ROOT / "grandMA3/tools/build_plugin.py"), "--check"],
                   cwd=ROOT, env=env, check=True)
    print(f"\nPASS: {len(runners)} test runners and grandMA3 import consistency")


if __name__ == "__main__":
    main()
