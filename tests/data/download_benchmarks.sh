#!/usr/bin/env bash
# Download CVRP and VRPTW benchmark instances for testing.
#
# Usage:  ./tests/data/download_benchmarks.sh
#
# The script is idempotent: existing files are not re-downloaded.
# Instances are saved to the same directory as this script (tests/data/).

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# CVRPLIB base URL (Uchoa et al. X instances).
CVRP_BASE_URL="http://vrp.atd-lab.inf.puc-rio.br/media/com_vrp/instances/Vrp-Set-X"

# CVRP instances to download.
CVRP_INSTANCES=(
    "X-n101-k25.vrp"
    "X-n106-k14.vrp"
    "X-n110-k13.vrp"
    "X-n120-k6.vrp"
    "X-n125-k30.vrp"
)

# Solomon VRPTW instances (from the Sintef TOP website).
SOLOMON_BASE_URL="https://www.sintef.no/globalassets/project/top/vrptw/solomon"
SOLOMON_INSTANCES=(
    "C101.txt"
    "C102.txt"
    "R101.txt"
    "R102.txt"
    "RC101.txt"
    "RC102.txt"
)

download() {
    local url="$1"
    local file="$2"
    local dest="${SCRIPT_DIR}/${file}"

    if [[ -f "$dest" ]]; then
        echo "  [skip] ${file} (already exists)"
        return 0
    fi

    echo "  [download] ${file} ..."
    if command -v curl &>/dev/null; then
        curl -fsSL -o "$dest" "${url}/${file}"
    elif command -v wget &>/dev/null; then
        wget -q -O "$dest" "${url}/${file}"
    else
        echo "ERROR: neither curl nor wget found" >&2
        exit 1
    fi
}

echo "Downloading CVRPLIB benchmark instances to ${SCRIPT_DIR}/ ..."
for inst in "${CVRP_INSTANCES[@]}"; do
    download "$CVRP_BASE_URL" "$inst"
done

echo "Downloading Solomon VRPTW instances to ${SCRIPT_DIR}/ ..."
for inst in "${SOLOMON_INSTANCES[@]}"; do
    download "$SOLOMON_BASE_URL" "$inst"
done

echo "Done."
