#include <algorithm>
#include <numeric>
#include <stdexcept>
#include <iostream>

#include <clock_sync/exponential_average.hpp>

namespace clock_sync {

void ExponentialAverage::push(const ExponentialAverage::Measurement& z) {
  if (m_state.has_value()) {
    auto& state = *m_state;
    Duration dt = z.timestamp - state.timestamp;
    
    // Drop old out of order measurements
    if (dt < Duration(0)) {
      return;
    }
    
    // If state is not too old
    if (dt <= m_max_age) {
      // Compute exp average
      double dt_dbl = std::chrono::duration<double>(z.timestamp - state.timestamp).count();
      double alpha = 1.0;
      if (m_time_constant > 1e-9) {
        alpha = std::min(dt_dbl / m_time_constant, 1.0);
      }
      double z_dbl = std::chrono::duration<double>(z.value).count();
      double x_dbl = std::chrono::duration<double>(state.value).count();

      state.timestamp = z.timestamp;
      state.value = std::chrono::duration_cast<Duration>(
        std::chrono::duration<double>(
          alpha * z_dbl + (1 - alpha) * x_dbl
        )
      );

      return;
    }

    // State is too old - recompute
    m_state.reset();
  }

  m_init_window.push(z);

  auto begin = m_init_window.begin();
  auto end = m_init_window.end();
  size_t min_num_samples = m_init_window.capacity();

  if (std::distance(begin, end) >= min_num_samples) {
    std::chrono::duration<double> value = std::accumulate(
      begin, end, std::chrono::duration<double>{},
      [min_num_samples](const std::chrono::duration<double>& acc, const ExponentialAverage::Measurement& sample) {
        return acc + std::chrono::duration<double>(sample.value) / static_cast<double>(min_num_samples);
      }
    );

    m_state.emplace(
      z.timestamp,
      std::chrono::duration_cast<Duration>(value)
    );
  }
}

ExponentialAverage::ExponentialAverage(size_t min_samples, Duration max_age, std::chrono::duration<double> time_constant)
  : m_init_window(min_samples, max_age),
    m_max_age(max_age),
    m_time_constant(time_constant.count())
{
  if (m_time_constant < 0.0) {
    throw std::invalid_argument("ExponentialAverage time constant must be nonnegative.");
  }
}

} // namespace clock_sync