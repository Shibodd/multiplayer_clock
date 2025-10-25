#include <clock_sync/clock_offset_calculator.hpp>

namespace clock_sync {

using Timepoint = ClockOffsetCalculator::Timepoint;
using Duration = ClockOffsetCalculator::Duration;

ClockOffsetCalculator::ClockOffsetCalculator(Duration max_age, size_t filter_min_samples, std::chrono::duration<double> filter_time_constant, std::optional<Logger> log)
  : m_max_age(max_age),
    m_rx_delay(filter_min_samples, max_age, filter_time_constant),
    m_log(log)
{
  if (m_log.has_value()) {
    if (m_log->rx != nullptr) {
      (*m_log->rx) << "rx_stamp,tx_stamp,rx_delay,rx_delay_filtered\n";
    }
    if (m_log->rx != nullptr) {
      (*m_log->rx) << "rx_stamp,tx_delay\n";
    }
  }
}

void ClockOffsetCalculator::on_our_rx_delay(Timepoint rx_stamp, Timepoint tx_stamp) {
  auto rx_delay = rx_stamp - tx_stamp;
  m_rx_delay.push(rx_stamp, rx_delay);

  if (m_log.has_value()) {
    if (m_log->rx != nullptr) {
      (*m_log->rx)
        << rx_stamp.time_since_epoch().count() << ','
        << tx_stamp.time_since_epoch().count() << ','
        << rx_delay.count() << ',';

      if (auto state = m_rx_delay.state()) {
        (*m_log->rx) << state->value.count();
      } else {
        (*m_log->rx) << "-1";
      }

      (*m_log->rx) << '\n';
    }
  }
}

void ClockOffsetCalculator::on_their_rx_delay(Timepoint rx_stamp, Duration min_tx_delay) {
  m_tx_delay.emplace(rx_stamp, min_tx_delay);

  if (m_log->tx != nullptr) {
    (*m_log->tx) << rx_stamp.time_since_epoch().count() << ',' << min_tx_delay.count() << '\n';
  }
}

std::optional<Duration> ClockOffsetCalculator::get_rx_delay(Timepoint now) const {
  if (auto state = m_rx_delay.state()) {
    if (now - state->timestamp <= m_max_age) {
      return state->value;
    }
  }
  return std::nullopt;
}

std::optional<Duration> ClockOffsetCalculator::get_tx_delay(Timepoint now) const {
  if (m_tx_delay.has_value()) {
    if (now - m_tx_delay->timestamp <= m_max_age) {
      return m_tx_delay->value;
    }
  }    
  return std::nullopt;
}

std::optional<Duration> ClockOffsetCalculator::clock_offset(Timepoint now) const {
  auto min_rx_delay = get_rx_delay(now);
  auto min_tx_delay = get_tx_delay(now);

  if (min_rx_delay.has_value() && min_tx_delay.has_value()) {
    return (*min_tx_delay - *min_rx_delay) / 2;
  } else {
    return std::nullopt;
  }
}

} // namespace clock_sync