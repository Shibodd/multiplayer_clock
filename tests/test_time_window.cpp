#include <gtest/gtest.h>
#include <chrono>
#include <stdexcept>
#include <clock_sync/time_window.hpp>

using namespace std::chrono;

// Dummy timepoint type for testing
using TestTimepoint = system_clock::time_point;
using Duration = TestTimepoint::duration;

// ---------- TimeRange Tests ----------

TEST(TimeRangeTest, ConstructsCorrectlyWithSingleTimepoint) {
  TestTimepoint tp;
  TimeRange<TestTimepoint> range(tp);

  EXPECT_EQ(range.start, tp);
  EXPECT_EQ(range.end, tp);
  EXPECT_EQ(range.duration(), Duration::zero());
}

TEST(TimeRangeTest, ConstructsCorrectlyWithStartAndEnd) {
  TestTimepoint t1;
  TestTimepoint t2 = t1 + 10ms;

  TimeRange<TestTimepoint> range(t1, t2);

  EXPECT_EQ(range.start, t1);
  EXPECT_EQ(range.end, t2);
  EXPECT_EQ(range.duration(), 10ms);
}

TEST(TimeRangeTest, ContainsWorksProperly) {
  TestTimepoint t1;
  TestTimepoint t2 = t1 + 10ms;
  TimeRange<TestTimepoint> range(t1, t2);

  EXPECT_TRUE(range.contains(t1));
  EXPECT_TRUE(range.contains(t1 + 5ms));
  EXPECT_TRUE(range.contains(t2));
  EXPECT_FALSE(range.contains(t2 + 1ms));
  EXPECT_FALSE(range.contains(t1 - 1ms));
}

TEST(TimeRangeTest, TailAndHeadLimitRange) {
  TestTimepoint t1;
  TestTimepoint t2 = t1 + 20ms;
  TimeRange<TestTimepoint> range(t1, t2);

  range = range.tail(10ms);
  EXPECT_EQ(range, TimeRange(t2 - 10ms, t2));

  range = range.head(5ms);
  EXPECT_EQ(range, TimeRange(t2 - 10ms, t2 - 5ms));
}

TEST(TimeRangeTest, OperatorPlusMergesRanges) {
  TestTimepoint t1;
  TimeRange<TestTimepoint> a(t1, t1 + 10ms);
  TimeRange<TestTimepoint> b(t1 + 5ms, t1 + 20ms);

  TimeRange<TestTimepoint> c = a + b;

  EXPECT_EQ(c.start, t1);
  EXPECT_EQ(c.end, t1 + 20ms);

  EXPECT_EQ(a.start, t1);
  EXPECT_EQ(a.end, t1 + 10ms);
}

// ---------- TimeWindow Tests ----------

TEST(TimeWindowTest, ThrowsWhenCapacityZero) {
  using Tw = TimeWindow<TestTimepoint, int>;
  EXPECT_THROW(Tw(10ms, 0), std::invalid_argument);
}

TEST(TimeWindowTest, InitiallyHasNoRange) {
  TimeWindow<TestTimepoint, int> w(10ms, 3);
  EXPECT_EQ(w.size(), 0);
  EXPECT_FALSE(w.range().has_value());
}

TEST(TimeWindowTest, PushSingleSampleCreatesRange) {
  TimeWindow<TestTimepoint, int> w(10ms, 3);
  TestTimepoint t;

  decltype(w)::Sample sample { t, 42 };

  w.push(sample);
  EXPECT_EQ(w.size(), 1);
  EXPECT_EQ(w.at(0), sample);
  EXPECT_TRUE(w.test_invariants());

  auto r = w.range();
  ASSERT_TRUE(r.has_value());
  EXPECT_EQ(r->start, t);
  EXPECT_EQ(r->end, t);
}

TEST(TimeWindowTest, PushMultipleSamplesKeepsSorted) {
  TimeWindow<TestTimepoint, int> w(50ms, 5);
  using Sample = decltype(w)::Sample;
  TestTimepoint base;

  std::array<Sample, 3> samples {
    Sample{ base + 10ms, 0 },
    Sample{ base, 1 },
    Sample{ base + 5ms, 2 }
  };

  for (const auto& s : samples) {
    w.push(s);
  }

  ASSERT_EQ(w.size(), 3);
  EXPECT_EQ(w.at(0), samples.at(1));
  EXPECT_EQ(w.at(1), samples.at(2));
  EXPECT_EQ(w.at(2), samples.at(0));
  EXPECT_TRUE(w.test_invariants());

  ASSERT_TRUE(w.range().has_value());
  EXPECT_EQ(w.range()->start, base);
  EXPECT_EQ(w.range()->end, base + 10ms);
}

TEST(TimeWindowTest, MinValuedFindsCorrectSample) {
  TimeWindow<TestTimepoint, int> w(100ms, 5);
  using Sample = decltype(w)::Sample;
  TestTimepoint base;

  std::array<Sample, 3> samples {
    Sample { base, 50 },
    Sample { base + 10ms, 10 },
    Sample { base + 20ms, 100 }
  };

  for (const auto& s : samples) {
    w.push(s);
  }

  auto minSample = w.min_valued();
  ASSERT_TRUE(minSample.has_value());
  EXPECT_EQ(minSample->value, 10);
}

TEST(TimeWindowTest, MinValuedReturnsNulloptWhenEmpty) {
  TimeWindow<TestTimepoint, int> w(10ms, 2);
  EXPECT_FALSE(w.min_valued().has_value());
}

TEST(TimeWindowTest, MaintainsCapacityInvariant1) {
  TimeWindow<TestTimepoint, int> w(100ms, 3);
  using Sample = decltype(w)::Sample;
  TestTimepoint base;

  std::array<Sample, 4> samples {
    Sample { base, 0 }, // should be removed, being the oldest
    Sample { base + 10ms, 1 },
    Sample { base + 5ms, 2 },
    Sample { base + 7ms, 3 }
  };

  for (const auto& s : samples) {
    w.push(s);
  }

  ASSERT_EQ(w.size(), 3);
  EXPECT_EQ(w.at(0), samples.at(2));
  EXPECT_EQ(w.at(1), samples.at(3));
  EXPECT_EQ(w.at(2), samples.at(1));
  EXPECT_TRUE(w.test_invariants());
}

TEST(TimeWindowTest, MaintainsCapacityInvariant2) {
  TimeWindow<TestTimepoint, int> w(100ms, 3);
  using Sample = decltype(w)::Sample;
  TestTimepoint base;

  std::array<Sample, 4> samples {
    Sample { base + 10ms, 0 },
    Sample { base + 5ms, 1 },
    Sample { base + 7ms, 2 },
    Sample { base, 3 }, // will not be pushed, not fitting and being too old
  };

  for (const auto& s : samples) {
    w.push(s);
  }

  ASSERT_EQ(w.size(), 3);
  EXPECT_EQ(w.at(0), samples.at(1));
  EXPECT_EQ(w.at(1), samples.at(2));
  EXPECT_EQ(w.at(2), samples.at(0));
  EXPECT_TRUE(w.test_invariants());
}

TEST(TimeWindowTest, MaintainsDurationInvariant1) {
  TimeWindow<TestTimepoint, int> w(20ms, 10);
  using Sample = decltype(w)::Sample;
  TestTimepoint base;

  std::array<Sample, 4> samples {
    Sample { base + 10ms, 0 },
    Sample { base, 1 }, // should be removed
    Sample { base + 5ms, 2 },
    Sample { base + 25ms, 3 }
  };

  for (const auto& s : samples) {
    w.push(s);
  }

  ASSERT_EQ(w.size(), 3);
  EXPECT_EQ(w.at(0), samples.at(2));
  EXPECT_EQ(w.at(1), samples.at(0));
  EXPECT_EQ(w.at(2), samples.at(3));
  EXPECT_TRUE(w.test_invariants());
}

TEST(TimeWindowTest, MaintainsDurationInvariant2) {
  TimeWindow<TestTimepoint, int> w(20ms, 10);
  using Sample = decltype(w)::Sample;
  TestTimepoint base;

  std::array<Sample, 4> samples {
    Sample { base + 10ms, 0 },
    Sample { base + 5ms, 1 },
    Sample { base + 25ms, 2 },
    Sample { base, 3 }, // should not be pushed, not fitting and being too old
  };

  for (const auto& s : samples) {
    w.push(s);
  }

  ASSERT_EQ(w.size(), 3);
  EXPECT_EQ(w.at(0), samples.at(1));
  EXPECT_EQ(w.at(1), samples.at(0));
  EXPECT_EQ(w.at(2), samples.at(2));
  EXPECT_TRUE(w.test_invariants());
}

TEST(TimeWindowTest, DiscardBeforeWorks) {
  TimeWindow<TestTimepoint, int> w(200ms, 10);
  using Sample = decltype(w)::Sample;
  TestTimepoint base;

  std::array<Sample, 4> samples {
    Sample { base + 10ms, 0 },
    Sample { base + 5ms, 1 },
    Sample { base + 25ms, 2 },
    Sample { base, 3 },
  };

  for (const auto& s : samples) {
    w.push(s);
  }

  w.discard_before(base + 10ms);

  ASSERT_EQ(w.size(), 2);
  EXPECT_EQ(w.at(0), samples.at(0));
  EXPECT_EQ(w.at(1), samples.at(2));
  EXPECT_TRUE(w.test_invariants());
}
