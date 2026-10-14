"""The conftest import guard tells "not built" from "stale or broken" (#227)."""

import os
import shutil
import subprocess
import sys
from pathlib import Path

import pytest

CONFTEST = Path(__file__).with_name("conftest.py")
PYTEST_ROOT = Path(pytest.__file__).resolve().parent.parent


def run_suite(tmp_path: Path, coso_files: dict[str, str]) -> subprocess.CompletedProcess[str]:
    """Run this conftest over a one-test suite, against a stub `coso` package.

    `-S` keeps site-packages and its editable-install hooks out, so the stub is
    the only `coso` the guard can see; pytest comes in through PYTHONPATH.
    """
    pkg = tmp_path / "coso"
    pkg.mkdir()
    for name, text in coso_files.items():
        (pkg / name).write_text(text)
    tests = tmp_path / "tests"
    tests.mkdir()
    shutil.copy(CONFTEST, tests / "conftest.py")
    (tests / "test_one.py").write_text("def test_one():\n    pass\n")
    env = dict(os.environ, PYTHONPATH=f"{tmp_path}{os.pathsep}{PYTEST_ROOT}")
    return subprocess.run(
        [sys.executable, "-S", "-m", "pytest", "-p", "no:cacheprovider", "tests"],
        cwd=tmp_path,
        env=env,
        capture_output=True,
        text=True,
    )


def test_missing_extension_skips_the_suite(tmp_path: Path) -> None:
    r = run_suite(tmp_path, {"__init__.py": "from coso._coso import __version__\n"})
    assert r.returncode == 5, r.stdout + r.stderr
    assert "Python bindings not built" in r.stdout


def test_stale_extension_fails_collection(tmp_path: Path) -> None:
    r = run_suite(
        tmp_path,
        {
            "__init__.py": "from coso._coso import __version__, NewBinding\n",
            "_coso.py": "__version__ = '0'\n",
        },
    )
    assert r.returncode not in (0, 5), r.stdout + r.stderr
    assert "stale or broken" in r.stdout + r.stderr


def test_unrelated_missing_module_fails_collection(tmp_path: Path) -> None:
    r = run_suite(
        tmp_path,
        {
            "__init__.py": "import coso_no_such_dependency\n",
        },
    )
    assert r.returncode not in (0, 5), r.stdout + r.stderr
    assert "coso_no_such_dependency" in r.stdout + r.stderr
