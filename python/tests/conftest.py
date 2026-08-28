"""Shared fixtures for COSO Python tests."""

import pytest

# The bindings are off in the default build, so on a plain checkout there is no
# coso extension module and every test here fails to import. Collect nothing
# instead, which exits 5 ("no tests ran") — the code both git hooks treat as a
# pass. A bare `import coso` exits 4 and reads as a test failure, blocking every
# commit that touches a .py file; pytest.importorskip in a conftest exits 1.
#
# The reason is printed rather than left silent, so a skipped suite is never
# mistaken for a passing one.
try:
    import coso
except ImportError:  # pragma: no cover - depends on how the tree was built
    coso = None
    collect_ignore_glob = ["*.py"]

    def pytest_report_header(config):
        return (
            "coso: Python bindings not built — skipping the Python suite. "
            "Configure with -DCOSO_BUILD_PYTHON=ON to run it."
        )


@pytest.fixture
def small_cvrp():
    """A small CVRP instance with 1 depot and 5 clients.

    Depot at origin, 5 clients in a rough semicircle.
    One vehicle type with capacity 15, 3 vehicles available.
    Each client demands 3 units, so each vehicle can serve up to 5 clients.
    """
    m = coso.RoutingModel()
    m.add_depot(0.0, 0.0)
    m.add_vehicle_type(3, coso.VehicleTypeParams())

    vt = coso.VehicleTypeParams()
    vt.capacity = [15]

    m2 = coso.RoutingModel()
    m2.add_depot(0.0, 0.0)
    m2.add_vehicle_type(3, vt)

    coords = [(10, 0), (10, 10), (0, 10), (20, 5), (15, 15)]
    for x, y in coords:
        cp = coso.ClientParams()
        cp.demand = [3]
        m2.add_client(float(x), float(y), cp)

    return m2


@pytest.fixture
def vrptw_model():
    """A small VRPTW instance: depot + 3 clients with time windows."""
    m = coso.RoutingModel()

    dp = coso.DepotParams()
    dp.tw = coso.TimeWindow(0, 1000)
    m.add_depot(0.0, 0.0, dp)

    vt = coso.VehicleTypeParams()
    vt.capacity = [100]
    m.add_vehicle_type(2, vt)

    # Client 0: early window
    cp0 = coso.ClientParams()
    cp0.demand = [5]
    cp0.tw = coso.TimeWindow(0, 200)
    m.add_client(10.0, 0.0, cp0)

    # Client 1: mid window
    cp1 = coso.ClientParams()
    cp1.demand = [5]
    cp1.tw = coso.TimeWindow(50, 300)
    m.add_client(20.0, 0.0, cp1)

    # Client 2: late window
    cp2 = coso.ClientParams()
    cp2.demand = [5]
    cp2.tw = coso.TimeWindow(100, 500)
    m.add_client(15.0, 10.0, cp2)

    return m


@pytest.fixture
def distance_matrix_model():
    """A model using explicit distance matrix (no coordinates)."""
    m = coso.RoutingModel()
    m.add_depot(0.0, 0.0)

    vt = coso.VehicleTypeParams()
    vt.capacity = [50]
    m.add_vehicle_type(1, vt)

    cp0 = coso.ClientParams()
    cp0.demand = [5]
    m.add_client(0.0, 0.0, cp0)  # client 0

    cp1 = coso.ClientParams()
    cp1.demand = [5]
    m.add_client(0.0, 0.0, cp1)  # client 1

    # Override Euclidean with explicit distances.
    # Node 0 = depot, Node 1 = client 0, Node 2 = client 1.
    m.set_distance(0, 1, 10)
    m.set_distance(1, 0, 10)
    m.set_distance(0, 2, 20)
    m.set_distance(2, 0, 20)
    m.set_distance(1, 2, 15)
    m.set_distance(2, 1, 15)

    return m
