#ifndef CLOCKSYNC_CLOCKSYNC_HPP
#define CLOCKSYNC_CLOCKSYNC_HPP

#include <mutex>
#include <boost/container/flat_map.hpp>

#include <clock_sync/messages.hpp>
#include <clock_sync/clock_offset_calculator.hpp>

namespace clock_sync {

struct ClockSync {
  struct Peer {
    unsigned char prev_rx_message_id;
    ClockOffsetCalculator::Timepoint prev_rx_timestamp;
    ClockOffsetCalculator calculator; 

    Peer(unsigned char rx_message_id,
         ClockOffsetCalculator::Timepoint prev_rx_ts,
         ClockOffsetCalculator::Duration max_age,
         size_t filter_min_samples,
         std::chrono::duration<double> filter_time_constant,
         std::optional<ClockOffsetCalculator::Logger> log = std::nullopt);
  };

  void on_message_rx(const ClockSyncMessage& msg, ClockOffsetCalculator::Timepoint rx_timestamp);

  ClockSyncMessage on_message_tx(std::chrono::system_clock::time_point now);

  void store_tx_timestamp(ClockOffsetCalculator::Timepoint tx_timestamp);

  std::optional<ClockOffsetCalculator::Duration> get_offset(unsigned char other_player_id, std::chrono::system_clock::time_point now);

  ClockSync(unsigned char player_id, ClockOffsetCalculator::Duration calculator_max_age, size_t calculator_min_samples, std::chrono::duration<double> calculator_time_constant, std::ostream* log = nullptr);
private:
  std::mutex m_mtx;
  std::vector<PeerMessagePart> m_peers_buffer;
  boost::container::flat_map<unsigned char, Peer> m_peers;
  
  std::optional<ClockOffsetCalculator::Timepoint> m_prev_tx_stamp;
  unsigned short m_message_id;
  unsigned char m_player_id;

  std::ostream* m_log;

  ClockOffsetCalculator::Duration m_calculator_max_age;
  size_t m_calculator_min_samples;
  std::chrono::duration<double> m_calculator_time_constant;
};

  
} // namespace clock_sync

#endif // !CLOCKSYNC_CLOCKSYNC_HPP