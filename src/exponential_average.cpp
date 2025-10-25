#include <algorithm>
#include <stdexcept>

#include <clock_sync/exponential_average.hpp>

namespace clock_sync {

void ExponentialAverage::push(const ExponentialAverage::Measurement& z) {
  if (m_num_samples == 0) {
    // Init state
    m_state.timestamp = z.timestamp;
    m_state.value = z.value;
  } else {
    Duration dt = z.timestamp - m_state.timestamp;
    
    // Drop old out of order measurements
    if (dt < Duration(0)) {
      return;
    }
    
    if (dt > m_max_age) {
      // State expired - reinit
      m_num_samples = 0;
      m_state.timestamp = z.timestamp;
      m_state.value = z.value;
    } else {
      // Compute exp average
      double dt_dbl = std::chrono::duration<double>(z.timestamp - m_state.timestamp).count();
      double alpha = 1.0;
      if (m_time_constant > 1e-9) {
        alpha = std::min(dt_dbl / m_time_constant, 1.0);
      }
      double z_dbl = std::chrono::duration<double>(z.value).count();
      double x_dbl = std::chrono::duration<double>(m_state.value).count();

      m_state.timestamp = z.timestamp;
      m_state.value = std::chrono::duration_cast<Duration>(
        std::chrono::duration<double>(
          alpha * z_dbl + (1 - alpha) * x_dbl
        )
      );
    }
  }
  ++m_num_samples;
}

std::optional<ExponentialAverage::State> ExponentialAverage::state() const {
  if (m_num_samples < m_min_samples) {
    return std::nullopt;
  }
  return m_state;
}

ExponentialAverage::ExponentialAverage(size_t min_samples, Duration max_age, std::chrono::duration<double> time_constant)
  : m_min_samples(min_samples),
    m_max_age(max_age),
    m_time_constant(time_constant.count())
{
  if (m_time_constant < 0.0) {
    throw std::invalid_argument("ExponentialAverage time constant must be nonnegative.");
  }
}

} // namespace clock_sync