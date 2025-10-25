#ifndef CLOCKSYNC_MEASUREMENTWINDOW_HPP
#define CLOCKSYNC_MEASUREMENTWINDOW_HPP

#include <boost/circular_buffer.hpp>
#include <clock_sync/utils/sample.hpp>

namespace clock_sync {

template <typename Timepoint, typename Value>
struct MeasurementWindow {
  using sample = utils::Sample<Timepoint, Value>;
  using duration = typename Timepoint::duration;
  using buffer = boost::circular_buffer<sample>;
  using iterator = typename buffer::iterator;

  void push(const sample& sample) {
    if (not m_buffer.empty() && sample.timestamp < m_buffer.back().timestamp) {
      return;
    }
    m_buffer.push_back(sample);
  }
  void push(Timepoint timestamp, Value value) { return push({ timestamp, value }); }

  iterator begin() {
    auto old_end = end();
    if (m_buffer.empty()) {
      return old_end;
    }
    
    // Find first non-expired measurement
    auto most_recent_timestamp = m_buffer.back().timestamp;
    auto begin = m_buffer.begin();
    while (begin != old_end && begin->timestamp + m_window_duration < most_recent_timestamp) {
      ++begin;
    }
    
    // Drop all expired measurements
    return m_buffer.erase(m_buffer.begin(), begin);
  }
  
  iterator end() {
    return m_buffer.end();
  }

  std::size_t capacity() const { return m_buffer.capacity(); }

  MeasurementWindow(std::size_t capacity, duration duration)
    : m_buffer(capacity), 
      m_window_duration(duration)
  {}

private:
  duration m_window_duration;
  buffer m_buffer;
};

} // namespace clock_sync

#endif // !CLOCKSYNC_MEASUREMENTWINDOW_HPP