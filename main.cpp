#include <csignal>
#include <chrono>
#include <array>
#include <cstring>
#include <iostream>
#include <sstream>
#include <thread>


#include <clock_sync/the_socket.hpp>

#include <boost/iostreams/stream.hpp>
#include <boost/iostreams/device/array.hpp>

#include <cereal/archives/binary.hpp>

#include <clock_sync/clock_sync.hpp>



static bool run = true;

using namespace clock_sync;

int main(int argc, char* argv[]) {
  unsigned char OUR_ID = std::stoi(argv[1]);
  unsigned char THEIR_ID = std::stoi(argv[2]);
  const char* LOG_DIR = argv[3];

  const char* const* ADDRESSES = argv + 4;
  const size_t NUM_ADDRESSES = argc - 4;

  TheSocket sock("0.0.0.0", ADDRESSES, NUM_ADDRESSES, 7423);

  ClockOffsetCalculator::Filter::Params params;
  params.window_size = 110;
  params.chunk_size = 5;
  params.duration = std::chrono::seconds(10);

  ClockSync clock_sync(
    OUR_ID,
    params,
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

      ClockSyncMessage msg = clock_sync.on_message_tx(std::chrono::system_clock::now());

      for (size_t i = 0; i < sock.num_groups(); ++i) {
        msg.set_prev_tx_stamp(sock.get_last_tx(i).time);

        // TODO: don't waste time reserializing the whole thing (i.e. drop cereal :) )
        os.clear();
        os.seekp(0, std::ios::beg);
        ar(msg);

        sock.send(i, iovec {
          .iov_base = buffer.data(),
          .iov_len = static_cast<size_t>(os.tellp())
        });

        if (sock.get_last_tx(i).sent != os.tellp()) {
          std::cerr << "not sent full message" << std::endl;
        }
        if (sock.get_last_tx(i).ec.value() != 0) {
          std::cerr << sock.get_last_tx(i).ec.message() << std::endl;
        }
      }

      std::this_thread::sleep_for(std::chrono::milliseconds(20));
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

      auto now = std::chrono::system_clock::now();
      if (auto off = clock_sync.get_offset(THEIR_ID, now)) {
        std::cout << now.time_since_epoch().count() << ',' << std::chrono::duration_cast<std::chrono::microseconds>(*off).count() << '\n';
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