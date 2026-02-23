#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "search/acceptance.h"

using namespace coso;

// --------------------------------------------------------------------------- //
//  LateAcceptance                                                              //
// --------------------------------------------------------------------------- //

TEST_CASE("LateAcceptance — accepts improving moves", "[acceptance]")
{
    LateAcceptance la(10);
    la.init(100);

    // Candidate better than current: always accepted.
    CHECK(la.accept(90, 100));
    CHECK(la.accept(50, 100));
}

TEST_CASE("LateAcceptance — accepts equal cost", "[acceptance]")
{
    LateAcceptance la(10);
    la.init(100);

    CHECK(la.accept(100, 100));
}

TEST_CASE("LateAcceptance — accepts worsening if within list", "[acceptance]")
{
    LateAcceptance la(5);
    la.init(100);

    // The list is [100, 100, 100, 100, 100].
    // A candidate of 100 when current is 90 should be accepted (100 <= 100).
    CHECK(la.accept(100, 90));

    // A candidate of 101 when current is 90: 101 > 100 (list) and 101 > 90.
    CHECK_FALSE(la.accept(101, 90));
}

TEST_CASE("LateAcceptance — list evolves over iterations", "[acceptance]")
{
    LateAcceptance la(3);
    la.init(100);
    // list: [100, 100, 100], index=0

    // Iterate with improving costs.
    la.iteration(90);   // list: [90, 100, 100], index=1
    la.iteration(80);   // list: [90, 80, 100], index=2
    la.iteration(70);   // list: [90, 80, 70], index=0

    // Now index is 0, list[0]=90. Candidate 85 <= 90 => accepted.
    CHECK(la.accept(85, 70));

    // Candidate 95: 95 > 90 and 95 > 70 => rejected.
    CHECK_FALSE(la.accept(95, 70));
}

// --------------------------------------------------------------------------- //
//  SimulatedAnnealing                                                          //
// --------------------------------------------------------------------------- //

TEST_CASE("SimulatedAnnealing — always accepts improvements", "[acceptance]")
{
    SimulatedAnnealing sa(100.0, 0.99);
    sa.init(100);

    CHECK(sa.accept(90, 100));
    CHECK(sa.accept(50, 100));
}

TEST_CASE("SimulatedAnnealing — probabilistically accepts worsening",
          "[acceptance]")
{
    // High temperature: should accept most worsening moves.
    SimulatedAnnealing sa(10000.0, 0.99, 123);
    sa.init(100);

    int accepted = 0;
    int const trials = 1000;
    for (int i = 0; i < trials; ++i) {
        if (sa.accept(110, 100))
            ++accepted;
    }

    // With T=10000 and delta=10, prob = exp(-10/10000) ~ 0.999.
    CHECK(accepted > 900);
}

TEST_CASE("SimulatedAnnealing — low temperature rejects worsening",
          "[acceptance]")
{
    SimulatedAnnealing sa(0.001, 0.99, 42);
    sa.init(100);

    int accepted = 0;
    int const trials = 1000;
    for (int i = 0; i < trials; ++i) {
        if (sa.accept(200, 100))
            ++accepted;
    }

    // With T=0.001 and delta=100, prob ~ 0.
    CHECK(accepted < 10);
}

TEST_CASE("SimulatedAnnealing — temperature decays", "[acceptance]")
{
    SimulatedAnnealing sa(100.0, 0.5);
    sa.init(100);

    CHECK_THAT(sa.temperature(),
               Catch::Matchers::WithinAbs(100.0, 1e-9));

    sa.iteration(100);
    CHECK_THAT(sa.temperature(),
               Catch::Matchers::WithinAbs(50.0, 1e-9));

    sa.iteration(100);
    CHECK_THAT(sa.temperature(),
               Catch::Matchers::WithinAbs(25.0, 1e-9));
}

TEST_CASE("SimulatedAnnealing — zero temperature rejects all worsening",
          "[acceptance]")
{
    SimulatedAnnealing sa(100.0, 0.0);  // alpha=0 => temp becomes 0 after 1 iter
    sa.init(100);
    sa.iteration(100);

    CHECK_THAT(sa.temperature(),
               Catch::Matchers::WithinAbs(0.0, 1e-15));

    // Worsening: rejected.
    CHECK_FALSE(sa.accept(101, 100));
    // Improving: still accepted.
    CHECK(sa.accept(99, 100));
}

// --------------------------------------------------------------------------- //
//  RecordToRecord                                                              //
// --------------------------------------------------------------------------- //

TEST_CASE("RecordToRecord — accepts within threshold of best", "[acceptance]")
{
    RecordToRecord rtr(10.0, 1.0);
    rtr.init(100);

    // Candidate 109 <= 100 + 10 = 110: accepted.
    CHECK(rtr.accept(109, 100));

    // Candidate 111 > 110: rejected.
    CHECK_FALSE(rtr.accept(111, 100));
}

TEST_CASE("RecordToRecord — tracks best cost", "[acceptance]")
{
    RecordToRecord rtr(10.0, 0.0);  // no decay
    rtr.init(100);

    // Improving candidate updates best.
    CHECK(rtr.accept(90, 100));
    // Now best is 90. Candidate 101 <= 90 + 10 = 100: accepted.
    CHECK(rtr.accept(100, 90));
    // Candidate 101 > 100: rejected.
    CHECK_FALSE(rtr.accept(101, 90));
}

TEST_CASE("RecordToRecord — threshold decays linearly", "[acceptance]")
{
    RecordToRecord rtr(10.0, 2.0);
    rtr.init(100);

    CHECK_THAT(rtr.threshold(),
               Catch::Matchers::WithinAbs(10.0, 1e-9));

    rtr.iteration(100);
    CHECK_THAT(rtr.threshold(),
               Catch::Matchers::WithinAbs(8.0, 1e-9));

    rtr.iteration(100);
    CHECK_THAT(rtr.threshold(),
               Catch::Matchers::WithinAbs(6.0, 1e-9));

    // Decay to zero and stay there.
    for (int i = 0; i < 10; ++i)
        rtr.iteration(100);

    CHECK_THAT(rtr.threshold(),
               Catch::Matchers::WithinAbs(0.0, 1e-9));
}

// --------------------------------------------------------------------------- //
//  AcceptanceCriterion (variant wrapper)                                        //
// --------------------------------------------------------------------------- //

TEST_CASE("AcceptanceCriterion — wraps LateAcceptance", "[acceptance]")
{
    AcceptanceCriterion ac(LateAcceptance(5));
    ac.init(100);

    CHECK(ac.accept(95, 100));
    CHECK_FALSE(ac.accept(105, 90));

    ac.iteration(95);
}

TEST_CASE("AcceptanceCriterion — wraps SimulatedAnnealing", "[acceptance]")
{
    AcceptanceCriterion ac(SimulatedAnnealing(100.0, 0.99));
    ac.init(100);

    CHECK(ac.accept(90, 100));
    ac.iteration(90);
}

TEST_CASE("AcceptanceCriterion — wraps RecordToRecord", "[acceptance]")
{
    AcceptanceCriterion ac(RecordToRecord(10.0, 1.0));
    ac.init(100);

    CHECK(ac.accept(109, 100));
    CHECK_FALSE(ac.accept(111, 100));
    ac.iteration(100);
}
