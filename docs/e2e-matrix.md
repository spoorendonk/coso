# E2E Variant Coverage Matrix

This matrix maps every roadmap variant code (`R/N/S/A/K/P`) to concrete E2E scenario IDs,
owning work units, and rollout status so agents can pick work without ambiguity.

Status legend:
- `planned`: scenario IDs reserved, implementation not started
- `blocked_api`: public model API lane not merged yet (`12.1` / `12.2`)
- `blocked_solve`: model solve path lane not merged yet (`13.1` / `13.2` / `13.3`)
- `in_progress`: actively implemented in an open PR
- `done`: merged and part of `e2e-smoke` / `e2e-benchmark`

## Routing (`R`)

| Variant | Problem | Abbrev | Scenario IDs (planned) | Pack lane | Owning work unit | Owner | Status |
|---|---|---|---|---|---|---|---|
| R1 | Capacitated VRP | CVRP | `smoke_r1_cvrp_01`, `bench_r1_cvrp_01` | 14.1 | 14.1 | TBD | planned |
| R2 | VRP with Time Windows | VRPTW | `smoke_r2_vrptw_01`, `bench_r2_vrptw_01` | 14.1 | 14.1 | TBD | planned |
| R3 | Heterogeneous Fleet VRP | HFVRP | `smoke_r3_hfvrp_01`, `bench_r3_hfvrp_01` | 14.1 | 14.1 | TBD | planned |
| R4 | Multi-Depot VRP | MDVRP | `smoke_r4_mdvrp_01`, `bench_r4_mdvrp_01` | 14.1 | 14.1 | TBD | planned |
| R5 | Open VRP | OVRP | `smoke_r5_ovrp_01`, `bench_r5_ovrp_01` | 14.1 | 14.1 | TBD | planned |
| R6 | VRP Simultaneous PD | VRPSPD | `smoke_r6_vrpspd_01`, `bench_r6_vrpspd_01` | 14.1 | 14.1 | TBD | planned |
| R7 | VRP with Backhauls | VRPB | `smoke_r7_vrpb_01`, `bench_r7_vrpb_01` | 14.1 | 14.1 | TBD | planned |
| R8 | Team Orienteering | TOP | `smoke_r8_top_01`, `bench_r8_top_01` | 14.1 | 14.1 | TBD | planned |
| R9 | Multi-dim Capacity | MDCVRP | `smoke_r9_mdcvrp_01`, `bench_r9_mdcvrp_01` | 14.1 | 14.1 | TBD | planned |
| R10 | Routing Profiles | - | `smoke_r10_profiles_01`, `bench_r10_profiles_01` | 14.1 | 14.1 | TBD | planned |
| R11 | Client Groups | GVRP | `smoke_r11_gvrp_01`, `bench_r11_gvrp_01` | 14.1 | 14.1 | TBD | planned |
| R12 | Multi-trip VRP | MTVRP | `smoke_r12_mtvrp_01`, `bench_r12_mtvrp_01` | 14.1 | 14.1 | TBD | planned |
| R13 | Release Times | - | `smoke_r13_release_01`, `bench_r13_release_01` | 14.1 | 14.1 | TBD | planned |
| R14 | Overtime | - | `smoke_r14_overtime_01`, `bench_r14_overtime_01` | 14.1 | 14.1 | TBD | planned |
| R15 | Paired Pickup-Delivery TW | PDPTW | `smoke_r15_pdptw_01`, `bench_r15_pdptw_01` | 14.1 | 14.1 | TBD | planned |
| R16 | Capacitated Arc Routing | CARP | `smoke_r16_carp_01`, `bench_r16_carp_01` | 14.2 | 14.2 | TBD | planned |
| R17 | Cumulative CVRP | CCVRP | `smoke_r17_ccvrp_01`, `bench_r17_ccvrp_01` | 14.2 | 14.2 | TBD | planned |
| R18 | Split Delivery VRP | SDVRP | `smoke_r18_sdvrp_01`, `bench_r18_sdvrp_01` | 14.2 | 14.2 | TBD | planned |
| R19 | Time-Dependent VRP | TDVRP | `smoke_r19_tdvrp_01`, `bench_r19_tdvrp_01` | 14.2 | 14.2 | TBD | planned |
| R20 | Electric VRP | EVRP | `smoke_r20_evrp_01`, `bench_r20_evrp_01` | 14.2 | 14.2 | TBD | planned |
| R21 | Period VRP | PVRP | `smoke_r21_pvrp_01`, `bench_r21_pvrp_01` | 14.2 | 14.2 | TBD | planned |
| R22 | Inventory Routing | IRP | `smoke_r22_irp_01`, `bench_r22_irp_01` | 14.2 | 14.2 | TBD | planned |
| R23 | Location-Routing | LRP | `smoke_r23_lrp_01`, `bench_r23_lrp_01` | 14.2 | 14.2 | TBD | planned |
| R24 | Site-Dependent VRP | SDVRPTW | `smoke_r24_sdvrptw_01`, `bench_r24_sdvrptw_01` | 14.2 | 14.2 | TBD | planned |
| R25 | Clustered VRP | cluVRP | `smoke_r25_cluvrp_01`, `bench_r25_cluvrp_01` | 14.2 | 14.2 | TBD | planned |
| R26 | VRP with Transshipment | VRPTF | `smoke_r26_vrptf_01`, `bench_r26_vrptf_01` | 14.2 | 14.2 | TBD | planned |
| R27 | Technician Routing & Scheduling | TRSP | `smoke_r27_trsp_01`, `bench_r27_trsp_01` | 14.2 | 14.2 | TBD | planned |
| R28 | Home Healthcare Routing | HHCRP | `smoke_r28_hhcrp_01`, `bench_r28_hhcrp_01` | 14.2 | 14.2 | TBD | planned |

## Network (`N`)

| Variant | Problem | Abbrev | Scenario IDs (planned) | Pack lane | Owning work unit | Owner | Status |
|---|---|---|---|---|---|---|---|
| N1 | Multi-Commodity Flow | MCF | `smoke_n1_mcf_01`, `bench_n1_mcf_01` | 14.3 | 14.3 | TBD | planned |
| N2 | Resource-Constrained MCF | RCMCF | `smoke_n2_rcmcf_01`, `bench_n2_rcmcf_01` | 14.3 | 14.3 | TBD | planned |
| N3 | Liner Shipping Network Design | LSNDP | `smoke_n3_lsndp_01`, `bench_n3_lsndp_01` | 14.3 | 14.3 | TBD | planned |

## Scheduling (`S`)

| Variant | Problem | Abbrev | Scenario IDs (planned) | Pack lane | Owning work unit | Owner | Status |
|---|---|---|---|---|---|---|---|
| S1 | Job Shop Scheduling | JSP | `smoke_s1_jsp_01`, `bench_s1_jsp_01` | 14.4 | 14.4 | TBD | blocked_solve |
| S2 | Permutation Flow Shop | PFSP | `smoke_s2_pfsp_01`, `bench_s2_pfsp_01` | 14.4 | 14.4 | TBD | blocked_solve |
| S3 | Flexible Job Shop | FJSP | `smoke_s3_fjsp_01`, `bench_s3_fjsp_01` | 14.4 | 14.4 | TBD | blocked_solve |
| S4 | RCPSP | RCPSP | `smoke_s4_rcpsp_01`, `bench_s4_rcpsp_01` | 14.4 | 14.4 | TBD | blocked_solve |
| S5 | JSP with Setup Times | JSP-SDST | `smoke_s5_jspsdst_01`, `bench_s5_jspsdst_01` | 14.4 | 14.4 | TBD | blocked_solve |
| S6 | Parallel Machine | `P||Cmax` | `smoke_s6_pm_01`, `bench_s6_pm_01` | 14.4 | 14.4 | TBD | blocked_solve |
| S7 | Unrelated Parallel Machine | `R||Cmax` | `smoke_s7_upm_01`, `bench_s7_upm_01` | 14.4 | 14.4 | TBD | blocked_solve |
| S8 | Hybrid Flow Shop | HFS | `smoke_s8_hfs_01`, `bench_s8_hfs_01` | 14.4 | 14.4 | TBD | blocked_solve |
| S9 | Open Shop | OSSP | `smoke_s9_ossp_01`, `bench_s9_ossp_01` | 14.4 | 14.4 | TBD | blocked_solve |
| S10 | Multi-Mode RCPSP | MRCPSP | `smoke_s10_mrcpsp_01`, `bench_s10_mrcpsp_01` | 14.4 | 14.4 | TBD | blocked_solve |
| S11 | Preemptive RCPSP | PRCPSP | `smoke_s11_prcpsp_01`, `bench_s11_prcpsp_01` | 14.4 | 14.4 | TBD | blocked_solve |
| S12 | Car Sequencing | - | `smoke_s12_carseq_01`, `bench_s12_carseq_01` | 14.4 | 14.4 | TBD | blocked_solve |

## Assignment (`A`)

| Variant | Problem | Abbrev | Scenario IDs (planned) | Pack lane | Owning work unit | Owner | Status |
|---|---|---|---|---|---|---|---|
| A1 | Nurse Rostering | NRP | `smoke_a1_nrp_01`, `bench_a1_nrp_01` | 14.5 | 14.5 | TBD | blocked_solve |
| A2 | Employee Scheduling | ESP | `smoke_a2_esp_01`, `bench_a2_esp_01` | 14.5 | 14.5 | TBD | blocked_solve |
| A3 | Multi-Activity Scheduling | MATSP | `smoke_a3_matsp_01`, `bench_a3_matsp_01` | 14.5 | 14.5 | TBD | blocked_solve |
| A4 | School Timetabling | - | `smoke_a4_school_01`, `bench_a4_school_01` | 14.5 | 14.5 | TBD | blocked_solve |
| A5 | Conference Scheduling | - | `smoke_a5_conference_01`, `bench_a5_conference_01` | 14.5 | 14.5 | TBD | blocked_solve |
| A6 | Bed Allocation | BAS | `smoke_a6_bas_01`, `bench_a6_bas_01` | 14.5 | 14.5 | TBD | blocked_solve |

## Packing (`K`)

| Variant | Problem | Abbrev | Scenario IDs (planned) | Pack lane | Owning work unit | Owner | Status |
|---|---|---|---|---|---|---|---|
| K1 | Bin Packing | BPP | `smoke_k1_bpp_01`, `bench_k1_bpp_01` | 14.6 | 14.6 | TBD | blocked_solve |
| K2 | Vector Bin Packing | VBP | `smoke_k2_vbp_01`, `bench_k2_vbp_01` | 14.6 | 14.6 | TBD | blocked_solve |
| K3 | Bin Packing w/ Conflicts | BPPC | `smoke_k3_bppc_01`, `bench_k3_bppc_01` | 14.6 | 14.6 | TBD | blocked_solve |

## Production (`P`)

| Variant | Problem | Abbrev | Scenario IDs (planned) | Pack lane | Owning work unit | Owner | Status |
|---|---|---|---|---|---|---|---|
| P1 | Capacitated Lot Sizing | CLSP | `smoke_p1_clsp_01`, `bench_p1_clsp_01` | 14.7 | 14.7 | TBD | planned |
| P2 | Multi-Level CLSP | MLCLSP | `smoke_p2_mlclsp_01`, `bench_p2_mlclsp_01` | 14.7 | 14.7 | TBD | planned |

## Coordination Rules

- One row update per PR: claim by setting `Owner` and `Status=in_progress`.
- A row can only be marked `done` after:
  - scenario JSON exists,
  - invariant checks are registered,
  - case is in `e2e-smoke` or `e2e-benchmark`,
  - CI evidence is attached in the PR.
- Parser-format scenarios are tracked in `14.8` and should reference row IDs above.
