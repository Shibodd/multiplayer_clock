#include <gtest/gtest.h>
#include <clock_sync/clock_sync.hpp>

using namespace std::chrono_literals;

using namespace clock_sync;

TEST(ClockSyncTest, BasicExchange) {
  constexpr unsigned char OUR_ID = 14;
  constexpr unsigned char THEIR_ID = 26;
  
  constexpr std::chrono::system_clock::duration CLOCK_OFFSET = 1234s; // TheirT = OurT + OFFSET
  constexpr std::chrono::system_clock::duration PERIOD = 5s; // send period
  constexpr std::chrono::system_clock::duration DELAY = 3s; // comm delay
  constexpr std::chrono::system_clock::duration OUR_PHASE = 1s; // Start sending at OUR_T0 + OUR_PHASE 
  
  // Clock time at the start
  constexpr std::chrono::system_clock::time_point OUR_T0 = std::chrono::system_clock::time_point(10s);
  constexpr std::chrono::system_clock::time_point THEIR_T0 = OUR_T0 + CLOCK_OFFSET;

  // Start message ID
  constexpr unsigned char OUR_MID0 = 0;
  constexpr unsigned char THEIR_MID0 = 0;
  ClockOffsetCalculator::Filter::Params params;
  params.window_size = 1;
  params.chunk_size = 1;
  params.duration = 2 * PERIOD;
  ClockSync clk(OUR_ID, params, &std::cerr);

  /* PERIOD 0
  No information known by either peer
  */

  // We send at T0 + OUR_PHASE
  std::cout << "tx0" << std::endl;
  {
    auto now = OUR_T0 + OUR_PHASE;
    auto tx_msg = clk.on_message_tx(now);
    EXPECT_FALSE(tx_msg.prev_tx_stamp().has_value());
    EXPECT_EQ(tx_msg.message_id(), OUR_MID0);
    EXPECT_TRUE(tx_msg.peers().empty());
    // clk.store_tx_timestamp(now);
  }

  // They send at T0
  std::cout << "rx0" << std::endl;
  clk.on_message_rx({ THEIR_ID, THEIR_MID0 }, OUR_T0 + DELAY);
  EXPECT_FALSE(clk.get_offset(THEIR_ID, OUR_T0 + DELAY).has_value());

  /* PERIOD 1
  Peers have information about previous messages,
  and can compute rx delay upon reception;
  but they'll only transmit this information on the next period.
  */

  // We send at T0 + OUR_PHASE + PERIOD
  std::cout << "tx1" << std::endl;
  {
    auto now = OUR_T0 + OUR_PHASE + PERIOD;
    auto tx_msg = clk.on_message_tx(now);
    // Has to be inserted outside now. ASSERT_TRUE(tx_msg.prev_tx_stamp().has_value());
    // Has to be inserted outside now. EXPECT_EQ(*tx_msg.prev_tx_stamp(), OUR_T0 + OUR_PHASE);
    EXPECT_FALSE(tx_msg.prev_tx_stamp().has_value());
    EXPECT_EQ(tx_msg.message_id(), OUR_MID0 + 1);
    // We now know about our peer, although we have no estimate for the rx delay.
    ASSERT_FALSE(tx_msg.peers().empty());
    EXPECT_EQ(tx_msg.peers().front().id(), THEIR_ID);
    EXPECT_FALSE(tx_msg.peers().front().min_rx_delay().has_value());
    // clk.store_tx_timestamp(now);
  }
  
  // They send at T0 + PERIOD
  std::cout << "rx1" << std::endl;
  {
    ClockSyncMessage rx_msg(THEIR_ID, THEIR_MID0 + 1, THEIR_T0);
    // Our peer has no idea about the rx delay either
    rx_msg.peers().emplace_back(OUR_ID, std::nullopt);
    clk.on_message_rx(rx_msg, OUR_T0 + PERIOD + DELAY);
  }
  EXPECT_FALSE(clk.get_offset(THEIR_ID, OUR_T0 + PERIOD + DELAY).has_value());

  /* PERIOD 2
  Peers get to know each other's rx delay and the offset becomes available.
  */
  // We send at T0 + OUR_PHASE + 2*PERIOD

  std::cout << "tx2" << std::endl;
  {
    auto now = OUR_T0 + OUR_PHASE + 2 * PERIOD;
    auto tx_msg = clk.on_message_tx(now);
    // Has to be inserted outside now. ASSERT_TRUE(tx_msg.prev_tx_stamp().has_value());
    // Has to be inserted outside now. EXPECT_EQ(*tx_msg.prev_tx_stamp(), OUR_T0 + OUR_PHASE + PERIOD);
    EXPECT_FALSE(tx_msg.prev_tx_stamp().has_value());
    EXPECT_EQ(tx_msg.message_id(), OUR_MID0 + 2);
    ASSERT_FALSE(tx_msg.peers().empty());
    EXPECT_EQ(tx_msg.peers().front().id(), THEIR_ID);
    ASSERT_TRUE(tx_msg.peers().front().min_rx_delay().has_value());
    EXPECT_EQ(tx_msg.peers().front().min_rx_delay()->count(), (DELAY-CLOCK_OFFSET).count());
    // clk.store_tx_timestamp(now);
  }

  std::cout << "rx2" << std::endl;
  {
    ClockSyncMessage rx_msg(THEIR_ID, THEIR_MID0 + 2, THEIR_T0 + PERIOD);
    rx_msg.peers().emplace_back(OUR_ID, DELAY + CLOCK_OFFSET);
    clk.on_message_rx(rx_msg, OUR_T0 + 2 * PERIOD + DELAY);
  }

  auto offset = clk.get_offset(THEIR_ID, OUR_T0 + 2 * PERIOD + DELAY);
  ASSERT_TRUE(offset.has_value());
  EXPECT_EQ(*offset, CLOCK_OFFSET);
}
