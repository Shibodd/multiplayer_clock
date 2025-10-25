#ifndef CLOCKSYNC_SAMPLE_HPP
#define CLOCKSYNC_SAMPLE_HPP

namespace clock_sync::utils {

template <typename Timestamp, typename Value>
struct Sample {
  Timestamp timestamp {};
  Value value {};

  Sample() {}
  Sample(Timestamp timestamp, Value value)
    : timestamp(timestamp),
      value(value)
  {}
};

} // namespace clock_sync::utils

#endif // !CLOCKSYNC_SAMPLE_HPP