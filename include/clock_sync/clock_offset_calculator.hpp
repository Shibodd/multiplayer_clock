#ifndef CLOCKSYNC_CLOCKOFFSETCALCULATOR_HPP
#define CLOCKSYNC_CLOCKOFFSETCALCULATOR_HPP

#include <chrono>
#include <iostream>

#include <clock_sync/utils/sample.hpp>
#include <clock_sync/exponential_average.hpp>

namespace clock_sync {

struct ClockOffsetCalculator {
  using Timepoint = std::chrono::system_clock::time_point;
  using Duration = Timepoint::duration;

  struct Logger {
    std::ostream* rx;
    std::ostream* tx;
    unsigned int id;
  };

  ClockOffsetCalculator(Duration max_age, size_t filter_min_samples, std::chrono::duration<double> filter_time_constant, std::optional<Logger> log);

  void set_logger(std::optional<Logger> log) { m_log = log; }

  /**
  @param rx_stamp our reception stamp for this message
  @param tx_stamp their transmission stamp for this message
  */
  void on_our_rx_delay(Timepoint rx_stamp, Timepoint tx_stamp);

  /**
  @param rx_delay our reception stamp for this information
  @param min_tx_delay their latest min_rx_delay
  */
  void on_their_rx_delay(Timepoint rx_stamp, Duration min_tx_delay);

  /**
  @return our rx_delay
  */
  std::optional<Duration> get_rx_delay(Timepoint now) const;

  /**
  @return their rx_delay
  */
  std::optional<Duration> get_tx_delay(Timepoint now) const;

  /**
  @return clock offset "delta" s.t. their_t = our_t + delta
  */
  std::optional<Duration> clock_offset(Timepoint now) const;

private:
  std::optional<Logger> m_log;
  Duration m_max_age;
  ExponentialAverage m_rx_delay;
  std::optional<utils::Sample<Timepoint, Duration>> m_tx_delay;
};


} // namespace clock_sync

#endif // !CLOCKSYNC_CLOCKOFFSETCALCULATOR_HPP