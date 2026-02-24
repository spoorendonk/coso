"""COSO — Combinatorial Structure-aware Optimization."""

from coso._coso import (
    __version__,
    Coord,
    CostParams,
    TimeLimit,
    TimeWindow,
    Result,
    VehicleTypeParams,
    ClientParams,
    DepotParams,
    RoutingModel,
    solve_instance,
)

__all__ = [
    "__version__",
    "Coord",
    "CostParams",
    "TimeLimit",
    "TimeWindow",
    "Result",
    "VehicleTypeParams",
    "ClientParams",
    "DepotParams",
    "RoutingModel",
    "solve_instance",
]
