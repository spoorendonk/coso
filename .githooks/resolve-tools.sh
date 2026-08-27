#!/bin/bash
# Resolves each dev tool to the project venv when there is one, else to PATH.
# Sourced by pre-commit and pre-push. Each variable is empty when that tool is
# unavailable, and every caller reports the skip rather than passing in silence.

# Everything set here is read by the sourcing hook, not by this file.
# shellcheck disable=SC2034

VENV_BIN=""
for d in .venv venv; do
	if [ -d "$d/bin" ]; then
		VENV_BIN="$d/bin"
		break
	fi
done

resolve_tool() {
	local name="$1"
	if [ -n "$VENV_BIN" ] && [ -x "$VENV_BIN/$name" ]; then
		echo "$VENV_BIN/$name"
	elif command -v "$name" &>/dev/null; then
		echo "$name"
	else
		echo ""
	fi
}

CLANG_FORMAT=$(resolve_tool clang-format)
CLANG_TIDY=$(resolve_tool clang-tidy)
RUFF=$(resolve_tool ruff)
MYPY=$(resolve_tool mypy)
SHELLCHECK=$(resolve_tool shellcheck)
SHFMT=$(resolve_tool shfmt)

# clang-tidy needs a compilation database to parse a translation unit.
COMPILE_DB=""
if [ -f "build/compile_commands.json" ]; then
	COMPILE_DB="-p build"
elif [ -f "compile_commands.json" ]; then
	COMPILE_DB="-p ."
fi
