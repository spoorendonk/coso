#include <catch2/catch_test_macros.hpp>

#include "routing/resources/loading_resource.h"

using namespace coso;

// ---------------------------------------------------------------------------
//  Helpers
// ---------------------------------------------------------------------------

/// 3 requests (6 clients): pickup then delivery.
///   Request 0: pickup=0, delivery=1
///   Request 1: pickup=2, delivery=3
///   Request 2: pickup=4, delivery=5
static std::vector<LoadingResource::ClientInfo> make_loading_info()
{
    std::vector<int> pickups   = {0, 2, 4};
    std::vector<int> deliveries = {1, 3, 5};
    return LoadingResource::build_lookup(6, pickups, deliveries);
}

// ===========================================================================
//  Build lookup tests
// ===========================================================================

TEST_CASE("LoadingResource::build_lookup assigns roles correctly",
          "[loading_resource]")
{
    auto info = make_loading_info();

    CHECK(info[0].request == 0);
    CHECK(info[0].is_pickup);
    CHECK(!info[0].is_delivery);

    CHECK(info[1].request == 0);
    CHECK(!info[1].is_pickup);
    CHECK(info[1].is_delivery);

    CHECK(info[4].request == 2);
    CHECK(info[4].is_pickup);
}

// ===========================================================================
//  Init tests
// ===========================================================================

TEST_CASE("LoadingResource::init for pickup client",
          "[loading_resource]")
{
    auto info = make_loading_info();
    auto s = LoadingResource::init(info, 0);
    REQUIRE(s.pending.size() == 1);
    CHECK(s.pending[0] == 0);
    CHECK(s.needed.empty());
    CHECK(s.violations == 0);
}

TEST_CASE("LoadingResource::init for delivery client",
          "[loading_resource]")
{
    auto info = make_loading_info();
    auto s = LoadingResource::init(info, 1);
    CHECK(s.pending.empty());
    REQUIRE(s.needed.size() == 1);
    CHECK(s.needed[0] == 0);
    CHECK(s.violations == 0);
}

TEST_CASE("LoadingResource::init_depot creates empty state",
          "[loading_resource]")
{
    auto s = LoadingResource::init_depot();
    CHECK(s.pending.empty());
    CHECK(s.needed.empty());
    CHECK(s.violations == 0);
}

// ===========================================================================
//  LIFO merge tests
// ===========================================================================

TEST_CASE("LoadingResource LIFO: correct order (last picked up delivered first)",
          "[loading_resource]")
{
    auto info = make_loading_info();
    auto policy = LoadingResource::Policy::LIFO;

    // Route: pickup0, pickup1, delivery1, delivery0
    // LIFO correct: deliver 1 first (loaded last), then 0.
    auto sp0 = LoadingResource::init(info, 0);  // pickup req 0
    auto sp1 = LoadingResource::init(info, 2);  // pickup req 1
    auto sd1 = LoadingResource::init(info, 3);  // delivery req 1
    auto sd0 = LoadingResource::init(info, 1);  // delivery req 0

    auto m01 = LoadingResource::merge(sp0, sp1, policy);
    CHECK(m01.pending.size() == 2);  // both pending
    CHECK(m01.violations == 0);

    auto m012 = LoadingResource::merge(m01, sd1, policy);
    CHECK(m012.pending.size() == 1);  // req 0 still pending
    CHECK(m012.violations == 0);

    auto m0123 = LoadingResource::merge(m012, sd0, policy);
    CHECK(m0123.pending.empty());
    CHECK(m0123.needed.empty());
    CHECK(m0123.violations == 0);
    CHECK(LoadingResource::excess(m0123) == 0);
}

TEST_CASE("LoadingResource LIFO: wrong order (delivers in pickup order)",
          "[loading_resource]")
{
    auto info = make_loading_info();
    auto policy = LoadingResource::Policy::LIFO;

    // Route: pickup0, pickup1, delivery0, delivery1
    // LIFO violation: delivering 0 before 1 (but 1 was loaded last).
    auto sp0 = LoadingResource::init(info, 0);  // pickup req 0
    auto sp1 = LoadingResource::init(info, 2);  // pickup req 1
    auto sd0 = LoadingResource::init(info, 1);  // delivery req 0
    auto sd1 = LoadingResource::init(info, 3);  // delivery req 1

    auto m01 = LoadingResource::merge(sp0, sp1, policy);
    auto m_deliveries = LoadingResource::merge(sd0, sd1, policy);
    auto m_all = LoadingResource::merge(m01, m_deliveries, policy);

    // The delivery order is [req0, req1], but LIFO expects [req1, req0].
    CHECK(m_all.violations > 0);
    CHECK(LoadingResource::excess(m_all) > 0);
}

// ===========================================================================
//  FIFO merge tests
// ===========================================================================

TEST_CASE("LoadingResource FIFO: correct order (first picked up delivered first)",
          "[loading_resource]")
{
    auto info = make_loading_info();
    auto policy = LoadingResource::Policy::FIFO;

    // Route: pickup0, pickup1, delivery0, delivery1
    // FIFO correct: deliver 0 first (loaded first), then 1.
    auto sp0 = LoadingResource::init(info, 0);
    auto sp1 = LoadingResource::init(info, 2);
    auto sd0 = LoadingResource::init(info, 1);
    auto sd1 = LoadingResource::init(info, 3);

    auto m01 = LoadingResource::merge(sp0, sp1, policy);
    auto m_deliveries = LoadingResource::merge(sd0, sd1, policy);
    auto m_all = LoadingResource::merge(m01, m_deliveries, policy);

    CHECK(m_all.violations == 0);
    CHECK(LoadingResource::excess(m_all) == 0);
}

TEST_CASE("LoadingResource FIFO: wrong order (delivers in reverse pickup order)",
          "[loading_resource]")
{
    auto info = make_loading_info();
    auto policy = LoadingResource::Policy::FIFO;

    // Route: pickup0, pickup1, delivery1, delivery0
    // FIFO violation: delivering 1 before 0 (but 0 was loaded first).
    auto sp0 = LoadingResource::init(info, 0);
    auto sp1 = LoadingResource::init(info, 2);
    auto sd1 = LoadingResource::init(info, 3);
    auto sd0 = LoadingResource::init(info, 1);

    auto m01 = LoadingResource::merge(sp0, sp1, policy);
    auto m_deliveries = LoadingResource::merge(sd1, sd0, policy);
    auto m_all = LoadingResource::merge(m01, m_deliveries, policy);

    CHECK(m_all.violations > 0);
    CHECK(LoadingResource::excess(m_all) > 0);
}

// ===========================================================================
//  Edge cases
// ===========================================================================

TEST_CASE("LoadingResource: single request has no ordering constraint",
          "[loading_resource]")
{
    auto info = make_loading_info();

    auto sp0 = LoadingResource::init(info, 0);  // pickup req 0
    auto sd0 = LoadingResource::init(info, 1);  // delivery req 0

    auto m_lifo = LoadingResource::merge(sp0, sd0,
                                         LoadingResource::Policy::LIFO);
    CHECK(m_lifo.violations == 0);
    CHECK(m_lifo.pending.empty());
    CHECK(m_lifo.needed.empty());

    auto m_fifo = LoadingResource::merge(sp0, sd0,
                                         LoadingResource::Policy::FIFO);
    CHECK(m_fifo.violations == 0);
}

TEST_CASE("LoadingResource: delivery without pickup yields needed entry",
          "[loading_resource]")
{
    auto info = make_loading_info();

    auto sd0 = LoadingResource::init(info, 1);  // delivery req 0

    // Merging delivery with depot: delivery needs a pickup.
    auto depot = LoadingResource::init_depot();
    auto m = LoadingResource::merge(depot, sd0,
                                    LoadingResource::Policy::LIFO);

    CHECK(m.needed.size() == 1);
    CHECK(LoadingResource::excess(m) == 1);
}

TEST_CASE("LoadingResource: three requests LIFO correct order",
          "[loading_resource]")
{
    auto info = make_loading_info();
    auto policy = LoadingResource::Policy::LIFO;

    // pickup0, pickup1, pickup2, delivery2, delivery1, delivery0
    auto sp0 = LoadingResource::init(info, 0);
    auto sp1 = LoadingResource::init(info, 2);
    auto sp2 = LoadingResource::init(info, 4);
    auto sd2 = LoadingResource::init(info, 5);
    auto sd1 = LoadingResource::init(info, 3);
    auto sd0 = LoadingResource::init(info, 1);

    auto pickups = LoadingResource::merge(
        LoadingResource::merge(sp0, sp1, policy), sp2, policy);
    auto deliveries = LoadingResource::merge(
        LoadingResource::merge(sd2, sd1, policy), sd0, policy);
    auto all = LoadingResource::merge(pickups, deliveries, policy);

    CHECK(all.violations == 0);
    CHECK(all.pending.empty());
    CHECK(all.needed.empty());
    CHECK(LoadingResource::excess(all) == 0);
}
