"""Exercise the actual SDL application, including its keyboard event handlers."""
import os
import pathlib
import subprocess
import sys

executable = pathlib.Path(sys.argv[1]).resolve()
output = pathlib.Path(sys.argv[2]).resolve()
assert executable.is_file(), "the playable C++ application must exist"
env = dict(os.environ, SDL_VIDEODRIVER="dummy", SDL_AUDIODRIVER="dummy")
result = subprocess.run([str(executable), "--smoke-test", str(output)], env=env,
                        capture_output=True, text=True, timeout=90)
print(result.stdout)
print(result.stderr, file=sys.stderr)
assert result.returncode == 0, "SDL application input and rendering checks failed"
assert "SMOKE PASS" in result.stdout
for name in ("title", "manual", "dialogue", "gameplay", "paused", "cirno", "sakuya", "yukari"):
    image = output / (name + ".bmp")
    assert image.is_file() and image.stat().st_size > 1_000_000, name

# Audio is optional even when the SDL backend itself cannot initialize.
env["SDL_AUDIODRIVER"] = "scarlet-nonexistent-driver"
silent = subprocess.run([str(executable), "--smoke-test", str(output / "silent")],
                        env=env, capture_output=True, text=True, timeout=90)
assert silent.returncode == 0 and "SMOKE PASS" in silent.stdout, silent.stderr
assert "continuing silently" in silent.stderr
print("Silent launch PASS: unavailable audio does not prevent play.")
