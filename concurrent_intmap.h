#pragma once

#include <unordered_map>
#include <thread>

namespace concurrent_revisions {

template <class V>
class concurrent_intmap {
public:
  const bool has(int ix) const {
    std::lock_guard<std::mutex> lk(m_);
    return dat_.count(ix) != 0;
  }

  const V &get(int ix) const {
    std::lock_guard<std::mutex> lk(m_);
    return dat_.find(ix)->second;
  }

  void set(int ix, const V &v) {
    std::lock_guard<std::mutex> lk(m_);
    dat_.insert(make_pair(ix, v));
  }

private:
  std::unordered_map<int, V> dat_;
  mutable std::mutex m_;
};

} // namespace concurrent_revisions
