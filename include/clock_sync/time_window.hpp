#ifndef CLOCKSYNC_TIMEWINDOW_HPP
#define CLOCKSYNC_TIMEWINDOW_HPP

#include <numeric>
#include <vector>
#include <optional>
#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <cassert>

template <typename Timepoint>
struct TimeRange {
  using Duration = typename Timepoint::duration;

  Timepoint start;
  Timepoint end;

  explicit TimeRange(Timepoint tp) : start(tp), end(tp) {}
  TimeRange(Timepoint begin, Timepoint end) : start(begin), end(end) {}

  bool contains(Timepoint query) const {
    return query >= start && query <= end;
  }
  auto head(Duration duration) const {
    TimeRange ans = *this;
    ans.end = std::min(ans.end, ans.start + duration);
    return ans;
  }
  auto tail(Duration duration) const {
    TimeRange ans = *this;
    ans.start = std::max(ans.start, ans.end - duration);
    return ans;
  }
  TimeRange operator+(const TimeRange& other) const {
    TimeRange ans = *this;
    ans.start = std::min(ans.start, other.start);
    ans.end = std::max(ans.end, other.end);
    return ans;
  }
  bool operator==(const TimeRange& other) const {
    return start == other.start && end == other.end;
  }
  bool operator!=(const TimeRange& other) const {
    return not (*this == other);
  }
  Duration duration() const { return end - start; }

  friend std::ostream& operator<<(std::ostream& os, const TimeRange& rng) {
    return os << rng.start.time_since_epoch().count() << "-" << rng.end.time_since_epoch().count();
  }
};

template <typename Timepoint, typename Value>
struct TimeWindow {
  using Duration = typename Timepoint::duration;

  struct Sample {
    Timepoint timestamp;
    Value value;

    Sample(Timepoint timestamp, Value value)
      : timestamp(timestamp), value(value)
    {}

    bool operator==(const Sample& other) const {
      return timestamp == other.timestamp and value == other.value;
    }

    friend std::ostream& operator<<(std::ostream& os, const Sample& s) {
      return os << s.value << "@" << s.timestamp.time_since_epoch().count();
    }
  };

  TimeWindow(Duration duration, size_t capacity)
    : m_duration(duration)
  {
    if (duration <= Duration(0)) {
      throw std::invalid_argument("TimeWindow duration must be > 0");
    }
    if (capacity == 0) {
      throw std::invalid_argument("TimeWindow capacity must be > 0");
    }
    m_samples.reserve(capacity);
  }

  Duration window_duration() const { return m_duration; }

  std::optional<TimeRange<Timepoint>> range() const {
    if (m_samples.size() <= 0) {
      return std::nullopt;
    }
    return TimeRange { m_samples.front().timestamp, m_samples.back().timestamp };
  }

  void discard_before(Timepoint timestamp) {
    auto pos = insertion_position(timestamp);
    m_samples.erase(m_samples.begin(), pos);
  }

  void push(Sample sample) {
    auto old_range_opt = range();

    // Empty, just push
    if (not old_range_opt.has_value()) {
      m_samples.push_back(sample);
      return;
    }

    auto old_begin = m_samples.begin();
    auto old_end = m_samples.end();

    auto old_range = *old_range_opt;
    auto new_range = old_range + TimeRange(sample.timestamp);

    // If the sample is outside the window range
    if (new_range != old_range) {
      // If timestamp is older than window range
      if (new_range.start == sample.timestamp) {
        // If we can fit it
        if (new_range.duration() <= m_duration and size() < capacity()) {
          m_samples.insert(old_begin, sample);
        }

        // just ignore it if it doesn't fit
      } else {
        // Timestamp is newer than window range
        assert(sample.timestamp == new_range.end);

        // Clip the new range to the last m_duration
        new_range = new_range.tail(m_duration);

        // Find how many elements we should delete from the oldest ones
        auto new_begin = old_begin;
        while (new_begin != old_end && not new_range.contains(new_begin->timestamp)) {
          ++new_begin;
        }
        if (new_begin == old_begin && size() >= capacity()) {
          ++new_begin;
        }

        m_samples.erase(old_begin, new_begin);
        m_samples.push_back(sample);
      }
    } else {
      if (size() == capacity()) {
        m_samples.erase(m_samples.begin());
      }

      m_samples.insert(insertion_position(sample.timestamp), sample);
    }
  }

  void push(Timepoint timestamp, Value value) { push(Sample { timestamp, value }); }

  bool test_invariants() const {
    bool ok = true;
    if (auto rng = range()) {
      ok &= rng->duration() <= m_duration;
    }
    ok &= std::is_sorted(m_samples.begin(), m_samples.end(), [](const Sample& a, const Sample& b) {
      return a.timestamp < b.timestamp;
    });
    return ok;
  }
  
  std::optional<Sample> min_valued() const {
    auto end = cend();
    auto it = std::min_element(cbegin(), end, [](const Sample& a, const Sample& b) {
      return a.value < b.value;
    });

    if (it == end) {
      return std::nullopt;
    } else {
      return *it;
    }
  }

  bool empty() const { return m_samples.empty(); }
  auto cbegin() const { return m_samples.cbegin(); }
  auto cend() const { return m_samples.cend(); }
  size_t size() const { return m_samples.size(); }
  size_t capacity() const { return m_samples.capacity(); }
  const Sample& at(size_t i) const { return m_samples.at(i); }
private:
  auto insertion_position(Timepoint timestamp) {
    return std::lower_bound(m_samples.begin(), m_samples.end(), timestamp, [](const Sample& stored_sample, decltype(Sample::timestamp) sample_timestamp) {
      return stored_sample.timestamp < sample_timestamp;
    });
  }

  Duration m_duration;
  std::vector<Sample> m_samples;
};

#endif // !CLOCKSYNC_TIMEWINDOW_HPP