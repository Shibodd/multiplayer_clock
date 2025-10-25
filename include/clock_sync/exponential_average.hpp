#include <cstddef>
#include <chrono>
#include <optional>

#include <clock_sync/utils/sample.hpp>

namespace clock_sync {

struct ExponentialAverage {
  using Timestamp = std::chrono::system_clock::time_point;
  using Duration = Timestamp::duration;

  using State = utils::Sample<Timestamp, Duration>;
  using Measurement = utils::Sample<Timestamp, Duration>;

  void push(const Measurement& z);
  void push(Timestamp timestamp, Duration value) { return push({timestamp, value}); }

  std::optional<State> state() const;

  ExponentialAverage(size_t min_samples, Duration max_age, std::chrono::duration<double> time_constant);

private:
  size_t m_min_samples;
  Duration m_max_age;
  double m_time_constant;

  State m_state;
  size_t m_num_samples = 0;
};

} // namespace clock_sync