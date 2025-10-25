#include <clock_sync/clock_sync.hpp>

#define _CLSYN_LOG(os, x) (os) << "ClockSync: " << x << '\n'

#define CLSYN_LOG(x) \
  do { \
    if (m_log != nullptr) { \
      _CLSYN_LOG(*m_log, x); \
    } \
    if (m_file_log.has_value()) { \
      _CLSYN_LOG(*m_file_log, x); \
    } \
  } while(false)

namespace clock_sync {

ClockSync::Peer::Peer(
      player_id_t id,
      message_id_t rx_message_id,
      ClockOffsetCalculator::Timepoint prev_rx_ts,
      ClockOffsetCalculator::Duration max_age,
      std::size_t filter_min_samples,
      std::chrono::duration<double> filter_time_constant,
      const std::filesystem::path& log_directory)
  : m_prev_rx_msg_id(rx_message_id),
    m_prev_rx_timestamp(prev_rx_ts),
    m_calculator(max_age, filter_min_samples, filter_time_constant)
{
  if (not log_directory.empty()) {
    auto filename = std::to_string(id) + ".csv";
    m_logger = std::make_unique<LogFiles>();
    
    m_logger->m_rx_log.open(log_directory / ("rx_" + filename));
    m_calculator.set_rx_logger(&m_logger->m_rx_log);

    m_logger->m_tx_log.open(log_directory / ("tx_" + filename));
    m_calculator.set_tx_logger(&m_logger->m_tx_log);
  }
}

void ClockSync::on_message_rx(const ClockSyncMessage& msg, ClockOffsetCalculator::Timepoint rx_timestamp) {
  std::lock_guard lk(m_mtx);

  CLSYN_LOG("Received message " << static_cast<int>(msg.message_id()) << " from peer " << static_cast<int>(msg.player_id()) << " at time " << rx_timestamp.time_since_epoch().count());

  if (msg.player_id() == m_player_id) {
    CLSYN_LOG("Discard message as player id equals ours");
    return;
  }

  auto peer_it = m_peers.lower_bound(msg.player_id());
  if (peer_it == m_peers.end() || peer_it->first != msg.player_id()) {
    CLSYN_LOG("New peer with ID " << static_cast<int>(msg.player_id()));
    // New peer, we can't possibly have information about its previous message
    peer_it = m_peers.emplace_hint(peer_it,
      std::piecewise_construct,
      std::forward_as_tuple(msg.player_id()),
      std::forward_as_tuple(msg.player_id(), msg.message_id(), rx_timestamp, m_calculator_max_age, m_calculator_min_samples, m_calculator_time_constant, m_log_directory)
    );
  } else {
    // Use the information about the previous message
    if (msg.message_id() == peer_it->second.m_prev_rx_msg_id + 1) {
      // Find the information about us the peer has
      auto peer_part_end = msg.peers().cend();
      auto peer_part_it = std::find_if(msg.peers().cbegin(), peer_part_end, [this](const PeerMessagePart& part) {
        return part.id() == m_player_id;
      });

      // If the peer has any information about us
      if (peer_part_it != peer_part_end) {
        // Add the information it has to our window

        if (msg.prev_tx_stamp().has_value()) {
          auto val = *(msg.prev_tx_stamp());
          peer_it->second.m_calculator.on_our_rx_delay(
            peer_it->second.m_prev_rx_timestamp,
            val
          );

          CLSYN_LOG("Peer prev_tx_stamp is " << val.time_since_epoch().count());
        } else {
          CLSYN_LOG("Peer has no prev_tx_stamp");
        }

        if (peer_part_it->min_rx_delay().has_value()) {
          auto val = *(peer_part_it->min_rx_delay());
          peer_it->second.m_calculator.on_their_rx_delay(rx_timestamp, val);
          CLSYN_LOG("Peer min_rx_delay is " << val.count());
        } else {
          CLSYN_LOG("Peer has no min_rx_delay");
        }
      } else {
        CLSYN_LOG("Peer has no information about us");
      }
    } else {
      CLSYN_LOG("Message ID jump, not using message");
    }
    
    peer_it->second.m_prev_rx_msg_id = msg.message_id();
    peer_it->second.m_prev_rx_timestamp = rx_timestamp;
  }
}

ClockSyncMessage ClockSync::on_message_tx(std::chrono::system_clock::time_point now) {
  std::lock_guard lk(m_mtx);

  

  ClockSyncMessage msg(m_player_id, m_message_id, m_prev_tx_stamp);
  for (const auto& peer : m_peers) {
    msg.peers().push_back(PeerMessagePart(peer.first, peer.second.m_calculator.get_rx_delay(now)));
  }
  CLSYN_LOG("Sending message " << static_cast<int>(m_message_id));
  ++m_message_id;

  return msg;
}

void ClockSync::store_tx_timestamp(ClockOffsetCalculator::Timepoint tx_timestamp) {
  m_prev_tx_stamp = tx_timestamp;
  CLSYN_LOG("Storing tx timestamp " << tx_timestamp.time_since_epoch().count());
}

std::optional<ClockOffsetCalculator::Duration> ClockSync::get_offset(unsigned char other_player_id, std::chrono::system_clock::time_point now) {
  std::lock_guard lk(m_mtx);
  
  auto pos = m_peers.find(other_player_id);
  if (pos == m_peers.end()) {
    CLSYN_LOG("Offset queried for unknown peer " << static_cast<int>(other_player_id));
    return std::nullopt;
  }

  auto ans = pos->second.m_calculator.clock_offset(now);
  if (ans) {
    CLSYN_LOG("Offset query answered with " << ans->count());
  } else {
    CLSYN_LOG("Offset queried, but it's unknown");
  }
  
  return ans;
}

ClockSync::ClockSync(unsigned char player_id, ClockOffsetCalculator::Duration calculator_max_age, size_t calculator_min_samples, std::chrono::duration<double> calculator_time_constant, std::ostream* log, std::filesystem::path log_directory)
  : m_message_id(0),
    m_player_id(player_id),
    m_log(log),
    m_log_directory(log_directory),
    m_calculator_max_age(calculator_max_age),
    m_calculator_min_samples(calculator_min_samples),
    m_calculator_time_constant(calculator_time_constant)
{
  if (not m_log_directory.empty()) {
    std::filesystem::create_directory(m_log_directory);
    m_file_log.emplace(m_log_directory / "log.txt");
  }
}

} // namespace clock_sync