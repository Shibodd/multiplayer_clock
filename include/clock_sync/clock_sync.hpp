#ifndef CLOCKSYNC_CLOCKSYNC_HPP
#define CLOCKSYNC_CLOCKSYNC_HPP

#include <climits>
#include <mutex>
#include <clock_sync/messages.hpp>
#include <boost/container/flat_map.hpp>

#define CLSYN_LOG(os_ptr, x) \
  do { \
    if (os_ptr != nullptr) { \
      (*os_ptr) << "ClockSync: " << x << '\n'; \
    } \
  } while(false)

struct ClockSync {
  struct Peer {
    unsigned char prev_rx_message_id;
    ClockOffsetCalculator::Timepoint prev_rx_timestamp;
    ClockOffsetCalculator rx_diff_window; 

    Peer(unsigned char rx_message_id,
         ClockOffsetCalculator::Duration window_duration,
         size_t window_capacity,
         ClockOffsetCalculator::Timepoint prev_rx_ts)
      : prev_rx_message_id(rx_message_id),
        prev_rx_timestamp(prev_rx_ts),
        rx_diff_window(window_duration, window_capacity)
    {}
  };

  void on_message_rx(const ClockSyncMessage& msg, ClockOffsetCalculator::Timepoint rx_timestamp) {
    std::lock_guard lk(m_mtx);

    CLSYN_LOG(m_log, "Received message " << static_cast<int>(msg.message_id()) << " from peer " << static_cast<int>(msg.player_id()) << " at time " << rx_timestamp.time_since_epoch().count());

    if (msg.player_id() == m_player_id) {
      CLSYN_LOG(m_log, "Discard message as player id equals ours");
      return;
    }

    auto peer_it = m_peers.lower_bound(msg.player_id());
    if (peer_it == m_peers.end() || peer_it->first != msg.player_id()) {
      CLSYN_LOG(m_log, "New peer with ID " << static_cast<int>(msg.player_id()));
      // New peer, we can't possibly have information about its previous message
      peer_it = m_peers.emplace_hint(peer_it,
        std::piecewise_construct,
        std::forward_as_tuple(msg.player_id()),
        std::forward_as_tuple(msg.message_id(), m_window_duration, m_window_capacity, rx_timestamp)
      );
    } else {
      // Use the information about the previous message
      if (msg.message_id() == peer_it->second.prev_rx_message_id + 1) {
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
            peer_it->second.rx_diff_window.on_message_rx_delay(
              peer_it->second.prev_rx_timestamp,
              val
            );

            CLSYN_LOG(m_log, "Peer prev_tx_stamp is " << val.time_since_epoch().count());
            CLSYN_LOG(m_log, "New window size " << peer_it->second.rx_diff_window.window_size());
          } else {
            CLSYN_LOG(m_log, "Peer has no prev_tx_stamp");
          }

          if (peer_part_it->min_rx_delay().has_value()) {
            auto val = *(peer_part_it->min_rx_delay());
            peer_it->second.rx_diff_window.on_their_min_rx_delay(rx_timestamp, val);
            CLSYN_LOG(m_log, "Peer min_rx_delay is " << val.count());
          } else {
            CLSYN_LOG(m_log, "Peer has no min_rx_delay");
          }
        } else {
          CLSYN_LOG(m_log, "Peer has no information about us");
        }
      } else {
        CLSYN_LOG(m_log, "Message ID jump, not using message" << msg.player_id());
      }
      
      peer_it->second.prev_rx_message_id = msg.message_id();
      peer_it->second.prev_rx_timestamp = rx_timestamp;
    }
  }

  ClockSyncMessage on_message_tx() {
    std::lock_guard lk(m_mtx);

    ClockSyncMessage msg(m_player_id, m_message_id, m_prev_tx_stamp);
    for (const auto& peer : m_peers) {
      msg.peers().push_back(PeerMessagePart(peer.first, peer.second.rx_diff_window.get_min_rx_delay()));
    }
    CLSYN_LOG(m_log, "Sending message " << m_message_id);
    ++m_message_id;

    return msg;
  }

  void store_tx_timestamp(ClockOffsetCalculator::Timepoint tx_timestamp) {
    m_prev_tx_stamp = tx_timestamp;
    CLSYN_LOG(m_log, "Storing tx timestamp " << tx_timestamp.time_since_epoch().count());
  }

  std::optional<ClockOffsetCalculator::Duration> get_offset(unsigned char other_player_id, std::chrono::system_clock::time_point now = std::chrono::system_clock::now()) {
    std::lock_guard lk(m_mtx);
    
    auto pos = m_peers.find(other_player_id);
    if (pos == m_peers.end()) {
      CLSYN_LOG(m_log, "Offset queried for unknown peer " << other_player_id);
      return std::nullopt;
    }

    auto ans = pos->second.rx_diff_window.clock_offset(now);
    if (ans) {
      CLSYN_LOG(m_log, "Offset query answered with " << ans->count());
    } else {
      CLSYN_LOG(m_log, "Offset queried, but it's unknown");
    }
    
    return ans;
  }

  ClockSync(unsigned char player_id, ClockOffsetCalculator::Duration window_duration, size_t window_capacity, std::ostream* log = nullptr) 
    : m_message_id(0),
      m_player_id(player_id),
      m_log(log),
      m_window_duration(window_duration),
      m_window_capacity(window_capacity)
  {}

private:
  std::mutex m_mtx;
  std::vector<PeerMessagePart> m_peers_buffer;
  boost::container::flat_map<unsigned char, Peer> m_peers;
  
  std::optional<ClockOffsetCalculator::Timepoint> m_prev_tx_stamp;
  unsigned short m_message_id;
  unsigned char m_player_id;

  std::ostream* m_log;

  ClockOffsetCalculator::Duration m_window_duration;
  size_t m_window_capacity;
};

#endif // !CLOCKSYNC_CLOCKSYNC_HPP