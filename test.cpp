#include <unistd.h>
#include <netinet/udp.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <linux/sockios.h>
#include <linux/net_tstamp.h>

#include <vector>
#include <algorithm>

struct CompareSockAddrIn  {
  bool operator()(const sockaddr_in& a, const sockaddr_in& b) const {
    return true;
  }
};

int main() {
  std::vector<sockaddr_in> v;
  

  sockaddr_in test;

  std::find_if(v.begin(), v.end(), [test](const sockaddr_in& a) {
    return a.sin_addr.s_addr == test.sin_addr.s_addr;
  });
}