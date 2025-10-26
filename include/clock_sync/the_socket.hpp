#include <system_error>
#include <optional>
#include <cstring>
#include <chrono>
#include <vector>

#include <netinet/in.h>

enum class TimestampSource {
  None = 0,
  Wall,
  Software
};

struct RxMessage {
  long group = -1;
  std::string_view data {};
  std::chrono::system_clock::time_point timestamp { decltype(timestamp)::duration(0) };
  TimestampSource timestamp_source { TimestampSource::None };
  bool truncated = false;
};

struct Options {
  bool enable_broadcast;
  bool disable_loopback;
};

struct TheSocket {
  TheSocket(const char* multicast_iface_address, const char* const * addresses, size_t num_addresses, unsigned short port, const Options& options);

  void send(size_t group, iovec part);
  void send(size_t group, iovec* parts, size_t num_parts);

  /**
    @brief Receives a single message (from one group)
  */
  std::optional<RxMessage> receive(char* buffer, size_t buf_size);

  size_t num_groups() const { return m_groups.size(); }

  const auto& get_last_tx(size_t group) const {
    return m_groups.at(group).last_tx;
  }

  ~TheSocket();
private:
  struct Group {
    sockaddr_in address;
    struct LastTx {
      ssize_t sent = 0;
      std::error_code ec = {};
      std::chrono::system_clock::time_point time = {};
    } last_tx;

    Group(sockaddr_in addr) : address(addr) {}
  };

  std::vector<Group> m_groups;
  int m_sock;
};