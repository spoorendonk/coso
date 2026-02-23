#!/usr/bin/env bash
# Download CVRP benchmark instances from CVRPLIB for testing.
#
# Usage:  ./tests/data/download_benchmarks.sh
#
# The script is idempotent: existing files are not re-downloaded.
# Instances are saved to the same directory as this script (tests/data/).

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# CVRPLIB base URL (Uchoa et al. X instances).
BASE_URL="http://vrp.atd-lab.inf.puc-rio.br/media/com_vrp/instances/Vrp-Set-X"

# Instances to download.  Each entry is "filename".
INSTANCES=(
    "X-n101-k25.vrp"
    "X-n106-k14.vrp"
    "X-n110-k13.vrp"
    "X-n120-k6.vrp"
    "X-n125-k30.vrp"
)

download() {
    local file="$1"
    local dest="${SCRIPT_DIR}/${file}"

    if [[ -f "$dest" ]]; then
        echo "  [skip] ${file} (already exists)"
        return 0
    fi

    echo "  [download] ${file} ..."
    if command -v curl &>/dev/null; then
        curl -fsSL -o "$dest" "${BASE_URL}/${file}"
    elif command -v wget &>/dev/null; then
        wget -q -O "$dest" "${BASE_URL}/${file}"
    else
        echo "ERROR: neither curl nor wget found" >&2
        exit 1
    fi
}

echo "Downloading CVRPLIB benchmark instances to ${SCRIPT_DIR}/ ..."
for inst in "${INSTANCES[@]}"; do
    download "$inst"
done
echo "Done."
