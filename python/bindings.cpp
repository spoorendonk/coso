#include <nanobind/nanobind.h>

namespace nb = nanobind;

NB_MODULE(_coso, m) {
    m.doc() = "COSO — Combinatorial Structure-aware Optimization";
    m.attr("__version__") = "0.1.0";
}
