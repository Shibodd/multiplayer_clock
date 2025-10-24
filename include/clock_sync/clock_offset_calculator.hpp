#ifndef CLOCKSYNC_CLOCKOFFSETCALCULATOR_HPP
#define CLOCKSYNC_CLOCKOFFSETCALCULATOR_HPP

#include <chrono>
#include <clock_sync/time_window.hpp>

struct ClockOffsetCalculator {
  using Clock = std::chrono::system_clock;
  using Duration = Clock::duration;
  using Timepoint = Clock::time_point;

  ClockOffsetCalculator(Duration window_duration, size_t window_capacity)
    : m_rx_delay(window_duration, window_capacity)
  {}

  /**
  @param rx_stamp our reception stamp for this message
  @param tx_stamp their transmission stamp for this message
  */
  void on_message_rx_delay(Timepoint rx_stamp, Timepoint tx_stamp) {
    m_rx_delay.push(rx_stamp, rx_stamp - tx_stamp);
  }

  /**
  @param rx_delay our reception stamp for this information
  @param min_tx_delay their latest min_rx_delay
  */
  void on_their_min_rx_delay(Timepoint rx_stamp, Duration min_tx_delay) {
    m_min_tx_delay.emplace(rx_stamp, min_tx_delay);
  }

  /**
  @return our min_rx_delay
  */
  std::optional<Duration> get_min_rx_delay() const {
    if (auto sample = m_rx_delay.min_valued()) {
      return sample->value;
    } else {
      return std::nullopt;
    }
  }

  /**
  @return their latest min_rx_delay
  */
  std::optional<Duration> get_min_tx_delay(std::chrono::system_clock::time_point now) const {
    if (m_min_tx_delay.has_value()) {
      auto age = now - m_min_tx_delay->timestamp;
      if (age <= m_rx_delay.window_duration()) {
        return m_min_tx_delay->value;
      } else {
        return std::nullopt;
      }
    } else {
      return std::nullopt;
    }
  }

  /**
  @return clock offset "delta" s.t. their_t = our_t + delta
  */
  std::optional<Duration> clock_offset(std::chrono::system_clock::time_point now) const {
    auto min_rx_delay = get_min_rx_delay();
    auto min_tx_delay = get_min_tx_delay(now);

    if (min_rx_delay.has_value() && min_tx_delay.has_value()) {
      return (*min_tx_delay - *min_rx_delay) / 2;
    } else {
      return std::nullopt;
    }
  }

  size_t window_size() const { return m_rx_delay.size(); }

private:
  std::optional<TimeWindow<Timepoint, Duration>::Sample> m_min_tx_delay;
  TimeWindow<Timepoint, Duration> m_rx_delay;
};

#endif // !CLOCKSYNC_CLOCKOFFSETCALCULATOR_HPP