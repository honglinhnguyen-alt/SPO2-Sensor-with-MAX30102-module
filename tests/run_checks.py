#!/usr/bin/env python3
"""Run host-side simulations. This does not compile for an Arduino board."""
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

root = Path(__file__).resolve().parents[1]
for program in ("g++", "node"):
    if not shutil.which(program):
        raise SystemExit(f"Required host tool not found: {program}")

subprocess.run([sys.executable, "-m", "unittest", "discover", "-s", "tests",
                "-p", "test_*.py", "-v"], cwd=root, check=True)
with tempfile.TemporaryDirectory(prefix="max30102-check-") as directory:
    temporary = Path(directory)
    executable = temporary / "check"
    dashboard = temporary / "dashboard.html"
    subprocess.run(["g++", "-std=c++11", "-Wall", "-Wextra", "-Werror",
                    "-I", str(root / "tests" / "native"),
                    str(root / "tests" / "native" / "check.cpp"),
                    "-o", str(executable)], cwd=root, check=True)
    subprocess.run([str(executable), str(dashboard)], cwd=root, check=True)
    subprocess.run(["node", str(root / "tests" / "frontend-check.js"),
                    str(dashboard)], cwd=root, check=True)

print("All host-side checks passed. Physical sensor/Wi-Fi testing remains separate.")
