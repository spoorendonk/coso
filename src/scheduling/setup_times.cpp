#include "scheduling/setup_times.h"

#include <stdexcept>

namespace coso {

SetupTimeMatrix::SetupTimeMatrix(int num_operations, int num_machines)
    : num_ops_(num_operations),
      num_machines_(num_machines),
      data_(static_cast<size_t>(num_operations) * num_operations * num_machines, 0) {
    if (num_operations < 0 || num_machines < 0) {
        throw std::invalid_argument("SetupTimeMatrix: dimensions must be non-negative");
    }
}

void SetupTimeMatrix::set(int from, int to, int machine, int time) {
    if (from < 0 || from >= num_ops_ || to < 0 || to >= num_ops_ || machine < 0 ||
        machine >= num_machines_) {
        throw std::out_of_range("SetupTimeMatrix::set: index out of range");
    }
    data_[index(from, to, machine)] = time;
}

void SetupTimeMatrix::set(int from, int to, int time) {
    if (from < 0 || from >= num_ops_ || to < 0 || to >= num_ops_) {
        throw std::out_of_range("SetupTimeMatrix::set: index out of range");
    }
    for (int m = 0; m < num_machines_; ++m) {
        data_[index(from, to, m)] = time;
    }
}

int SetupTimeMatrix::setup_time(int from, int to, int machine) const {
    if (data_.empty()) {
        return 0;
    }
    if (from < 0 || from >= num_ops_ || to < 0 || to >= num_ops_ || machine < 0 ||
        machine >= num_machines_) {
        return 0;
    }
    return data_[index(from, to, machine)];
}

}  // namespace coso
