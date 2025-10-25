#include <cstddef>
#include <chrono>
#include <optional>

#include <clock_sync/utils/sample.hpp>
#include <clock_sync/measurement_window.hpp>

namespace clock_sync {

struct ExponentialAverage {
  using Timestamp = std::chrono::system_clock::time_point;
  using Duration = Timestamp::duration;

  using State = utils::Sample<Timestamp, Duration>;
  using Measurement = utils::Sample<Timestamp, Duration>;

  void push(const Measurement& z);
  void push(Timestamp timestamp, Duration value) { return push({timestamp, value}); }

  std::optional<State> state() const { return m_state; }

  ExponentialAverage(size_t min_samples, Duration max_age, std::chrono::duration<double> time_constant);

private:
  Duration m_max_age;
  double m_time_constant;

  std::optional<State> m_state;
  MeasurementWindow<Timestamp, Duration> m_init_window;
};

} // namespace clock_sync