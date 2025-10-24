#include <vector>
#include <optional>
#include <functional>
#include <stdexcept>
#include <cassert>

template <typename Timestamp, typename Value>
struct Sample {
  Timestamp timestamp;
  Value value;

  Sample() {}
  Sample(Timestamp timestamp, Value value)
    : timestamp(timestamp),
      value(value)
  {}
};

template <typename Timestamp, typename MeasurementValue, typename OutputValue>
struct Filter {
  struct HistoryEntryValue {
    MeasurementValue measurement;
    std::optional<OutputValue> output;

    HistoryEntryValue() {}
    HistoryEntryValue(MeasurementValue measurement, std::optional<OutputValue> output)
      : measurement(measurement),
        output(output)
    {}
  };
  using HistoryEntry = Sample<Timestamp, HistoryEntryValue>;
  using History = std::vector<HistoryEntry>;

  struct Accessor {
    const HistoryEntry* at(long idx) const {
      if (idx < -1 || idx >= m_base_idx) {
        throw std::out_of_range("index out of range");
      }
      if (idx == -1) {
        if (m_filter.m_committed_state.has_value()) {
          return &*m_filter.m_committed_state;
        }
        return nullptr;
      } else {
        return &m_filter.m_history[idx];
      }
    }
    const HistoryEntry* rat(long ridx) const {
      long idx = m_base_idx - ridx;
      if (idx < -1) {
        return nullptr;
      }
      return at(idx);
    }
    size_t size() const {
      if (m_base_idx == 0 && not m_filter.m_committed_state.has_value()) {
        return 0;
      }
      return m_base_idx + 1;
    }
    size_t base_idx() const { return m_base_idx; }
    using FilterT = Filter<Timestamp, MeasurementValue, OutputValue>;
    Accessor(const FilterT& filter, long base_idx)
      :  m_base_idx(base_idx),
        m_filter(filter)
    {}
    Accessor(const FilterT& filter, typename FilterT::History::const_iterator base)
      : Accessor(filter, std::distance(filter.m_history.begin(), base))
    
    {}
  private:
    long m_base_idx;
    const FilterT& m_filter;
  };

  using FilterFunction = std::function<std::optional<OutputValue>(Accessor, Timestamp, MeasurementValue)>;

  Filter(std::size_t capacity, const FilterFunction& filter_fcn)
    :  m_filter_fcn(filter_fcn)
  {
    if (capacity <= 0) {
      throw std::invalid_argument("Filter capacity must be > 0");
    }
    m_history.reserve(capacity);
  }

  void push(Timestamp timestamp, MeasurementValue measurement) {
    assert(m_history.capacity() > 0);

    if (m_history.size() == m_history.capacity()) {
      m_committed_state = m_history.front();
      m_history.erase(m_history.begin());
    }

    auto entry = HistoryEntry(
      timestamp,
      HistoryEntryValue(
        measurement,
        m_filter_fcn(Accessor(*this, 0), timestamp, measurement)
      )
    );

    if (m_history.empty()) {
      m_committed_state = entry;
    } else {
      auto begin = m_history.begin();
      auto end = m_history.end();
      auto pos = std::lower_bound(begin, end, timestamp, [](const HistoryEntry& entry, const Timestamp& timestamp) {
        return entry.timestamp < timestamp;
      });
      pos = m_history.insert(pos, entry);
      recompute_output(pos + 1);
    }

    assert(std::is_sorted(m_history.begin(), m_history.end(), [](const HistoryEntry& a, const HistoryEntry& b) {
      return a.timestamp < b.timestamp;
    }));
    assert(!m_committed_state.has_value() || m_history.empty() || m_committed_state->timestamp < m_history.front().timestamp);
  }

  std::optional<Sample<Timestamp, OutputValue>> output() const {
    if (m_history.empty()) {
      if (m_committed_state.has_value() && m_committed_state->value.output.has_value()) {
        return Sample<Timestamp, OutputValue>{ m_committed_state->timestamp, *m_committed_state->value.output };
      }
    } else if (m_history.back().value.output.has_value()) {
      return Sample<Timestamp, OutputValue>{ m_history.back().timestamp, *m_history.back().value.output };
    }

    return std::nullopt;
  }

private:
  void recompute_output(typename History::iterator begin) {
    auto end = m_history.end();
    for (; begin != end; ++begin) {
      begin->value.output = m_filter_fcn(
        Accessor(*this, begin),
        begin->timestamp,
        begin->value.measurement
      );
    }
  }

  std::optional<HistoryEntry> m_committed_state;
  History m_history;
  FilterFunction m_filter_fcn;
};