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
COMPILE_DB_FILE=""
if [ -f "build/compile_commands.json" ]; then
	COMPILE_DB="-p build"
	COMPILE_DB_FILE="build/compile_commands.json"
elif [ -f "compile_commands.json" ]; then
	COMPILE_DB="-p ."
	COMPILE_DB_FILE="compile_commands.json"
fi

# Whether the compilation database has an entry for a path. Lets the hooks tell
# "this file is not built in this configuration" apart from "clang-tidy could
# not parse it", which otherwise look identical and let a wholly broken lint
# setup report a clean tree.
in_compile_db() {
	[ -n "$COMPILE_DB_FILE" ] || return 1
	python3 - "$COMPILE_DB_FILE" "$1" <<-'PY'
		import json, os, sys
		db, target = sys.argv[1], os.path.realpath(sys.argv[2])
		try:
		    entries = json.load(open(db))
		except Exception:
		    sys.exit(1)
		for e in entries:
		    f = e.get("file", "")
		    if not os.path.isabs(f):
		        f = os.path.join(e.get("directory", ""), f)
		    if os.path.realpath(f) == target:
		        sys.exit(0)
		sys.exit(1)
	PY
}

# Translation units that include the given headers, so a header-only change is
# still analysed. Restricted to files git tracks: the compile database also
# contains the fetched dependencies' own sources (Catch2 builds from .cpp), and
# without this filter a header-only change drags third-party translation units
# into the lint run and reports findings that are not ours to fix.
tus_including() {
	local headers="$1"
	[ -n "$COMPILE_DB_FILE" ] || return 0
	python3 - "$COMPILE_DB_FILE" <<-PY | grep -Fxf <(git ls-files '*.cpp' '*.cc' '*.cxx') || true
		import json, os, re, sys
		headers = [h for h in """$headers""".split() if h]
		try:
		    entries = json.load(open(sys.argv[1]))
		except Exception:
		    sys.exit(0)
		bases = {os.path.basename(h) for h in headers}
		pat = re.compile(r'#\s*include\s*[<"]([^>"]+)[>"]')
		root = os.getcwd()
		for e in entries:
		    f = e.get("file", "")
		    if not os.path.isabs(f):
		        f = os.path.join(e.get("directory", ""), f)
		    try:
		        src = open(f, encoding="utf-8", errors="replace").read()
		    except OSError:
		        continue
		    if any(os.path.basename(m) in bases for m in pat.findall(src)):
		        rel = os.path.relpath(f, root)
		        if not rel.startswith(".."):
		            print(rel)
	PY
}

# Shell scripts are matched by shebang as well as extension: the hooks in
# .githooks/ carry no .sh suffix, and without this they would be the only shell
# in the repo exempt from the shfmt and shellcheck gates they impose on it.
shell_files() {
	local f
	while IFS= read -r f; do
		[ -n "$f" ] || continue
		case "$f" in
		*.sh | *.bash)
			echo "$f"
			continue
			;;
		esac
		[ -f "$f" ] || continue
		if head -c 128 "$f" 2>/dev/null | head -1 | grep -qE '^#!.*\b(ba)?sh\b'; then
			echo "$f"
		fi
	done <<<"$1"
}
