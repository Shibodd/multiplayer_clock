#include <csignal>
#include <chrono>
#include <array>
#include <cstring>
#include <iostream>
#include <sstream>
#include <thread>

#include <unistd.h>
#include <netinet/udp.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <linux/sockios.h>
#include <linux/net_tstamp.h>

#include <boost/iostreams/stream.hpp>
#include <boost/iostreams/device/array.hpp>

#include <cereal/archives/binary.hpp>

#include <clock_sync/clock_sync.hpp>

inline std::ostream& print_error(std::ostream& os, const char* msg) {
  return os << msg << " failed: " << strerrorname_np(errno) << " (" << strerror(errno) << ')';
} 

inline void assert_ok(int rc, const char* msg) {
  if (rc != 0) {
    std::ostringstream os;
    print_error(os, msg);
    throw std::runtime_error(os.str());
  }
}

enum class TimestampSource {
  None = 0,
  Wall,
  Software
};

struct RxMessage {
  std::string_view data {};
  std::chrono::system_clock::time_point timestamp { decltype(timestamp)::duration(0) };
  TimestampSource timestamp_source { TimestampSource::None };
  bool truncated = false;
};

struct MulticastSocket {
  MulticastSocket(const char* group, unsigned short port) {
    m_sock = socket(AF_INET, SOCK_DGRAM, 0);

    memset(&m_group, 0, sizeof(m_group));
    m_group.sin_family = AF_INET;
    m_group.sin_addr.s_addr = inet_addr(group);
    m_group.sin_port = htons(port);

    {
      // Set minimum buffering possible
      int value = 0;
      assert_ok(setsockopt(m_sock, SOL_SOCKET, SO_SNDBUF, &value, sizeof(value)), "set sndbuf");
      assert_ok(setsockopt(m_sock, SOL_SOCKET, SO_RCVBUF, &value, sizeof(value)), "set rcvbuf");

      value = 1;
      assert_ok(setsockopt(m_sock, IPPROTO_IP, IP_MULTICAST_LOOP, &value, sizeof(value)), "disable loopback");
      assert_ok(setsockopt(m_sock, SOL_SOCKET, SO_REUSEADDR, &value, sizeof(value)), "enable reuseaddr");
      assert_ok(setsockopt(m_sock, SOL_SOCKET, SO_TIMESTAMPNS, &value, sizeof(value)), "enable rx timestamping");

      value = SOF_TIMESTAMPING_SOFTWARE | SOF_TIMESTAMPING_TX_SCHED;
      assert_ok(setsockopt(m_sock, SOL_SOCKET, SO_TIMESTAMPING, &value, sizeof(value)), "enable timestamping");
      
      // value = 200;
      // assert_ok(setsockopt(m_sock, SOL_SOCKET, SO_BUSY_POLL, &value, sizeof(value)), "enable busy polling");
    }
    {
      timeval value;
      memset(&value, 0, sizeof(value));
      value.tv_sec = 1;
      assert_ok(setsockopt(m_sock, SOL_SOCKET, SO_RCVTIMEO, &value, sizeof(value)), "set rcvtimeo");
    }
    
    sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = m_group.sin_port;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    assert_ok(bind(m_sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)), "bind");
    
    
    // TODO: real time scheduling

    ip_mreq mreq;
    memset(&mreq, 0, sizeof(mreq));
    mreq.imr_interface.s_addr = htonl(INADDR_ANY);
    mreq.imr_multiaddr.s_addr = m_group.sin_addr.s_addr;
    assert_ok(setsockopt(m_sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char*) &mreq, sizeof(mreq)), "join multicast group");
  }

  ssize_t send(std::string_view msg, std::chrono::system_clock::time_point& time) {
    iovec iov {
      .iov_base = const_cast<decltype(msg)::value_type*>(msg.data()),
      .iov_len = msg.size()
    };

    msghdr msgh;
    memset(&msgh, 0, sizeof(msgh));
    msgh.msg_iov = &iov;
    msgh.msg_iovlen = 1;
    msgh.msg_name = &m_group;
    msgh.msg_namelen = sizeof(m_group);

    // Loots of drivers out there that do not support TX SW timestamping.
    // Here we approximate it as the midpoint of sendmsg.
    auto before = std::chrono::system_clock::now();
    ssize_t tx_ans = sendmsg(m_sock, &msgh, 0);
    auto after = std::chrono::system_clock::now();
    time = before + (after - before) / 2;

    return tx_ans;
  }

  std::optional<RxMessage> receive(char* buffer, size_t buf_size) {
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
        print_error(std::cerr, "msg with truncated control!!");
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
          std::chrono::duration_cast<std::chrono::system_clock::duration>(std::chrono::seconds(rx_time->tv_sec) + std::chrono::nanoseconds(rx_time->tv_nsec))
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

  ~MulticastSocket() {
    close(m_sock);
  }
private:
  struct sockaddr_in m_group;
  int m_sock;
};

static bool run = true;

using namespace clock_sync;

int main(int argc, char* argv[]) {
  unsigned char OUR_ID = std::stoi(argv[1]);
  unsigned char THEIR_ID = std::stoi(argv[2]);
  const char* LOG_DIR = argv[3];

  MulticastSocket sock("239.1.2.3", 7423);
  ClockSync clock_sync(
    OUR_ID,
    std::chrono::seconds(30),
    50,
    std::chrono::minutes(1),
    nullptr,
    LOG_DIR
  );

  auto tx = std::thread([&sock, &clock_sync]() {
    std::array<char, 512> buffer;
    boost::iostreams::array_sink sink(buffer.data(), buffer.size());
    boost::iostreams::stream os(sink);
    cereal::BinaryOutputArchive ar(os);

    while (run) {
      std::chrono::system_clock::time_point tx_timestamp;
      os.clear();
      os.seekp(0, std::ios::beg);

      ClockSyncMessage msg = clock_sync.on_message_tx(std::chrono::system_clock::now());
      ar(msg);

      auto pos = os.tellp();
      sock.send(std::string_view(buffer.data(), pos), tx_timestamp);
      clock_sync.store_tx_timestamp(tx_timestamp);
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
  });
  auto rx = std::thread([&sock, &clock_sync, &THEIR_ID]() {
    std::array<char, 512> buffer;

    while (run) {
      ClockOffsetCalculator::Timepoint rx_timestamp;

      std::optional<RxMessage> msg = sock.receive(buffer.data(), buffer.size());
      
      if (msg.has_value()) {
        if (msg->timestamp_source != TimestampSource::Software) {
          std::cerr << "bad stamp" << std::endl;
          exit(1);
        }

        boost::iostreams::array_source source(msg->data.data(), msg->data.size());
        boost::iostreams::stream is(source);
        cereal::BinaryInputArchive archive(is);
        ClockSyncMessage csyn_msg;

        try {
          archive(csyn_msg);
        } catch (const cereal::Exception& ex) {
          std::cerr << "Failed to parse incoming message: " << ex.what() << ". Discarding it!";
          continue;
        }

        clock_sync.on_message_rx(csyn_msg, msg->timestamp);
      } else {
        std::cerr << "rcvtimeo" << std::endl;
      }

      if (auto off = clock_sync.get_offset(THEIR_ID, std::chrono::system_clock::now())) {
        std::cout << "Offset (us): " << std::chrono::duration_cast<std::chrono::microseconds>(*off).count() << std::endl;
      }
    }
  });

  signal(SIGINT, [](int) {
    run = false;
    std::cerr << "SIGINT received - stopping" << std::endl;
  });

  rx.join();
  tx.join();
}