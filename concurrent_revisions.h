#pragma once

#include <atomic>
#include <functional>
#include <iostream>
#include <thread>
#include <vector>

#include "concurrent_intmap.h"

namespace concurrent_revisions {

class segment;
class revision_impl;
template <class T, class Merge> class versioned;

template <class T>
class default_merger {
public:
  const T &operator()(const T &main, const T &join, const T &root) const {
    return join;
  }
};

template <class T>
class add_merger {
public:
  T operator()(const T &main, const T &join, const T &root) const {
    return main + join - root;
  }
};

template <class T>
class max_merger {
public:
  const T &operator()(const T &main, const T &join, const T &root) const {
    return std::max(main, join);
  }
};

template <class T>
class min_merger {
public:
  const T &operator()(const T &main, const T &join, const T &root) const {
    return std::min(main, join);
  }
};

template <class Iterator>
class max_iter_merger {
public:
  Iterator operator()(Iterator main, Iterator join, Iterator root) const {
    if (*main < *join)
      return join;
    else
      return main;
  }
};

template <class Iterator>
class min_iter_merger {
public:
  Iterator operator()(Iterator main, Iterator join, Iterator root) const {
    if (*main < *join)
      return main;
    else
      return join;
  }
};

namespace detail {

class versioned_any {
public:
  virtual ~versioned_any() {};
  virtual void release(segment &s) = 0;
  virtual void collapse(revision_impl &main, segment &parent) = 0;
  virtual void merge(revision_impl &main, revision_impl &join_rev, segment &join) = 0;
  virtual std::shared_ptr<versioned_any> ptr() = 0;
};

} // namespace detail

template <class T, class Merge>
class versioned_val : public detail::versioned_any {
public:
  versioned_val();

  const T &get() const;
  void set(const T &v);

  void release(segment &s);
  void collapse(revision_impl &main, segment &parent);
  void merge(revision_impl &main, revision_impl &join_rev, segment &join);

  std::shared_ptr<detail::versioned_any> ptr() {
    return q_.lock();
  }

  void dump() {
    versions_.dump();
  }

private:
  const T &get(revision_impl &r) const;
  const T &get(segment &r) const;
  void set(revision_impl &r, const T & v);

  std::weak_ptr<versioned_val<T, Merge> > q_;
  concurrent_intmap<T> versions_;
  Merge mf_;

  friend class versioned<T, Merge>;
};

template <class T, class Merge = default_merger<T> >
class versioned {
public:
  versioned()
    : p_ (std::make_shared<versioned_val<T, Merge> >()) {
    p_->q_ = p_;
    p_->set(T());
  }

  versioned &operator=(const versioned &r) {
    p_->set((T)r);
    return *this;
  }

  versioned &operator=(const T &v) {
    p_->set(v);
    return *this;
  }

  operator const T&() const {
    return p_->get();
  }

  void dump() {
    p_->dump();
  }
  
  std::shared_ptr<versioned_val<T, Merge> > p_;
};

namespace detail_ {
template <typename = void>
class segment_ {
public:
  static std::atomic_int version_count_;
};

template <typename T>
std::atomic_int segment_<T>::version_count_;
} // namespace detail_

class segment : public detail_::segment_<> {
public:
  explicit segment(segment *parent);

  void release();
  void collapse(revision_impl &main);

  //private:
  std::atomic_int version_;
  std::atomic_int refcount_;
  segment *parent_;
  std::vector<std::shared_ptr<detail::versioned_any> > written_;
};

namespace detail_ {
template <typename = void>
class revision_impl_ {
public:
  static __thread revision_impl* current_revision;
};

template <typename T>
__thread revision_impl* revision_impl_<T>::current_revision = nullptr;
} // namespace detail_

class revision_impl : public detail_::revision_impl_<> {
public:
  revision_impl(segment *root, segment *current);

  template <class F>
  revision_impl *fork(F action);

  void join(revision_impl *r);

  //private:
  segment *root_;
  segment *current_;
  std::unique_ptr<std::thread> thread_;
};

// implementation

template <class T, class Merge>
inline versioned_val<T, Merge>::versioned_val()
{
}

template <class T, class Merge>
inline const T &versioned_val<T, Merge>::get() const
{
  return get(*revision_impl::current_revision);
}

template <class T, class Merge>
inline void versioned_val<T, Merge>::set(const T &v)
{
  set(*revision_impl::current_revision, v);
}

template <class T, class Merge>
inline const T &versioned_val<T, Merge>::get(revision_impl &r) const
{
  return get(*r.current_);
}

template <class T, class Merge>
inline const T &versioned_val<T, Merge>::get(segment &r) const
{
  segment *s = &r;
  while(!versions_.has(s->version_)) {
    s = s->parent_;
  }
  return versions_.get(s->version_);
}



template <class T, class Merge>
inline void versioned_val<T, Merge>::set(revision_impl &r, const T &v)
{
  if (!versions_.has(r.current_->version_))
    r.current_->written_.push_back(this->ptr());
  versions_.set(r.current_->version_, v);
}

template <class T, class Merge>
inline void versioned_val<T, Merge>::release(segment &s)
{
  versions_.erase(s.version_);
}

template <class T, class Merge>
inline void versioned_val<T, Merge>::collapse(revision_impl &main, segment &parent)
{
  if (!versions_.has(main.current_->version_))
    set(main, versions_.get(parent.version_));
  versions_.erase(parent.version_);
}

template <class T, class Merge>
inline void versioned_val<T, Merge>::merge(revision_impl &main, revision_impl &join_rev, segment &join)
{
  segment *s = join_rev.current_;
  while(!versions_.has(s->version_))
    s = s->parent_;
  if (s == &join) {
    //set(main, versions_.get(join.version_));
    set(main, mf_(get(), get(join), get(*join_rev.root_)));
  }
}

//-----

inline segment::segment(segment *parent)
  : parent_(parent)
{
  if (parent_) ++parent_->refcount_;
  version_ = version_count_++; // this must be atomic?
  refcount_ = 1;
}

inline void segment::release()
{
  if (--refcount_ == 0) {
    for (size_t i = 0; i < written_.size(); ++i)
      written_[i]->release(*this);
    if (parent_) parent_->release();
  }
}

inline void segment::collapse(revision_impl &main)
{
  // assert: main.current == this
  while(parent_ && (parent_ != main.root_ && parent_->refcount_ == 1)) {
    for (size_t i = 0; i < parent_->written_.size(); ++i)
      parent_->written_[i]->collapse(main, *parent_);
    parent_ = parent_->parent_;
  }
}

//-----

inline revision_impl::revision_impl(segment *root, segment *current)
  : root_(root)
  , current_(current)
  , thread_(nullptr)
{
}

template <class F>
inline revision_impl *revision_impl::fork(F action)
{
  // std::cout << "forking" << std::endl;
  segment *seg = new segment(current_);
  // std::cout << "seg: " << seg << std::endl;
  revision_impl *r = new revision_impl(current_, seg);

  current_->release();
  current_ = new segment(current_);
  r->thread_ = std::unique_ptr<std::thread>(new std::thread(std::bind([](revision_impl *rr, F aa){
      revision_impl *previous = current_revision;
      current_revision = rr;
      try {
        aa();
      } catch(...) {
      }
      current_revision = previous;
      }, r, action)));
  return r;
}

inline void revision_impl::join(revision_impl *r)
{
  try {
    r->thread_->join();
    r->thread_.reset();

    segment *s = r->current_;
    while(s != r->root_) {
      
      for (auto p = s->written_.begin(); p != s->written_.end(); ++p)
        (*p)->merge(*this, *r, *s);
      s = s->parent_;
    }
  } catch(const std::exception& e) {
    std::cerr << e.what() <<std::endl;
  } catch(...) {
  }
  r->current_->release();
  current_->collapse(*this);
}

class revision {
public:
  revision() {}

  explicit revision(revision_impl *impl)
    : impl_(impl) {}

  revision_impl *ptr() {
    return impl_.get();
  }

private:
  std::shared_ptr<revision_impl> impl_;
};

//-----

template <typename F>
inline revision fork(F action)
{
  if (!revision_impl::current_revision) {
    segment *root_segment = new segment(nullptr);

    revision_impl::current_revision = new revision_impl(root_segment, root_segment);
  }

  return revision(revision_impl::current_revision->fork(action));
}

inline void join(revision r)
{
  revision_impl::current_revision->join(r.ptr());
}

namespace detail_ {
class initializer {
public:
  initializer() {
    revision r = fork([]{});
    join(r);
  }
};

template <typename = void>
class initializer_holder {
public:
  static initializer init;
};

template <typename T>
initializer initializer_holder<T>::init;

static initializer& init_ = initializer_holder<>::init;  // hitsuyou?
}  // namespace detail_

} // concurrent_revisions
