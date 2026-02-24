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

    auto const& left  = load_prefix(pos - 1);  // prefix up to pos-1
    auto const& right = load_suffix(pos + 1);   // suffix from pos+1 onward

    auto merged = LoadResource::merge(left, right);
    return LoadResource::excess(merged, data_->vehicle_type(vehicle_type_));
}

int Route::eval_insert_distance(int pos, int client) const
{
    assert(pos >= 0 && pos <= size());

    int profile = data_->vehicle_type(vehicle_type_).profile;
    int new_node = node_(client);

    // Previous node (depot if pos == 0, else client at pos-1).
    int prev_node = (pos == 0) ? depot_ : node_(clients_[pos - 1]);

    // Next node (depot if pos == size, else client at pos).
    int next_node = (pos == size()) ? depot_ : node_(clients_[pos]);

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
    int rem_node = node_(clients_[pos]);

    // Previous node (depot if pos == 0).
    int prev_node = (pos == 0) ? depot_ : node_(clients_[pos - 1]);

    // Next node (depot if pos == size()-1).
    int next_node = (pos == size() - 1) ? depot_ : node_(clients_[pos + 1]);

    // Old edges: prev -> rem -> next.
    int old_cost = data_->dist(profile, prev_node, rem_node)
                 + data_->dist(profile, rem_node, next_node);

    // New edge: prev -> next.
    int new_cost = data_->dist(profile, prev_node, next_node);

    return new_cost - old_cost;
}

int Route::eval_insert_dist_excess(int pos, int client) const
{
    assert(pos >= 0 && pos <= size());

    int profile = data_->vehicle_type(vehicle_type_).profile;
    auto client_state = DistanceResource::init(*data_, client);
    auto const& left  = dist_prefix(pos - 1);  // depot -> ... -> c[pos-1]
    auto const& right = dist_suffix(pos);       // c[pos] -> ... -> depot

    // Full route: depot -> ... -> c[pos-1] -> new_client -> c[pos] -> ... -> depot
    auto merged_left = DistanceResource::merge(left, client_state, *data_, profile);
    auto full        = DistanceResource::merge(merged_left, right, *data_, profile);

    return DistanceResource::excess(full, data_->vehicle_type(vehicle_type_));
}

int Route::eval_remove_dist_excess(int pos) const
{
    assert(pos >= 0 && pos < size());

    int profile = data_->vehicle_type(vehicle_type_).profile;
    auto const& left  = dist_prefix(pos - 1);   // depot -> ... -> c[pos-1]
    auto const& right = dist_suffix(pos + 1);    // c[pos+1] -> ... -> depot

    // Full route: depot -> ... -> c[pos-1] -> c[pos+1] -> ... -> depot
    auto full = DistanceResource::merge(left, right, *data_, profile);

    return DistanceResource::excess(full, data_->vehicle_type(vehicle_type_));
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
    load_prefix_.resize(n + 1);
    load_prefix_[0] = LoadResource::init_depot(*data_);

    for (int i = 0; i < n; ++i) {
        auto client_state = LoadResource::init(*data_, clients_[i]);
        load_prefix_[i + 1] = LoadResource::merge(load_prefix_[i], client_state);
    }

    // --- Load suffix ---
    // suffix_[n] = depot state (empty).
    // suffix_[i] = merge(init(client[i]), suffix_[i+1])
    load_suffix_.resize(n + 1);
    load_suffix_[n] = LoadResource::init_depot(*data_);

    for (int i = n - 1; i >= 0; --i) {
        auto client_state = LoadResource::init(*data_, clients_[i]);
        load_suffix_[i] = LoadResource::merge(client_state, load_suffix_[i + 1]);
    }

    // --- Load excess ---
    if (n > 0) {
        load_excess_ = LoadResource::excess(
            load_prefix_[n], data_->vehicle_type(vehicle_type_));
    } else {
        load_excess_ = 0;
    }

    // --- Distance resource prefix/suffix ---
    int profile = data_->vehicle_type(vehicle_type_).profile;

    // prefix_[0] = depot state.
    // prefix_[i+1] = merge(prefix_[i], init(client[i]))
    //   This captures: depot -> c0 -> ... -> c[i]
    dist_prefix_.resize(n + 1);
    dist_prefix_[0] = DistanceResource::init_depot(depot_);

    for (int i = 0; i < n; ++i) {
        auto client_state = DistanceResource::init(*data_, clients_[i]);
        dist_prefix_[i + 1] = DistanceResource::merge(
            dist_prefix_[i], client_state, *data_, profile);
    }

    // suffix_[n] = depot state.
    // suffix_[i] = merge(init(client[i]), suffix_[i+1])
    //   This captures: c[i] -> ... -> c[n-1] -> depot
    dist_suffix_.resize(n + 1);
    dist_suffix_[n] = DistanceResource::init_depot(depot_);

    for (int i = n - 1; i >= 0; --i) {
        auto client_state = DistanceResource::init(*data_, clients_[i]);
        dist_suffix_[i] = DistanceResource::merge(
            client_state, dist_suffix_[i + 1], *data_, profile);
    }

    // --- Distance, duration, and distance excess ---
    if (n > 0) {
        // Full route: merge prefix (depot -> all clients) with depot return.
        // prefix_[n] has first=depot, last=c[n-1].
        // Merging with depot adds the c[n-1] -> depot edge.
        auto full_state = DistanceResource::merge(
            dist_prefix_[n],
            DistanceResource::init_depot(depot_),
            *data_, profile);
        distance_ = full_state.distance;
        duration_ = full_state.duration;
        dist_excess_ = DistanceResource::excess(
            full_state, data_->vehicle_type(vehicle_type_));
    } else {
        distance_ = 0;
        duration_ = 0;
        dist_excess_ = 0;
    }
}

} // namespace coso
