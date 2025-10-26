#include <algorithm>
#include <chrono>
#include <iostream>
#include <vector>
#include <optional>
#include <cstring>
#include <sstream>

#include <unistd.h>
#include <netinet/udp.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <linux/sockios.h>
#include <linux/net_tstamp.h>

#include <clock_sync/the_socket.hpp>

#define PERROR(os, x) (os << x << " failed: " << strerrorname_np(errno) << " (" << strerror(errno) << ')')
#define PERROR_SPRINT(x) PERROR(std::ostringstream(), x).str()

#define SPRINT(x) [&]() { \
  return (std::ostringstream() << x).str(); \
}()
#define ASSERT_OK_PERROR(rc, x) do { if (rc != 0) throw std::runtime_error(PERROR_SPRINT(x)); } while(false)
#define ASSERT(cond, exception, x) do { if (not (cond)) throw exception(SPRINT(x)); } while(false)

TheSocket::~TheSocket() {
  close(m_sock);
}

TheSocket::TheSocket(const char* multicast_iface_address, const char* const * addresses, size_t num_addresses, unsigned short port, const Options& options) {
  m_sock = socket(AF_INET, SOCK_DGRAM, 0);

  m_groups.reserve(num_addresses);
  for (size_t i = 0; i < num_addresses; ++i) {
    const char* arg_addr = addresses[i];

    sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    int ret = inet_pton(AF_INET, arg_addr, &addr.sin_addr);
    ASSERT(ret > 0, std::invalid_argument, "Parsing address " << arg_addr << " failed.");

    // Don't push duplicate addresses
    auto it = std::find_if(m_groups.begin(), m_groups.end(), [&addr](const Group& grp) {
      return grp.address.sin_addr.s_addr == addr.sin_addr.s_addr;
    });
    if (it == m_groups.end()) {
      m_groups.emplace_back(addr);
    }
  }

  {
    // Set minimum buffering possible on sending
    // int value = 0;
    // assert_ok(setsockopt(m_sock, SOL_SOCKET, SO_SNDBUF, &value, sizeof(value)), "set sndbuf");
    // assert_ok(setsockopt(m_sock, SOL_SOCKET, SO_RCVBUF, &value, sizeof(value)), "set rcvbuf");

    int value = 1;
    ASSERT_OK_PERROR(setsockopt(m_sock, SOL_SOCKET, SO_REUSEADDR, &value, sizeof(value)), "enable reuseaddr");
    ASSERT_OK_PERROR(setsockopt(m_sock, SOL_SOCKET, SO_TIMESTAMPNS, &value, sizeof(value)), "enable rx timestamping");
    if (options.enable_broadcast) {
      ASSERT_OK_PERROR(setsockopt(m_sock, SOL_SOCKET, SO_BROADCAST, &value, sizeof(value)), "enable broadcast");
    }
    if (options.disable_loopback) {
      value = 0;
      ASSERT_OK_PERROR(setsockopt(m_sock, IPPROTO_IP, IP_MULTICAST_LOOP, &value, sizeof(value)), "disable loopback");
    }

    value = SOF_TIMESTAMPING_SOFTWARE | SOF_TIMESTAMPING_TX_SCHED;
    ASSERT_OK_PERROR(setsockopt(m_sock, SOL_SOCKET, SO_TIMESTAMPING, &value, sizeof(value)), "enable timestamping");
    
    // value = 200;
    // assert_ok(setsockopt(m_sock, SOL_SOCKET, SO_BUSY_POLL, &value, sizeof(value)), "enable busy polling");
  }
  {
    timeval value;
    memset(&value, 0, sizeof(value));
    value.tv_sec = 1;
    ASSERT_OK_PERROR(setsockopt(m_sock, SOL_SOCKET, SO_RCVTIMEO, &value, sizeof(value)), "set rcvtimeo");
  }

  // Bind to filter on specified port coming from any address
  {
    sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    ASSERT_OK_PERROR(bind(m_sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)), "bind");
  }
  
  // Join multicast groups
  {
    in_addr iface_addr;
    memset(&iface_addr, 0, sizeof(iface_addr));
    int ret = inet_pton(AF_INET, multicast_iface_address, &iface_addr);
    ASSERT(ret > 0, std::invalid_argument, "Parsing interface address " << multicast_iface_address << " failed");

    for (const auto& addr : m_groups) {
      if (IN_MULTICAST(ntohl(addr.address.sin_addr.s_addr))) {
        ip_mreq mreq;
        memset(&mreq, 0, sizeof(mreq));
        mreq.imr_interface = iface_addr;
        mreq.imr_multiaddr = addr.address.sin_addr;
        ASSERT_OK_PERROR(setsockopt(m_sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char*) &mreq, sizeof(mreq)), "join multicast group");
      }
    }
  }
}

void TheSocket::send(size_t group, iovec part) {
  return send(group, &part, 1);
}

void TheSocket::send(size_t group, iovec* parts, size_t num_parts) {
  msghdr msgh;
  memset(&msgh, 0, sizeof(msgh));
  msgh.msg_iov = parts;
  msgh.msg_iovlen = num_parts;

  auto& grp = m_groups[group];

  msgh.msg_namelen = sizeof(grp.address);
  msgh.msg_name = &grp.address;

  // Loots of drivers out there that do not support TX SW timestamping.
  // Here we approximate it as the midpoint of sendmsg.
  auto before = std::chrono::system_clock::now();
  ssize_t sent = sendmsg(m_sock, &msgh, 0);
  int ec = errno;
  auto after = std::chrono::system_clock::now();

  grp.last_tx.sent = sent;
  grp.last_tx.ec = std::error_code(ec, std::generic_category());
  grp.last_tx.time = before + (after - before) / 2;
}

/**
  @brief Receives a single message (from one group)
*/
std::optional<RxMessage> TheSocket::receive(char* buffer, size_t buf_size) {
  iovec iov {
    .iov_base = buffer,
    .iov_len = buf_size
  };

  char ctrlbuf[1024];

  msghdr msgh;
  memset(&msgh, 0, sizeof(msgh));
  msgh.msg_iov = &iov;
  msgh.msg_iovlen = 1;
  msgh.msg_control = ctrlbuf;
  msgh.msg_controllen = sizeof(ctrlbuf);

  ssize_t sz = recvmsg(m_sock, &msgh, 0);

  if (sz >= 0) {
    RxMessage ans;
    ans.data = std::string_view(buffer, sz);

    ans.truncated = msgh.msg_flags;
    if (msgh.msg_flags & MSG_CTRUNC) {
      std::cerr << "msg with truncated control!!\n";
    } 

    timespec* rx_time = nullptr;

    for (cmsghdr* cmsg = CMSG_FIRSTHDR(&msgh); cmsg != NULL; cmsg = CMSG_NXTHDR(&msgh,cmsg)) {
      // std::cout << cmsg->cmsg_level << ", " << cmsg->cmsg_type << std::endl;
      if (cmsg->cmsg_level == SOL_SOCKET) {
        if (cmsg->cmsg_type == SCM_TIMESTAMPNS) {
          rx_time = reinterpret_cast<timespec*>(CMSG_DATA(cmsg));
        }
      }
    }

    if (rx_time != nullptr) {
      ans.timestamp = std::chrono::system_clock::time_point(
        std::chrono::duration_cast<std::chrono::system_clock::duration>(
          std::chrono::seconds(rx_time->tv_sec) + std::chrono::nanoseconds(rx_time->tv_nsec)
        )
      );
      ans.timestamp_source = TimestampSource::Software;
    } else {
      ans.timestamp = std::chrono::system_clock::now();
      ans.timestamp_source = TimestampSource::Wall;
    }

    return ans;
  }

  return std::nullopt;
}