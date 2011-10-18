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
    auto pib = dat_.insert(make_pair(ix, v));
    if (!pib.second) pib.first->second = v;
  }

  void erase(int ix) {
    std::lock_guard<std::mutex> lk(m_);
    dat_.erase(ix);
  }

  void dump() {
    std::lock_guard<std::mutex> lk(m_);
    std::cout << "vvvvv" << std::endl;
    for(auto p = dat_.begin(); p != dat_.end(); ++p)
      std::cout << p->first << ": " << p->second << std::endl;
    std::cout << "^^^^^" << std::endl;
  }

private:
  std::unordered_map<int, V> dat_;
  mutable std::mutex m_;
};

} // namespace concurrent_revisions
