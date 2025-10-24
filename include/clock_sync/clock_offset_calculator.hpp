#ifndef CLOCKSYNC_CLOCKOFFSETCALCULATOR_HPP
#define CLOCKSYNC_CLOCKOFFSETCALCULATOR_HPP

#include <chrono>
#include <clock_sync/filter.hpp>
#include <clock_sync/time_window.hpp>

template <typename Timepoint, typename Duration>
auto duration_exponential_average(double time_constant) {
  return [time_constant](const typename Filter<Timepoint, Duration, Duration>::Accessor& acc, Timepoint timestamp, Duration measurement) -> std::optional<Duration> {
    if (auto prev = acc.rat(1)) {
      if (prev->value.output.has_value()) {
        double dt = std::chrono::duration<double>(timestamp - prev->timestamp).count();
        double alpha = std::clamp(dt / time_constant, 0.0, 1.0);
        double ynm1_dbl = std::chrono::duration<double>(*prev->value.output).count();
        double zn_dbl = std::chrono::duration<double>(measurement).count();
        return std::chrono::duration_cast<Duration>(
          std::chrono::duration<double>(alpha * zn_dbl + (1 - alpha) * ynm1_dbl)
        );
      }
    }
    return measurement;
  };
}

struct ClockOffsetCalculator {
  using Clock = std::chrono::system_clock;
  using Duration = Clock::duration;
  using Timepoint = Clock::time_point;

  ClockOffsetCalculator(size_t history_capacity, Duration max_age, std::chrono::duration<double> time_constant)
    : m_max_age(max_age),
      m_rx_delay(history_capacity, duration_exponential_average<Timepoint, Duration>(time_constant.count()))
  {}

  /**
  @param rx_stamp our reception stamp for this message
  @param tx_stamp their transmission stamp for this message
  */
  void on_our_rx_delay(Timepoint rx_stamp, Timepoint tx_stamp) {
    auto rx_delay = rx_stamp - tx_stamp;
    m_rx_delay.push(rx_stamp, rx_delay);
  }

  /**
  @param rx_delay our reception stamp for this information
  @param min_tx_delay their latest min_rx_delay
  */
  void on_their_rx_delay(Timepoint rx_stamp, Duration min_tx_delay) {
    m_tx_delay.emplace(rx_stamp, min_tx_delay);
  }

  /**
  @return our rx_delay
  */
  std::optional<Duration> get_rx_delay(Timepoint now) const {
    if (auto output = m_rx_delay.output()) {
      if (now - output->timestamp <= m_max_age) {
        std::cerr << output->value.count() << std::endl;
        return output->value;
      }
    }
    return std::nullopt;
  }

  /**
  @return their rx_delay
  */
  std::optional<Duration> get_tx_delay(Timepoint now) const {
    if (m_tx_delay.has_value()) {
      if (now - m_tx_delay->timestamp <= m_max_age) {
        return m_tx_delay->value;
      }
    }    
    return std::nullopt;
  }

  /**
  @return clock offset "delta" s.t. their_t = our_t + delta
  */
  std::optional<Duration> clock_offset(std::chrono::system_clock::time_point now) const {
    auto min_rx_delay = get_rx_delay(now);
    auto min_tx_delay = get_tx_delay(now);

    if (min_rx_delay.has_value() && min_tx_delay.has_value()) {
      return (*min_tx_delay - *min_rx_delay) / 2;
    } else {
      return std::nullopt;
    }
  }

private:
  Duration m_max_age;
  Filter<Timepoint, Duration, Duration> m_rx_delay;
  std::optional<TimeWindow<Timepoint, Duration>::Sample> m_tx_delay;
};

#endif // !CLOCKSYNC_CLOCKOFFSETCALCULATOR_HPP