#pragma once

#include <thread>
#include <vector>
#include <iostream>

#include "concurrent_intmap.h"

namespace concurrent_revisions {

class segment;
class revision;

namespace detail {

class versioned_any {
public:
  virtual ~versioned_any() {};
  virtual void release() = 0;
  virtual void collapse(revision &main, segment &parent) = 0;
  virtual void merge(revision &main, revision &join_rev, segment &join) = 0;
};

} // namespace detail

template <class T>
class versioned : public detail::versioned_any {
public:
  const T &get() const;
  void set(const T &v);

  void release();
  void collapse(revision &main, segment &parent);
  void merge(revision &main, revision &join_rev, segment &join);

  //private:
  const T &get(revision &r) const;
  void set(revision &r, const T & v);

  concurrent_intmap<T> versions_;
};

class segment {
public:
  segment(segment *parent);

  void release();
  void collapse(revision &main);

  //private:
  int version_;
  int refcount_;
  segment *parent_;
  std::vector<detail::versioned_any*> written_;
  static int version_count_;
};

class revision {
public:
  revision(segment *root, segment *current);

  template <class F>
  revision *fork(F action);

  void join(revision *join);

  //private:
  segment *root_;
  segment *current_;
  std::thread *task_;

  static __thread revision *current_revision;
};

// implementation

template <class T>
inline const T &versioned<T>::get() const
{
  return get(*revision::current_revision);
}

template <class T>
inline void versioned<T>::set(const T &v)
{
  set(*revision::current_revision, v);
}

template <class T>
inline const T &versioned<T>::get(revision &r) const
{
  segment *s = r.current_;
  while(!versions_.has(s->version_))
    s = s->parent_;
  return versions_.get(s->version_);
}

template <class T>
inline void versioned<T>::set(revision &r, const T &v)
{
  if (!versions_.has(r.current_->version_))
    r.current_->written_.push_back(this);
  versions_.set(r.current_->version_, v);
}

template <class T>
inline void versioned<T>::release()
{
  abort();
}

template <class T>
inline void versioned<T>::collapse(revision &main, segment &parent)
{
  abort();
}

template <class T>
inline void versioned<T>::merge(revision &main, revision &join_rev, segment &join)
{
  abort();
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
      written_[i]->release();
    if (parent_) parent_->release();
  }
}

inline void segment::collapse(revision &main)
{
  // assert: main.current == this
  while(parent_ != main.root_ || parent_->refcount_ == 1) {
    for (size_t i = 0; i < parent_->written_.size(); ++i)
      parent_->written_[i]->collapse(main, *parent_);
    parent_ = parent_->parent_;
  }
}

//-----

inline revision::revision(segment *root, segment *current)
  : root_(root)
  , current_(current)
{
}

template <class F>
inline revision *revision::fork(F action)
{
  std::cout << "forking" << std::endl;
  revision *r = new revision(current_, new segment(current_));
  current_->release();
  current_ = new segment(current_);
  task_ = new std::thread([&]{
      revision *previous = current_revision;
      current_revision = r;
      std::cout << "* " << current_revision << std::endl;
      try {
        action();
      } catch(...) {
      }
      current_revision = previous;
    });
  return r;
}

inline void revision::join(revision *join)
{
  try {
    join->task_->join();
    segment *s = join->current_;
    while(s != join->root_) {
      for (auto p = s->written_.begin(); p != s->written_.end(); ++p)
        (*p)->merge(*this, *join, *s);
      s = s->parent_;
    }
  } catch(...) {
  }
  join->current_->release();
  current_->collapse(*this);
}

//-----

template <typename F>
inline revision *fork(F action)
{
  if (!revision::current_revision) {
    revision::current_revision = new revision(NULL, new segment(NULL));
  }

  return revision::current_revision->fork(action);
}

inline void join(revision *r)
{
  revision::current_revision->join(r);
}

} // concurrent_revisions
