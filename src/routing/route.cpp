#include "routing/route.h"

#include <algorithm>

namespace coso {

// ---------------------------------------------------------------------------
//  Construction
// ---------------------------------------------------------------------------

Route::Route(ProblemData const& data, int vehicle_type)
    : data_(&data), vehicle_type_(vehicle_type)
{
    // Default depot is node 0.
    depot_ = 0;

    // Start with empty route.
    update_();
}

// ---------------------------------------------------------------------------
//  Modification
// ---------------------------------------------------------------------------

void Route::set_clients(std::vector<int> clients)
{
    clients_ = std::move(clients);
    update_();
}

void Route::insert(int pos, int client)
{
    assert(pos >= 0 && pos <= size());
    clients_.insert(clients_.begin() + pos, client);
    update_();
}

void Route::remove(int pos)
{
    assert(pos >= 0 && pos < size());
    clients_.erase(clients_.begin() + pos);
    update_();
}

void Route::replace(int pos, int new_client)
{
    assert(pos >= 0 && pos < size());
    clients_[pos] = new_client;
    update_();
}

// ---------------------------------------------------------------------------
//  O(1) move evaluation
// ---------------------------------------------------------------------------

int Route::eval_insert_load(int pos, int client) const
{
    assert(pos >= 0 && pos <= size());

    // For DEPOT_VISIT insertions, the load evaluation is complex because
    // it creates a sub-trip boundary.  Fall back to full recomputation.
    if (client == DEPOT_VISIT) {
        Route temp(*data_, vehicle_type_);
        std::vector<int> new_clients(clients_.begin(), clients_.end());
        new_clients.insert(new_clients.begin() + pos, DEPOT_VISIT);
        temp.set_clients(std::move(new_clients));
        return temp.load_excess();
    }

    auto client_state = LoadResource::init(*data_, client);
    auto const& left  = load_prefix(pos - 1);  // prefix up to pos-1
    auto const& right = load_suffix(pos);       // suffix from pos onward

    auto merged_left = LoadResource::merge(left, client_state);
    auto merged      = LoadResource::merge(merged_left, right);

    return LoadResource::excess(merged, data_->vehicle_type(vehicle_type_));
}

int Route::eval_remove_load(int pos) const
{
    assert(pos >= 0 && pos < size());

    // For DEPOT_VISIT removals, fall back to full recomputation since
    // removing a sub-trip boundary merges two sub-trips.
    if (clients_[pos] == DEPOT_VISIT) {
        Route temp(*data_, vehicle_type_);
        std::vector<int> new_clients;
        new_clients.reserve(clients_.size() - 1);
        for (int i = 0; i < size(); ++i)
            if (i != pos)
                new_clients.push_back(clients_[i]);
        temp.set_clients(std::move(new_clients));
        return temp.load_excess();
    }

    auto const& left  = load_prefix(pos - 1);  // prefix up to pos-1
    auto const& right = load_suffix(pos + 1);   // suffix from pos+1 onward

    auto merged = LoadResource::merge(left, right);
    return LoadResource::excess(merged, data_->vehicle_type(vehicle_type_));
}

int Route::eval_insert_distance(int pos, int client) const
{
    assert(pos >= 0 && pos <= size());

    int profile = data_->vehicle_type(vehicle_type_).profile;
    // client can be DEPOT_VISIT (-1) for multi-trip depot insertion.
    int new_node = (client == DEPOT_VISIT) ? depot_ : node_(client);

    // Previous node (depot if pos == 0, else node at pos-1).
    int prev_node = (pos == 0)
        ? depot_
        : (clients_[pos - 1] == DEPOT_VISIT ? depot_ : node_(clients_[pos - 1]));

    // Next node (depot if pos == size, else node at pos).
    int next_node = (pos == size())
        ? depot_
        : (clients_[pos] == DEPOT_VISIT ? depot_ : node_(clients_[pos]));

    // Old edge: prev -> next.
    int old_cost = data_->dist(profile, prev_node, next_node);

    // New edges: prev -> new -> next.
    int new_cost = data_->dist(profile, prev_node, new_node)
                 + data_->dist(profile, new_node, next_node);

    return new_cost - old_cost;
}

int Route::eval_remove_distance(int pos) const
{
    assert(pos >= 0 && pos < size());

    int profile = data_->vehicle_type(vehicle_type_).profile;
    int rem_node = (clients_[pos] == DEPOT_VISIT)
        ? depot_ : node_(clients_[pos]);

    // Previous node (depot if pos == 0).
    int prev_node = (pos == 0)
        ? depot_
        : (clients_[pos - 1] == DEPOT_VISIT ? depot_ : node_(clients_[pos - 1]));

    // Next node (depot if pos == size()-1).
    int next_node = (pos == size() - 1)
        ? depot_
        : (clients_[pos + 1] == DEPOT_VISIT ? depot_ : node_(clients_[pos + 1]));

    // Old edges: prev -> rem -> next.
    int old_cost = data_->dist(profile, prev_node, rem_node)
                 + data_->dist(profile, rem_node, next_node);

    // New edge: prev -> next.
    int new_cost = data_->dist(profile, prev_node, next_node);

    return new_cost - old_cost;
}

// ---------------------------------------------------------------------------
//  Internal: update prefix/suffix arrays
// ---------------------------------------------------------------------------

void Route::update_()
{
    int n = size();

    // --- Load prefix ---
    // prefix_[0] = depot state (empty).
    // prefix_[i+1] = merge(prefix_[i], init(client[i]))
    // DEPOT_VISIT markers reset the load (new sub-trip starts at depot).
    load_prefix_.resize(n + 1);
    load_prefix_[0] = LoadResource::init_depot(*data_);

    for (int i = 0; i < n; ++i) {
        if (clients_[i] == DEPOT_VISIT) {
            // Depot visit: load resets to zero (new sub-trip).
            load_prefix_[i + 1] = LoadResource::init_depot(*data_);
        } else {
            auto client_state = LoadResource::init(*data_, clients_[i]);
            load_prefix_[i + 1] = LoadResource::merge(
                load_prefix_[i], client_state);
        }
    }

    // --- Load suffix ---
    // suffix_[n] = depot state (empty).
    // suffix_[i] = merge(init(client[i]), suffix_[i+1])
    // DEPOT_VISIT markers reset the load (sub-trip ends at depot).
    load_suffix_.resize(n + 1);
    load_suffix_[n] = LoadResource::init_depot(*data_);

    for (int i = n - 1; i >= 0; --i) {
        if (clients_[i] == DEPOT_VISIT) {
            // Depot visit: load resets (sub-trip boundary going right-to-left).
            load_suffix_[i] = LoadResource::init_depot(*data_);
        } else {
            auto client_state = LoadResource::init(*data_, clients_[i]);
            load_suffix_[i] = LoadResource::merge(
                client_state, load_suffix_[i + 1]);
        }
    }

    // --- Load excess ---
    // For multi-trip routes, we compute excess per sub-trip and sum.
    load_excess_ = 0;
    if (n > 0) {
        // Find sub-trip boundaries and evaluate each.
        int trip_start = 0;
        for (int i = 0; i <= n; ++i) {
            if (i == n || clients_[i] == DEPOT_VISIT) {
                // Sub-trip from trip_start to i-1 (exclusive of i).
                if (i > trip_start) {
                    // The prefix at i (or at the last real client before i)
                    // relative to the trip_start gives the sub-trip load.
                    // Since prefix resets at DEPOT_VISIT, load_prefix_[i]
                    // holds the cumulative load for this sub-trip.
                    load_excess_ += LoadResource::excess(
                        load_prefix_[i],
                        data_->vehicle_type(vehicle_type_));
                }
                trip_start = i + 1;
            }
        }
    }

    // --- Distance ---
    int profile = data_->vehicle_type(vehicle_type_).profile;
    distance_ = 0;
    if (n > 0) {
        // For multi-trip routes, DEPOT_VISIT means return to depot then depart.
        // Node for position i: depot if DEPOT_VISIT, else client node.
        auto node_at = [&](int i) -> int {
            return (clients_[i] == DEPOT_VISIT) ? depot_
                                                 : node_(clients_[i]);
        };

        // depot -> first position
        distance_ += data_->dist(profile, depot_, node_at(0));
        // position-to-position edges
        for (int i = 0; i + 1 < n; ++i) {
            distance_ += data_->dist(profile, node_at(i), node_at(i + 1));
        }
        // last position -> depot
        distance_ += data_->dist(profile, node_at(n - 1), depot_);
    }
}

} // namespace coso
