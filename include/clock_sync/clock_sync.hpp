#ifndef CLOCKSYNC_CLOCKSYNC_HPP
#define CLOCKSYNC_CLOCKSYNC_HPP

#include <mutex>
#include <boost/container/flat_map.hpp>

#include <fstream>
#include <filesystem>
#include <clock_sync/messages.hpp>
#include <clock_sync/clock_offset_calculator.hpp>

namespace clock_sync {

struct ClockSync {
  struct Peer {
    player_id_t id;
    message_id_t m_prev_rx_msg_id;
    ClockOffsetCalculator::Timepoint m_prev_rx_timestamp;
    ClockOffsetCalculator m_calculator;
    struct LogFiles {
      std::ofstream m_tx_log;
      std::ofstream m_rx_log;
    };
    std::unique_ptr<LogFiles> m_logger;

    Peer(player_id_t id,
      message_id_t rx_message_id,
      ClockOffsetCalculator::Timepoint prev_rx_ts,
      const ClockOffsetCalculator::Filter::Params& filter_params,
      const std::filesystem::path& log_directory = {});
  };

  void on_message_rx(const ClockSyncMessage& msg, ClockOffsetCalculator::Timepoint rx_timestamp);

  ClockSyncMessage on_message_tx(std::chrono::system_clock::time_point now);

  std::optional<ClockOffsetCalculator::Duration> get_offset(player_id_t other_player_id, std::chrono::system_clock::time_point now);

  ClockSync(player_id_t player_id, const ClockOffsetCalculator::Filter::Params& calculator_filter_params, std::ostream* log = nullptr, std::filesystem::path log_directory = std::filesystem::path{});
private:
  std::mutex m_mtx;
  std::vector<PeerMessagePart> m_peers_buffer;
  boost::container::flat_map<unsigned char, Peer> m_peers;
  
  std::optional<ClockOffsetCalculator::Timepoint> m_prev_tx_stamp;
  message_id_t m_message_id;
  player_id_t m_player_id;

  std::ostream* m_log;
  std::optional<std::ofstream> m_file_log;
  std::optional<std::ofstream> m_offset_file_log;
  std::filesystem::path m_log_directory;

  ClockOffsetCalculator::Filter::Params m_calculator_filter_params;
};

  
} // namespace clock_sync

#endif // !CLOCKSYNC_CLOCKSYNC_HPP