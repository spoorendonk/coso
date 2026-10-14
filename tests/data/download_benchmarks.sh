#!/usr/bin/env bash
# Download benchmark instances for testing (routing, scheduling, rostering).
#
# Usage:  ./tests/data/download_benchmarks.sh
#
# The script is idempotent: existing files are not re-downloaded.
# Instances are saved to the same directory as this script (tests/data/).
#
# ---------------------------------------------------------------------------
#  Provenance
# ---------------------------------------------------------------------------
#
#  This file is where benchmark provenance lives. Every set below carries the
#  URL it is fetched from and what it encodes; the v1 scope rulings (the last
#  comment on #200-#205) carry the rest -- which instance field justifies which
#  declaration, and the published best-known or optimal values to score against.
#
#  A set earns its place by all three of: a live URL, a documented format, and
#  published reference values. A set that only has the first two can be read but
#  not scored, and a run against it proves nothing.
#
#  Verified 2026-09-08 and NOT wired here yet. Recorded so the search is not
#  repeated; wiring them is per-milestone work, not this script's job:
#
#    Routing     Gehring & Homberger VRPTW (sintef.no) -- Solomon at 200-1000
#                Li & Lim PDPTW (sintef.no)
#                Cordeau MDVRP (neo.lcc.uma.es) -- 14 of 33 carry a duration cap
#                OPLib orienteering (github.com/bcamath-ds/OPLib)
#                Chao TOP (mech.kuleuven.be)
#    Packing     OR-Library binpack1-8 -- optimum is the 3rd header number
#                BPPLIB (github.com/mdelorme2/BPPLIB) -- .rar, needs a real
#                  decompressor, so prefer the two below
#                BPPC (site.unibo.it) -- conflicts, 800 instances
#                VPSolver archive (research.fdabrandao.pt) -- 2-D and 20-D
#                  vector packing, and re-hosts Falkenauer/Scholl/Hard28
#    Lot sizing  Suerie data / datam / POST (suerie.de/testsets.html -- note
#                  /testsets/ itself 403s, which is what hid these). datam
#                  (540/540) and POST (80/80) need no schema change at all,
#                  so they are the cheapest first wiring.
#                  Caveat: the published values are CLSPL, not CLSP, and sit
#                  13-17% below the true CLSP optimum. Score against
#                  self-computed optima, not against them.
#    Network     SNDlib (sndlib.put.poznan.pl -- zib.de redirects here)
#                Canad C/C+/R and Bipart/Mulgen (commalab.di.unipi.it)
#                Gunluk capacity expansion (same host; Columbia original 403s)
#    Rostering   schedulingbenchmarks.org .ros collection (GPost, ORTEC,
#                  Ikegami, ...)
#                INRC-II (mobiz.vives.be/inrc2)
#                NSPLib (projectmanagement.ugent.be)
#    Scheduling  SchedulingLab/fjsp-instances -- FJSP, 204 instances
#                OR-Library wt40/wt50 -- weighted tardiness, optima published
#                  (there is no wtopt100; four spellings probed, all 404)
#
#  Dead hosts, checked 2026-09-08. Do not spend time on these again:
#
#    mistic.heig-vd.ch/taillard, eric-taillard.ch  TCP connect times out.
#                                                  Taillard JSP survives via the
#                                                  JSPLIB mirror used below.
#    soa.iti.es/problem-instances                  HTTP 502. Ruiz's flow-shop
#                                                  and SDST sets are gone with
#                                                  it, so flow-shop currently
#                                                  has no verifiable source.
#    jobshop.jjvh.nl                               HTTP 200, but the body is a
#                                                  MySQL connection error.
#    prodhonc.free.fr                              Decayed to a placeholder.
#                                                  LRP survives only via a
#                                                  third-party mirror, which
#                                                  #200 rejected as not a
#                                                  source -- that rejection is
#                                                  what cut the depot cost and
#                                                  capacity slots.
#    bernabe.dorronsoro.es/vrp                     TLS cert mismatch. There is
#                                                  no VRPB evidence anywhere as
#                                                  a result.
#    comopt.ifi.uni-heidelberg.de/.../TSPLIB95     No response.
#
#  Not fetched, on purpose:
#
#    PSPLIB (RCPSP)  #202 defers RCPSP out of v1 and nothing reads .sm files any
#                    more. The old om-db.wi.tum.de/psplib/files/ URLs are 404; the
#                    site serves download_dataset.php?set=j30&mode=sm&format=zip
#                    and download_solution.php?set=j30&mode=sm&type=opt instead,
#                    verified 2026-10 (#231). Wire it when RCPSP is its own
#                    archetype.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# CVRPLIB, Uchoa et al. X instances, via the VROOM-Project GitHub mirror.
# CVRP: demand and a single vehicle capacity. The canonical host is
# galgos.inf.puc-rio.br/cvrplib, which also serves the best-known values and the
# Golden 1-8 instances that carry a DISTANCE route-length cap.
# Distances are nint(EUC_2D) -- a model that computes them from coordinates
# reproduces none of the published values, which is why explicit matrices are a
# v1 keep (#200).
CVRP_BASE_URL="https://raw.githubusercontent.com/VROOM-Project/vroom-scripts/master/benchmarks/CVRP/X"

# CVRP instances to download.
CVRP_INSTANCES=(
	"X-n101-k25.vrp"
	"X-n106-k14.vrp"
	"X-n110-k13.vrp"
	"X-n120-k6.vrp"
	"X-n125-k30.vrp"
)

# Solomon VRPTW instances, via the VROOM-Project GitHub mirror.
# Carries time windows, service times and the depot window. Canonical host is
# sintef.no/projectweb/top/vrptw/solomon-benchmark, which publishes best-knowns
# as a LEXICOGRAPHIC (vehicles, then distance) pair -- an objective RoutingModel
# cannot yet declare, so these instances can be read but not scored (#228).
# Solomon truncates distances to one decimal; CVRPLIB above rounds. Different
# conventions, same script.
SOLOMON_BASE_URL="https://raw.githubusercontent.com/VROOM-Project/vroom-scripts/master/benchmarks/VRPTW/solomon"
SOLOMON_INSTANCES=(
	"C101.txt"
	"C102.txt"
	"R101.txt"
	"R102.txt"
	"RC101.txt"
	"RC102.txt"
)

# A failed download is recorded, not fatal, so one dead host does not stop the
# rest -- but the script exits non-zero and names every file it did not get. A
# script that reports success while downloading nothing is worse than one that
# fails (#231).
FAILED=()

download_to_dir() {
	local url="$1"
	local dest="$2"

	if [[ -f "$dest" ]]; then
		echo "  [skip] $(basename "$dest") (already exists)"
		return 0
	fi

	echo "  [download] $(basename "$dest") ..."
	local ok=0
	if command -v curl &>/dev/null; then
		curl -fsSL -o "$dest" "$url" 2>/dev/null && ok=1
	elif command -v wget &>/dev/null; then
		wget -q -O "$dest" "$url" 2>/dev/null && ok=1
	else
		echo "ERROR: neither curl nor wget found" >&2
		exit 1
	fi
	if [[ $ok -eq 0 ]]; then
		echo "  [FAIL] $(basename "$dest") <- ${url}" >&2
		rm -f "$dest"
		FAILED+=("$url")
	fi
}

download() {
	download_to_dir "${1}/${2}" "${SCRIPT_DIR}/${2}"
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
#  Taillard's own hosts and jobshop.jjvh.nl are dead (see Provenance above);
#  the instances survive via the JSPLIB mirror, in Taillard format.

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

echo "Downloading Taillard JSP instances to ${TAILLARD_DIR}/ ..."
for inst in "${TAILLARD_INSTANCES[@]}"; do
	download_to_dir "${TAILLARD_BASE_URL}/${inst}" "${TAILLARD_DIR}/${inst}.txt"
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

if [[ ${#FAILED[@]} -gt 0 ]]; then
	echo "FAILED: ${#FAILED[@]} download(s) did not complete:" >&2
	printf '  %s\n' "${FAILED[@]}" >&2
	exit 1
fi
echo "Done."
