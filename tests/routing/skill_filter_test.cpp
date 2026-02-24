#include <catch2/catch_test_macros.hpp>

#include "routing/resources/skill_filter.h"

using namespace coso;

// ---------------------------------------------------------------------------
//  Helpers
// ---------------------------------------------------------------------------

/// Instance with skills: 1 depot, 4 clients, 2 vehicle types.
///   Client 0: requires {"cold_chain"}
///   Client 1: requires {"hazmat"}
///   Client 2: requires {"cold_chain", "hazmat"}
///   Client 3: no skills required
///   VType 0:  provides {"cold_chain"}         (1 vehicle)
///   VType 1:  provides {"cold_chain", "hazmat"} (1 vehicle)
static ProblemData make_skill_instance()
{
    ProblemData::Builder b;
    b.add_depot({0.0, 0.0});

    b.add_vehicle_type(1, {.capacity = {10}, .skills = {"cold_chain"}});
    b.add_vehicle_type(1, {.capacity = {10}, .skills = {"cold_chain", "hazmat"}});

    b.add_client({10.0, 0.0}, {.demand = {1}, .skills = {"cold_chain"}});            // 0
    b.add_client({20.0, 0.0}, {.demand = {1}, .skills = {"hazmat"}});                 // 1
    b.add_client({30.0, 0.0}, {.demand = {1}, .skills = {"cold_chain", "hazmat"}});   // 2
    b.add_client({40.0, 0.0}, {.demand = {1}});                                       // 3

    return b.build(0);
}

/// Instance with no skills at all.
static ProblemData make_no_skills_instance()
{
    ProblemData::Builder b;
    b.add_depot({0.0, 0.0});
    b.add_vehicle_type(1, {.capacity = {10}});

    b.add_client({10.0, 0.0}, {.demand = {1}});
    b.add_client({20.0, 0.0}, {.demand = {1}});

    return b.build(0);
}

/// Instance with many skills (test up to 5 distinct skills).
static ProblemData make_many_skills_instance()
{
    ProblemData::Builder b;
    b.add_depot({0.0, 0.0});

    b.add_vehicle_type(1, {.capacity = {10},
                           .skills = {"A", "B", "C"}});
    b.add_vehicle_type(1, {.capacity = {10},
                           .skills = {"A", "B", "C", "D", "E"}});

    b.add_client({10.0, 0.0}, {.demand = {1}, .skills = {"A"}});
    b.add_client({20.0, 0.0}, {.demand = {1}, .skills = {"B", "C"}});
    b.add_client({30.0, 0.0}, {.demand = {1}, .skills = {"D", "E"}});

    return b.build(0);
}

// ===========================================================================
//  build_skill_data tests
// ===========================================================================

TEST_CASE("SkillFilter::build_skill_data assigns unique bit indices",
          "[skill_filter]")
{
    auto data = make_skill_instance();
    auto sd = SkillFilter::build_skill_data(data);

    CHECK(sd.num_skills == 2);  // "cold_chain" and "hazmat"
    CHECK(sd.skill_index.size() == 2);
    CHECK(sd.skill_index.contains("cold_chain"));
    CHECK(sd.skill_index.contains("hazmat"));

    // Indices should be distinct.
    CHECK(sd.skill_index.at("cold_chain") != sd.skill_index.at("hazmat"));
}

TEST_CASE("SkillFilter::build_skill_data with no skills", "[skill_filter]")
{
    auto data = make_no_skills_instance();
    auto sd = SkillFilter::build_skill_data(data);

    CHECK(sd.num_skills == 0);
    CHECK(sd.client_mask[0] == 0);
    CHECK(sd.client_mask[1] == 0);
    CHECK(sd.vehicle_mask[0] == 0);
}

TEST_CASE("SkillFilter::build_skill_data client masks are correct",
          "[skill_filter]")
{
    auto data = make_skill_instance();
    auto sd = SkillFilter::build_skill_data(data);

    int cc = sd.skill_index.at("cold_chain");
    int hz = sd.skill_index.at("hazmat");

    // Client 0: cold_chain only.
    CHECK(sd.client_mask[0] == (uint64_t{1} << cc));

    // Client 1: hazmat only.
    CHECK(sd.client_mask[1] == (uint64_t{1} << hz));

    // Client 2: cold_chain + hazmat.
    CHECK(sd.client_mask[2] == ((uint64_t{1} << cc) | (uint64_t{1} << hz)));

    // Client 3: no skills.
    CHECK(sd.client_mask[3] == 0);
}

TEST_CASE("SkillFilter::build_skill_data vehicle masks are correct",
          "[skill_filter]")
{
    auto data = make_skill_instance();
    auto sd = SkillFilter::build_skill_data(data);

    int cc = sd.skill_index.at("cold_chain");
    int hz = sd.skill_index.at("hazmat");

    // VType 0: cold_chain only.
    CHECK(sd.vehicle_mask[0] == (uint64_t{1} << cc));

    // VType 1: cold_chain + hazmat.
    CHECK(sd.vehicle_mask[1] == ((uint64_t{1} << cc) | (uint64_t{1} << hz)));
}

// ===========================================================================
//  init / merge tests
// ===========================================================================

TEST_CASE("SkillFilter::init creates correct single-client state",
          "[skill_filter]")
{
    auto data = make_skill_instance();
    auto sd = SkillFilter::build_skill_data(data);

    auto s0 = SkillFilter::init(sd, 0);
    CHECK(s0.required == sd.client_mask[0]);

    auto s3 = SkillFilter::init(sd, 3);
    CHECK(s3.required == 0);  // no skills required
}

TEST_CASE("SkillFilter::init_depot has no required skills", "[skill_filter]")
{
    auto s = SkillFilter::init_depot();
    CHECK(s.required == 0);
}

TEST_CASE("SkillFilter::merge unions required skills", "[skill_filter]")
{
    auto data = make_skill_instance();
    auto sd = SkillFilter::build_skill_data(data);

    // Client 0 (cold_chain) + Client 1 (hazmat) -> both required.
    auto s0 = SkillFilter::init(sd, 0);
    auto s1 = SkillFilter::init(sd, 1);
    auto merged = SkillFilter::merge(s0, s1);

    int cc = sd.skill_index.at("cold_chain");
    int hz = sd.skill_index.at("hazmat");
    CHECK(merged.required == ((uint64_t{1} << cc) | (uint64_t{1} << hz)));
}

TEST_CASE("SkillFilter::merge with no-skill client doesn't add requirements",
          "[skill_filter]")
{
    auto data = make_skill_instance();
    auto sd = SkillFilter::build_skill_data(data);

    auto s0 = SkillFilter::init(sd, 0);  // cold_chain
    auto s3 = SkillFilter::init(sd, 3);  // no skills
    auto merged = SkillFilter::merge(s0, s3);

    CHECK(merged.required == sd.client_mask[0]);
}

TEST_CASE("SkillFilter::merge three clients accumulates all skills",
          "[skill_filter]")
{
    auto data = make_skill_instance();
    auto sd = SkillFilter::build_skill_data(data);

    auto s0 = SkillFilter::init(sd, 0);
    auto s1 = SkillFilter::init(sd, 1);
    auto s3 = SkillFilter::init(sd, 3);

    auto m01  = SkillFilter::merge(s0, s1);
    auto m013 = SkillFilter::merge(m01, s3);

    // Adding a no-skill client doesn't change the mask.
    CHECK(m013.required == m01.required);
}

TEST_CASE("SkillFilter::merge_reverse equals merge (direction-independent)",
          "[skill_filter]")
{
    auto data = make_skill_instance();
    auto sd = SkillFilter::build_skill_data(data);

    auto s0 = SkillFilter::init(sd, 0);
    auto s1 = SkillFilter::init(sd, 1);

    auto fwd = SkillFilter::merge(s0, s1);
    auto rev = SkillFilter::merge_reverse(s0, s1);

    CHECK(fwd.required == rev.required);
}

// ===========================================================================
//  excess / feasibility tests
// ===========================================================================

TEST_CASE("SkillFilter::excess reports missing skills", "[skill_filter]")
{
    auto data = make_skill_instance();
    auto sd = SkillFilter::build_skill_data(data);

    // Route with client 0 (cold_chain) served by vtype 0 (cold_chain) -> ok.
    auto s0 = SkillFilter::init(sd, 0);
    CHECK(SkillFilter::excess(s0, sd, 0) == 0);
    CHECK(SkillFilter::feasible(s0, sd, 0));

    // Route with client 1 (hazmat) served by vtype 0 (cold_chain only) -> 1 missing.
    auto s1 = SkillFilter::init(sd, 1);
    CHECK(SkillFilter::excess(s1, sd, 0) == 1);
    CHECK_FALSE(SkillFilter::feasible(s1, sd, 0));

    // Route with client 1 (hazmat) served by vtype 1 (cold_chain+hazmat) -> ok.
    CHECK(SkillFilter::excess(s1, sd, 1) == 0);
    CHECK(SkillFilter::feasible(s1, sd, 1));
}

TEST_CASE("SkillFilter::excess with multi-skill client", "[skill_filter]")
{
    auto data = make_skill_instance();
    auto sd = SkillFilter::build_skill_data(data);

    // Client 2 requires both cold_chain and hazmat.
    auto s2 = SkillFilter::init(sd, 2);

    // VType 0 (cold_chain only) -> missing hazmat.
    CHECK(SkillFilter::excess(s2, sd, 0) == 1);

    // VType 1 (cold_chain+hazmat) -> ok.
    CHECK(SkillFilter::excess(s2, sd, 1) == 0);
}

TEST_CASE("SkillFilter::excess with merged route", "[skill_filter]")
{
    auto data = make_skill_instance();
    auto sd = SkillFilter::build_skill_data(data);

    // Route: client 0 (cold_chain) + client 1 (hazmat).
    auto s0 = SkillFilter::init(sd, 0);
    auto s1 = SkillFilter::init(sd, 1);
    auto merged = SkillFilter::merge(s0, s1);

    // VType 0 (cold_chain only) -> missing hazmat.
    CHECK(SkillFilter::excess(merged, sd, 0) == 1);

    // VType 1 (cold_chain+hazmat) -> ok.
    CHECK(SkillFilter::excess(merged, sd, 1) == 0);
}

TEST_CASE("SkillFilter::excess with no skills required", "[skill_filter]")
{
    auto data = make_skill_instance();
    auto sd = SkillFilter::build_skill_data(data);

    // Client 3 (no skills) served by any vehicle -> always feasible.
    auto s3 = SkillFilter::init(sd, 3);
    CHECK(SkillFilter::excess(s3, sd, 0) == 0);
    CHECK(SkillFilter::excess(s3, sd, 1) == 0);
}

TEST_CASE("SkillFilter::excess with no skills in instance", "[skill_filter]")
{
    auto data = make_no_skills_instance();
    auto sd = SkillFilter::build_skill_data(data);

    auto s0 = SkillFilter::init(sd, 0);
    auto s1 = SkillFilter::init(sd, 1);
    auto merged = SkillFilter::merge(s0, s1);

    // No skills at all -> always feasible.
    CHECK(SkillFilter::excess(merged, sd, 0) == 0);
    CHECK(SkillFilter::feasible(merged, sd, 0));
}

TEST_CASE("SkillFilter::excess with many skills", "[skill_filter]")
{
    auto data = make_many_skills_instance();
    auto sd = SkillFilter::build_skill_data(data);

    CHECK(sd.num_skills == 5);

    // All clients: A + B + C + D + E.
    auto s0 = SkillFilter::init(sd, 0);
    auto s1 = SkillFilter::init(sd, 1);
    auto s2 = SkillFilter::init(sd, 2);

    auto m01  = SkillFilter::merge(s0, s1);
    auto m012 = SkillFilter::merge(m01, s2);

    // VType 0 has {A, B, C} -> missing D, E -> excess 2.
    CHECK(SkillFilter::excess(m012, sd, 0) == 2);

    // VType 1 has {A, B, C, D, E} -> ok.
    CHECK(SkillFilter::excess(m012, sd, 1) == 0);
}

TEST_CASE("SkillFilter: depot merge does not add requirements",
          "[skill_filter]")
{
    auto data = make_skill_instance();
    auto sd = SkillFilter::build_skill_data(data);

    auto depot = SkillFilter::init_depot();
    auto s0 = SkillFilter::init(sd, 0);

    auto merged = SkillFilter::merge(depot, s0);
    CHECK(merged.required == s0.required);

    auto merged2 = SkillFilter::merge(s0, depot);
    CHECK(merged2.required == s0.required);
}

TEST_CASE("SkillFilter: single client with subset of vehicle skills",
          "[skill_filter]")
{
    auto data = make_skill_instance();
    auto sd = SkillFilter::build_skill_data(data);

    // Client 0 requires cold_chain. VType 1 has cold_chain+hazmat (superset).
    auto s0 = SkillFilter::init(sd, 0);
    CHECK(SkillFilter::feasible(s0, sd, 1));
    CHECK(SkillFilter::excess(s0, sd, 1) == 0);
}
