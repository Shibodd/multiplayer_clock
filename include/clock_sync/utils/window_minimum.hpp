#ifndef CLOCKSYNC_WINDOWMINIMUM_HPP
#define CLOCKSYNC_WINDOWMINIMUM_HPP

#include <optional>

#include <clock_sync/utils/sample.hpp>
#include <clock_sync/utils/measurement_window.hpp>

namespace clock_sync::utils {

template <typename Timepoint, typename Value>
struct WindowMinimum {
  using Measurement = Sample<Timepoint, Value>;
  using Duration = typename Timepoint::duration;

  void push(Timepoint t, Duration z) { push({t, z}); }
  void push(Measurement z) {
    m_chunk.push(z);

    if (std::distance(m_chunk.begin(), m_chunk.end()) >= m_chunk.capacity()) {
      if (auto min = window_get_minimum(m_chunk)) {
        m_window.push(*min);
      }
      m_chunk.clear();
    }
  }

  std::optional<Measurement> minimum() {
    std::optional<Measurement> chunk_min = window_get_minimum(m_chunk);
    std::optional<Measurement> window_min = window_get_minimum(m_window);

    if (!chunk_min.has_value()) {
      return window_min;
    }
    if (!window_min.has_value()) {
      return chunk_min;
    }

    if (chunk_min->value <= window_min->value) {
      return *chunk_min;
    } else {
      return *window_min;
    }
  }

  struct Params {
    size_t chunk_size;
    Duration duration;
    size_t window_size;
  };

  WindowMinimum(const Params& params)
    : m_window(params.window_size, params.duration),
      m_chunk(params.chunk_size, params.duration)
  {
    if (params.window_size < 1) {
      throw std::invalid_argument("WindowMinimum window size must be gte 1!");
    }

    if (params.chunk_size < 1) {
      throw std::invalid_argument("WindowMinimum chunk size must be gte 1!");
    }
  }

private:
  static std::optional<Measurement> window_get_minimum(MeasurementWindow<Timepoint, Value>& window) {
    auto old_begin = window.begin();
    auto old_end = window.end();

    if (old_begin == old_end) {
      return std::nullopt;
    }

    auto new_begin = std::min_element(old_begin, old_end, SampleValueLess<Timepoint, Value>{});
    new_begin = window.drop_before(new_begin);

    auto new_back = window.end() - 1;
    return Measurement { new_back->timestamp, new_begin->value };
  }

  MeasurementWindow<Timepoint, Value> m_window;
  MeasurementWindow<Timepoint, Value> m_chunk;
};

} // namespace clock_sync

#endif // !CLOCKSYNC_WINDOWMINIMUM_HPP