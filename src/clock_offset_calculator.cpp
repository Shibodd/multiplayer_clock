#include <clock_sync/clock_offset_calculator.hpp>

namespace clock_sync {

using Timepoint = ClockOffsetCalculator::Timepoint;
using Duration = ClockOffsetCalculator::Duration;

ClockOffsetCalculator::ClockOffsetCalculator(Duration max_age, size_t filter_min_samples, std::chrono::duration<double> filter_time_constant, std::ostream* rx_log, std::ostream* tx_log)
  : m_max_age(max_age),
    m_rx_delay(filter_min_samples, max_age, filter_time_constant),
    m_rx_log(rx_log),
    m_tx_log(tx_log)
{
  if (m_rx_log != nullptr) {
    (*m_rx_log) << "rx_stamp,tx_stamp,rx_delay,rx_delay_filtered\n";
  }
  if (m_rx_log != nullptr) {
    (*m_rx_log) << "rx_stamp,tx_delay\n";
  }
}

void ClockOffsetCalculator::on_our_rx_delay(Timepoint rx_stamp, Timepoint tx_stamp) {
  auto rx_delay = rx_stamp - tx_stamp;
  if (m_min_rx_delay.has_value()) {
    m_min_rx_delay = std::min(*m_min_rx_delay, rx_delay);
  } else {
    m_min_rx_delay = rx_delay;
  }
  m_rx_delay.push(rx_stamp, rx_delay);

  if (m_rx_log != nullptr) {
    (*m_rx_log)
      << rx_stamp.time_since_epoch().count() << ','
      << tx_stamp.time_since_epoch().count() << ','
      << rx_delay.count() << ',';

    if (auto state = m_rx_delay.state()) {
      (*m_rx_log) << state->value.count();
    } else {
      (*m_rx_log) << "-1";
    }

    (*m_rx_log) << '\n';
  }
}

void ClockOffsetCalculator::on_their_rx_delay(Timepoint rx_stamp, Duration min_tx_delay) {
  m_tx_delay.emplace(rx_stamp, min_tx_delay);

  if (m_tx_log != nullptr) {
    (*m_tx_log) << rx_stamp.time_since_epoch().count() << ',' << min_tx_delay.count() << '\n';
  }
}

std::optional<Duration> ClockOffsetCalculator::get_rx_delay(Timepoint now) const {
  if (auto del = m_min_rx_delay) {
    return *del;
  }
  return std::nullopt;

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