#pragma once

#include <limits>
#include <cmath>
#include "utils/logger/logger.hpp"
#include <unordered_map>
#include <unordered_set>

namespace impl {

template <typename GraphCursor>
class Depth {
 public:
  bool depth_at_least(const GraphCursor &cursor, double d) {
    return depth(cursor) >= d;
  }

  double depth(const GraphCursor &cursor) {
    std::unordered_set<GraphCursor> stack;  // TODO do not construct stack in case of using cached value
    assert(stack.size() == 0);
    auto result = get_depth_(cursor, stack);
    assert(stack.size() == 0);
    return result;
  }

 size_t max_stack_size() const { return max_stack_size_; }

 private:
  std::unordered_map<GraphCursor, double> depth_;
  size_t max_stack_size_ = 0;

  double get_depth_(const GraphCursor &cursor, std::unordered_set<GraphCursor> &stack) {
    if (depth_.count(cursor)) {
      return depth_[cursor];
    }

    if (cursor.is_empty()) {
      return depth_[cursor] = std::numeric_limits<double>::infinity();
    }

    if (cursor.letter() == '*' || cursor.letter() == 'X') {
      // INFO("Empty depth " << cursor);
      return depth_[cursor] = 0;
    }

    if (stack.count(cursor)) {
      return depth_[cursor] = std::numeric_limits<double>::infinity();
    }

    auto nexts = cursor.next();
    stack.insert(cursor);
    max_stack_size_ = std::max(max_stack_size_, stack.size());
    double max_child = 0;
    for (const GraphCursor &n : nexts) {
      max_child = std::max(max_child, get_depth_(n, stack));
    }
    stack.erase(cursor);

    return depth_[cursor] = 1 + max_child;
  }
};

template <typename GraphCursor>
class DepthInt {
 public:
  bool depth_at_least(const GraphCursor &cursor, size_t d) {
    return depth(cursor) >= d;
  }

  size_t depth(const GraphCursor &cursor) {
    std::unordered_set<GraphCursor> stack;  // TODO do not construct stack in case of using cached value
    assert(stack.size() == 0);
    auto result = get_depth_(cursor, stack);
    assert(stack.size() == 0);
    return result;
  }

  size_t max_stack_size() const { return max_stack_size_; }
  static const size_t INF = std::numeric_limits<size_t>::max();

 private:
  std::unordered_map<GraphCursor, size_t> depth_;
  size_t max_stack_size_ = 0;

  size_t get_depth_(const GraphCursor &cursor, std::unordered_set<GraphCursor> &stack) {
    if (depth_.count(cursor)) {
      return depth_[cursor];
    }

    if (cursor.is_empty()) {
      return depth_[cursor] = INF;
    }

    if (cursor.letter() == '*' || cursor.letter() == 'X') {
      // INFO("Empty depth " << cursor);
      return depth_[cursor] = 0;
    }

    if (stack.count(cursor)) {
      return depth_[cursor] = INF;
    }

    auto nexts = cursor.next();
    stack.insert(cursor);
    max_stack_size_ = std::max(max_stack_size_, stack.size());
    size_t max_child = 0;
    for (const GraphCursor &n : nexts) {
      max_child = std::max(max_child, get_depth_(n, stack));
    }
    stack.erase(cursor);

    return depth_[cursor] = (max_child == INF) ? INF : 1 + max_child;
  }
};


template <typename GraphCursor>
class DummyDepthAtLeast {
  bool depth_at_least(const GraphCursor &, double) {
    return true;
  }
};

template <typename GraphCursor>
class DepthAtLeast {
  struct Estimation {
    size_t value;
    bool exact;
  };

 public:
  static const size_t STACK_LIMIT = 50000;
  static const size_t INF = std::numeric_limits<size_t>::max();

  bool depth_at_least(const GraphCursor &cursor, double depth) {
    if (depth <= 0) {
      return true;
    }
    return depth_at_least(cursor, static_cast<size_t>(depth));
  }

  bool depth_at_least(const GraphCursor &cursor, size_t depth) {
    if (depth == 0) {
      return true;
    }

    if (depth_.count(cursor)) {
      const auto &cached = depth_.find(cursor)->second;
      if (cached.value >= depth) {
        return true;
      } else if (cached.exact) {
        return false;
      }
    }

    const size_t coef = 2;
    size_t stack_limit = std::max<size_t>(coef * depth, 10);

    assert(stack_limit >= depth);
    assert(stack_limit <= STACK_LIMIT);

    std::unordered_set<GraphCursor> stack;
    get_depth_(cursor, stack, stack_limit);
    assert(stack.empty());

    assert(depth_.count(cursor));
    return depth_at_least(cursor, depth);
  }

  size_t max_stack_size() const { return max_stack_size_; }

 private:
  std::unordered_map<GraphCursor, Estimation> depth_;
  size_t max_stack_size_ = 0;

  Estimation get_depth_(const GraphCursor &cursor, std::unordered_set<GraphCursor> &stack, size_t stack_limit) {
    if (cursor.is_empty()) {
      return depth_[cursor] = {INF, true};
    }

    if (depth_.count(cursor)) {
      if (depth_[cursor].exact || depth_[cursor].value > stack_limit) {
        return depth_[cursor];
      }
    }

    if (cursor.letter() == '*' || cursor.letter() == 'X') {
      return depth_[cursor] = {0, true};
    }

    if (stack.count(cursor)) {
      return depth_[cursor] = {INF, true};
    }

    if (!stack_limit) {
      return depth_[cursor] = {1, false};
    }

    auto nexts = cursor.next();
    stack.insert(cursor);
    max_stack_size_ = std::max(max_stack_size_, stack.size());
    size_t max_child = 0;
    bool exact = true;
    for (const GraphCursor &n : nexts) {
      auto result = get_depth_(n, stack, stack_limit - 1);
      max_child = std::max(max_child, result.value);
      exact = exact && result.exact;
    }

    stack.erase(cursor);

    if (max_child == INF) {
      // Infinity is always "exact"
      return depth_[cursor] = {INF, true};
    } else {
      return depth_[cursor] = {1 + max_child, exact};
    }
  }
};

template <typename GraphCursor>
std::vector<GraphCursor> depth_subset(std::vector<std::pair<GraphCursor, size_t>> initial, bool forward = true) {
  std::unordered_set<GraphCursor> visited;
  struct CursorWithDepth {
    GraphCursor cursor;
    size_t depth;
    bool operator<(const CursorWithDepth &other) const {
      return depth < other.depth;
    }
  };

  std::priority_queue<CursorWithDepth> q;


  std::unordered_map<GraphCursor, size_t> initial_map;
  for (const auto &cursor_with_depth : initial) {
    initial_map[cursor_with_depth.first] = std::max(initial_map[cursor_with_depth.first], cursor_with_depth.second);
  }

  for (const auto &cursor_with_depth : initial_map) {
    q.push({cursor_with_depth.first, cursor_with_depth.second});
  }

  INFO("Initial queue size: " << q.size());
  size_t step = 0;
  while (!q.empty()) {
    CursorWithDepth cursor_with_depth = q.top();
    q.pop();

    if (step % 1000000 == 0) {
      INFO("Step " << step << ", queue size: " << q.size() << " depth: " << cursor_with_depth.depth << " visited size:" << visited.size());
    }
    ++step;

    if (visited.count(cursor_with_depth.cursor)) {
      continue;
    }

    visited.insert(cursor_with_depth.cursor);

    if (cursor_with_depth.depth > 0) {
      auto cursors = forward ? cursor_with_depth.cursor.next() : cursor_with_depth.cursor.prev();
      for (const auto &cursor : cursors) {
        if (!visited.count(cursor)) {
          q.push({cursor, cursor_with_depth.depth - 1});
        }
      }
    }
  }

  return std::vector<GraphCursor>(visited.cbegin(), visited.cend());
}

template <typename GraphCursor>
std::vector<GraphCursor> depth_subset(const GraphCursor& cursor, size_t depth, bool forward = true) {
  return depth_subset(std::vector<std::pair<GraphCursor, size_t>>({std::make_pair(cursor, depth)}), forward);
}

}  // namespace impl

// vim: set ts=2 sw=2 et :