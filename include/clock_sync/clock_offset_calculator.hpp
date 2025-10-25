#ifndef CLOCKSYNC_CLOCKOFFSETCALCULATOR_HPP
#define CLOCKSYNC_CLOCKOFFSETCALCULATOR_HPP

#include <chrono>
#include <iostream>

#include <clock_sync/types.hpp>
#include <clock_sync/utils/sample.hpp>
#include <clock_sync/exponential_average.hpp>

namespace clock_sync {

struct ClockOffsetCalculator {
  using Timepoint = std::chrono::system_clock::time_point;
  using Duration = Timepoint::duration;

  ClockOffsetCalculator(Duration max_age, size_t filter_min_samples, std::chrono::duration<double> filter_time_constant, std::ostream* rx_log = nullptr, std::ostream* tx_log = nullptr);

  void set_rx_logger(std::ostream* rx_log) { m_rx_log = rx_log; }
  void set_tx_logger(std::ostream* tx_log) { m_tx_log = tx_log; }

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
  std::ostream* m_rx_log;
  std::ostream* m_tx_log;
  Duration m_max_age;
  ExponentialAverage m_rx_delay;
  std::optional<utils::Sample<Timepoint, Duration>> m_tx_delay;
  std::optional<Duration> m_min_rx_delay;
};


} // namespace clock_sync

#endif // !CLOCKSYNC_CLOCKOFFSETCALCULATOR_HPP