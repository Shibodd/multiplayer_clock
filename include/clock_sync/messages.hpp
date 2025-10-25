#ifndef CLOCKSYNC_MESSAGES_HPP
#define CLOCKSYNC_MESSAGES_HPP

#include <clock_sync/clock_offset_calculator.hpp>
#include <clock_sync/types.hpp>

#include <cereal/cereal.hpp>

namespace clock_sync {

struct PeerMessagePart {
  PeerMessagePart() {}
  PeerMessagePart(player_id_t id, std::optional<ClockOffsetCalculator::Duration> m_min_rx_delay)
    : m_id(id),
      m_min_rx_delay(m_min_rx_delay.has_value()? m_min_rx_delay->count() : 0)
  {}

  template <class Archive>
  void serialize(Archive& ar) {
    ar(m_id);
    ar(m_min_rx_delay);
  }

  player_id_t id() const { return m_id; }
  std::optional<ClockOffsetCalculator::Duration> min_rx_delay() const {
    if (m_min_rx_delay != 0) {
      return ClockOffsetCalculator::Duration(m_min_rx_delay);
    } else {
      return std::nullopt;
    }
  }

private:
  player_id_t m_id;
  ClockOffsetCalculator::Timepoint::rep m_min_rx_delay;
};

struct ClockSyncMessage {
  ClockSyncMessage() {}
  ClockSyncMessage(
    player_id_t player_id,
    message_id_t message_id,
    std::optional<ClockOffsetCalculator::Timepoint> prev_tx_stamp
  )
    : m_player_id(player_id),
      m_message_id(message_id),
      m_prev_tx_stamp(prev_tx_stamp.has_value()? prev_tx_stamp->time_since_epoch().count() : 0)
  {}

  template <class Archive>
  void serialize(Archive& ar) {
    ar(m_player_id);
    ar(m_message_id);
    ar(m_prev_tx_stamp);

    if (Archive::is_loading::value) {
      peer_len_t num_peers;
      ar(num_peers);
      m_peers.resize(num_peers);
    } else {
      ar(static_cast<peer_len_t>(m_peers.size()));
    }
    for (auto& peer : m_peers) {
      ar(peer);
    }
  }

  player_id_t player_id() const { return m_player_id; }
  message_id_t message_id() const { return m_message_id; }
  std::optional<ClockOffsetCalculator::Timepoint> prev_tx_stamp() const {
    if (m_prev_tx_stamp > 0) {
      return ClockOffsetCalculator::Timepoint(
        ClockOffsetCalculator::Duration(m_prev_tx_stamp)
      );
    } else {
      return std::nullopt;
    }
  }
  std::vector<PeerMessagePart>& peers() { return m_peers; }
  const std::vector<PeerMessagePart>& peers() const { return m_peers; }

private:
  player_id_t m_player_id;
  message_id_t m_message_id;
  ClockOffsetCalculator::Timepoint::rep m_prev_tx_stamp;
  std::vector<PeerMessagePart> m_peers;
};

} // namespace clock_sync

#endif // !CLOCKSYNC_MESSAGES_HPP