#!/usr/bin/env bash
# Download benchmark instances for testing (routing + scheduling).
#
# Usage:  ./tests/data/download_benchmarks.sh
#
# The script is idempotent: existing files are not re-downloaded.
# Instances are saved to the same directory as this script (tests/data/).

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# CVRPLIB base URL (Uchoa et al. X instances, via VROOM-Project GitHub mirror).
CVRP_BASE_URL="https://raw.githubusercontent.com/VROOM-Project/vroom-scripts/master/benchmarks/CVRP/X"

# CVRP instances to download.
CVRP_INSTANCES=(
    "X-n101-k25.vrp"
    "X-n106-k14.vrp"
    "X-n110-k13.vrp"
    "X-n120-k6.vrp"
    "X-n125-k30.vrp"
)

# Solomon VRPTW instances (via VROOM-Project GitHub mirror).
SOLOMON_BASE_URL="https://raw.githubusercontent.com/VROOM-Project/vroom-scripts/master/benchmarks/VRPTW/solomon"
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
        curl -fsSL -o "$dest" "${url}/${file}" 2>/dev/null || {
            echo "  [warn] failed to download ${file}"
            rm -f "$dest"
            return 0
        }
    elif command -v wget &>/dev/null; then
        wget -q -O "$dest" "${url}/${file}" 2>/dev/null || {
            echo "  [warn] failed to download ${file}"
            rm -f "$dest"
            return 0
        }
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

# ---------------------------------------------------------------------------
#  Taillard JSP instances (ta01-ta10, 15x15)
# ---------------------------------------------------------------------------
#
#  Source: http://jobshop.jjvh.nl/  (Taillard format)
#  Mirror: https://raw.githubusercontent.com/tamy0612/JSPLIB/main/instances

TAILLARD_BASE_URL="https://raw.githubusercontent.com/tamy0612/JSPLIB/master/instances"
TAILLARD_DIR="${SCRIPT_DIR}/taillard"
mkdir -p "$TAILLARD_DIR"

TAILLARD_INSTANCES=(
    "ta01"
    "ta02"
    "ta03"
    "ta04"
    "ta05"
    "ta06"
    "ta07"
    "ta08"
    "ta09"
    "ta10"
)

download_to_dir() {
    local url="$1"
    local dest="$2"

    if [[ -f "$dest" ]]; then
        echo "  [skip] $(basename "$dest") (already exists)"
        return 0
    fi

    echo "  [download] $(basename "$dest") ..."
    if command -v curl &>/dev/null; then
        curl -fsSL -o "$dest" "$url" 2>/dev/null || {
            echo "  [warn] failed to download $(basename "$dest")"
            rm -f "$dest"
            return 0
        }
    elif command -v wget &>/dev/null; then
        wget -q -O "$dest" "$url" 2>/dev/null || {
            echo "  [warn] failed to download $(basename "$dest")"
            rm -f "$dest"
            return 0
        }
    else
        echo "ERROR: neither curl nor wget found" >&2
        exit 1
    fi
}

echo "Downloading Taillard JSP instances to ${TAILLARD_DIR}/ ..."
for inst in "${TAILLARD_INSTANCES[@]}"; do
    download_to_dir "${TAILLARD_BASE_URL}/${inst}" "${TAILLARD_DIR}/${inst}.txt"
done

# ---------------------------------------------------------------------------
#  PSPLIB j30 instances (j301_1 - j301_5)
# ---------------------------------------------------------------------------
#
#  Source: https://www.om-db.wi.tum.de/psplib/  (Patterson .sm format)
#  These are small 30-activity RCPSP instances with 4 renewable resources.

PSPLIB_BASE_URL="https://www.om-db.wi.tum.de/psplib/files"
PSPLIB_DIR="${SCRIPT_DIR}/psplib"
mkdir -p "$PSPLIB_DIR"

PSPLIB_INSTANCES=(
    "j301_1.sm"
    "j301_2.sm"
    "j301_3.sm"
    "j301_4.sm"
    "j301_5.sm"
)

echo "Downloading PSPLIB j30 instances to ${PSPLIB_DIR}/ ..."
for inst in "${PSPLIB_INSTANCES[@]}"; do
    download_to_dir "${PSPLIB_BASE_URL}/${inst}" "${PSPLIB_DIR}/${inst}"
done

# ---------------------------------------------------------------------------
#  NRP (Nurse Rostering) instances from schedulingbenchmarks.org
# ---------------------------------------------------------------------------
#
#  Source: https://www.schedulingbenchmarks.org/nrp/
#  Instances 1-8 cover small to medium problems (8-30 employees, 2-4 weeks).

NRP_BASE_URL="http://www.schedulingbenchmarks.org/nrp/data"
NRP_DIR="${SCRIPT_DIR}/nrp"
mkdir -p "$NRP_DIR"

NRP_INSTANCES=(
    "Instance1.txt"
    "Instance2.txt"
    "Instance3.txt"
    "Instance4.txt"
    "Instance5.txt"
    "Instance6.txt"
    "Instance7.txt"
    "Instance8.txt"
)

echo "Downloading NRP instances to ${NRP_DIR}/ ..."
for inst in "${NRP_INSTANCES[@]}"; do
    download_to_dir "${NRP_BASE_URL}/${inst}" "${NRP_DIR}/${inst}"
done

echo "Done."
