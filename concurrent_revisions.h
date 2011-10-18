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
template <class T> class versioned;

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

template <class T>
class versioned_val : public detail::versioned_any {
public:
  versioned_val();

  const T &get() const;
  void set(const T &v);

  void release(segment &s);
  void collapse(revision_impl &main, segment &parent);
  void merge(revision_impl &main, revision_impl &join_rev, segment &join);

  std::shared_ptr<detail::versioned_any> ptr() {
    return q_;
  }

  void dump() {
    versions_.dump();
  }

  //private:
  const T &get(revision_impl &r) const;
  void set(revision_impl &r, const T & v);

  std::shared_ptr<versioned_val<T> > q_;
  concurrent_intmap<T> versions_;
};

template <class T>
class versioned {
public:
  versioned()
    : p_ (new versioned_val<T>()) {
    p_->q_ = p_;
    p_->set(T());
  }

  template <class U>
  versioned &operator=(const versioned<U> &r) {
    p_->set((U)r);
    return *this;
  }

  template <class U>
  versioned &operator=(versioned<U> &r) {
    p_->set((U)r);
    return *this;
  }

  template <class U>
  versioned &operator=(const U &v) {
    p_->set(v);
    return *this;
  }

  operator const T&() const {
    return p_->get();
  }

  void dump() {
    p_->dump();
  }
  
  std::shared_ptr<versioned_val<T> > p_;
};

class segment {
public:
  segment(segment *parent);

  void release();
  void collapse(revision_impl &main);

  //private:
  std::atomic_int version_;
  std::atomic_int refcount_;
  segment *parent_;
  std::vector<std::shared_ptr<detail::versioned_any> > written_;
  static std::atomic_int version_count_;
};

class revision_impl {
public:
  revision_impl(segment *root, segment *current);

  template <class F>
  revision_impl *fork(F action);

  void join(revision_impl *r);

  //private:
  segment *root_;
  segment *current_;
  std::thread *thread_;

  static __thread revision_impl *current_revision;
};

// implementation

template <class T>
inline versioned_val<T>::versioned_val()
{
}

template <class T>
inline const T &versioned_val<T>::get() const
{
  return get(*revision_impl::current_revision);
}

template <class T>
inline void versioned_val<T>::set(const T &v)
{
  set(*revision_impl::current_revision, v);
}

template <class T>
inline const T &versioned_val<T>::get(revision_impl &r) const
{
  segment *s = r.current_;
  while(!versions_.has(s->version_)) {
    s = s->parent_;
  }
  return versions_.get(s->version_);
}

template <class T>
inline void versioned_val<T>::set(revision_impl &r, const T &v)
{
  if (!versions_.has(r.current_->version_))
    r.current_->written_.push_back(this->ptr());
  versions_.set(r.current_->version_, v);
}

template <class T>
inline void versioned_val<T>::release(segment &s)
{
  versions_.erase(s.version_);
}

template <class T>
inline void versioned_val<T>::collapse(revision_impl &main, segment &parent)
{
  if (!versions_.has(main.current_->version_))
    set(main, versions_.get(parent.version_));
  versions_.erase(parent.version_);
}

template <class T>
inline void versioned_val<T>::merge(revision_impl &main, revision_impl &join_rev, segment &join)
{
  puts("merge now!!!");
  segment *s = join_rev.current_;
  while(!versions_.has(s->version_))
    s = s->parent_;
  if (s == &join) {
    std::cout << "merging " << versions_.get(join.version_) << std::endl;
    set(main, versions_.get(join.version_));
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
{
}

template <class F>
inline revision_impl *revision_impl::fork(F action)
{
  std::cout << "forking" << std::endl;
  segment *seg = new segment(current_);
  std::cout << "seg: " << seg << std::endl;
  revision_impl *r = new revision_impl(current_, seg);

  current_->release();
  current_ = new segment(current_);
  r->thread_ = new std::thread(std::bind([](revision_impl *rr, F aa){
      revision_impl *previous = current_revision;
      current_revision = rr;
      try {
        aa();
      } catch(...) {
      }
      current_revision = previous;
      }, r, action));
  return r;
}

inline void revision_impl::join(revision_impl *r)
{
  try {
    r->thread_->join();
    segment *s = r->current_;
    printf("join time!: %p\n", s);
    while(s != r->root_) {
      puts("join: merge?");
      
      for (auto p = s->written_.begin(); p != s->written_.end(); ++p)
        (*p)->merge(*this, *r, *s);
      s = s->parent_;
    }
  } catch(const std::exception& e) {
    std::cout << e.what() <<std::endl;
  } catch(...) {
  }
  r->current_->release();
  current_->collapse(*this);
}

class revision {
public:
  revision(revision_impl *impl)
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
    segment *root_segment = new segment(NULL);

    revision_impl::current_revision = new revision_impl(root_segment, root_segment);
  }

  return revision(revision_impl::current_revision->fork(action));
}

inline void join(revision r)
{
  puts("joiN!!!!!!!!!!!!");
  revision_impl::current_revision->join(r.ptr());
}

} // concurrent_revisions
