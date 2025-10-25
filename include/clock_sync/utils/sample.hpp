#ifndef CLOCKSYNC_SAMPLE_HPP
#define CLOCKSYNC_SAMPLE_HPP

namespace clock_sync::utils {

template <typename Timepoint, typename Value>
struct Sample {
  Timepoint timestamp {};
  Value value {};

  Sample() {}
  Sample(Timepoint timestamp, Value value)
    : timestamp(timestamp),
      value(value)
  {}
};


template <typename Timepoint, typename Value>
struct SampleTimestampLess {
  bool operator()(const Sample<Timepoint, Value>& a, const Sample<Timepoint, Value>& b) {
    return a.timestamp < b.timestamp;
  }
};
template <typename Timepoint, typename Value>
struct SampleValueLess {
  bool operator()(const Sample<Timepoint, Value>& a, const Sample<Timepoint, Value>& b) {
    return a.value < b.value;
  }
};

template <typename Timepoint, typename Value>
struct SampleTimestampScalarLess {
  bool operator()(const Sample<Timepoint, Value>& a, const Timepoint& ts) {
    return a.timestamp < ts;
  }
};
template <typename Timepoint, typename Value>
struct SampleValueScalarLess {
  bool operator()(const Sample<Timepoint, Value>& a, const Timepoint& value) {
    return a.value < value;
  }
};

} // namespace clock_sync::utils

#endif // !CLOCKSYNC_SAMPLE_HPP