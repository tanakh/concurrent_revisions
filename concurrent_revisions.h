#pragma once

#include <thread>
#include <vector>
#include <iostream>
#include <functional>

#include "concurrent_intmap.h"

namespace concurrent_revisions {

class segment;
class revision;
template <class T> class versioned;

namespace detail {

class versioned_any {
public:
  virtual ~versioned_any() {};
  virtual void release(segment &s) = 0;
  virtual void collapse(revision &main, segment &parent) = 0;
  virtual void merge(revision &main, revision &join_rev, segment &join) = 0;
  virtual std::shared_ptr<versioned_any> ptr() = 0;
};

} // namespace detail

template <class T>
class versioned_val : public detail::versioned_any {
public:
  versioned_val() {
    printf("construct!!! %p\n", this);
  }

  const T &get() const;
  void set(const T &v);

  void release(segment &s);
  void collapse(revision &main, segment &parent);
  void merge(revision &main, revision &join_rev, segment &join);

  std::shared_ptr<detail::versioned_any> ptr() {
    std::cout << "NULL? " << this->q_.get() << std::endl;
    return q_;
  }

  void dump() {
    versions_.dump();
  }

  //private:
  const T &get(revision &r) const;
  void set(revision &r, const T & v);

  std::shared_ptr<versioned_val<T> > q_;
  concurrent_intmap<T> versions_;
};

template <class T>
class versioned {
public:
  versioned()
    : p_ (new versioned_val<T>()) {
    p_->q_ = p_;
  }

  versioned &operator=(const T &v) {
    p_->set(v);
    return *this;
  }

  operator const T&() const {
    return p_->get();
  }
  
  std::shared_ptr<versioned_val<T> > p_;
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
  std::vector<std::shared_ptr<detail::versioned_any> > written_;
  static int version_count_;
};

class revision {
public:
  revision(segment *root, segment *current);

  template <class F>
  revision *fork(F action);

  void join(revision *r);

  //private:
  segment *root_;
  segment *current_;
  std::thread *thread_;

  static __thread revision *current_revision;
};

// implementation

template <class T>
inline const T &versioned_val<T>::get() const
{
  return get(*revision::current_revision);
}

template <class T>
inline void versioned_val<T>::set(const T &v)
{
  set(*revision::current_revision, v);
}

template <class T>
inline const T &versioned_val<T>::get(revision &r) const
{
  segment *s = r.current_;
  printf("get! %p\n", s);
  while(!versions_.has(s->version_)) {
    s = s->parent_;
    printf(" %p\n", s);
  }
  return versions_.get(s->version_);
}

template <class T>
inline void versioned_val<T>::set(revision &r, const T &v)
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
inline void versioned_val<T>::collapse(revision &main, segment &parent)
{
  if (!versions_.has(main.current_->version_))
    set(main, versions_.get(parent.version_));
  versions_.erase(parent.version_);
}

template <class T>
inline void versioned_val<T>::merge(revision &main, revision &join_rev, segment &join)
{
  segment *s = join_rev.current_;
  while(!versions_.has(s->version_))
    s = s->parent_;
  if (s == &join)
    set(main, versions_.get(join.version_));
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

inline void segment::collapse(revision &main)
{
  // assert: main.current == this
  while(parent_ && (parent_ != main.root_ || parent_->refcount_ == 1)) {
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
  segment *seg = new segment(current_);
  std::cout << "seg: " << seg << std::endl;
  revision *r = new revision(current_, seg);

  std::cout << "0* " << r << std::endl;
  std::cout << "0+ " << r->current_ << std::endl;
  std::cout << "0- " << r->root_ << std::endl;

  current_->release();
  current_ = new segment(current_);
  thread_ = new std::thread(std::bind([](revision *rr, F aa){
      revision *previous = current_revision;
      current_revision = rr;
      std::cout << "* " << current_revision << std::endl;
      std::cout << "+ " << current_revision->current_ << std::endl;
      std::cout << "- " << current_revision->root_ << std::endl;
      try {
        std::cout << "* " << current_revision << std::endl;
        std::cout << "+ " << current_revision->current_ << std::endl;
        std::cout << "- " << current_revision->root_ << std::endl;

        aa();
      } catch(...) {
      }
      current_revision = previous;
      }, r, action));
  return r;
}

inline void revision::join(revision *r)
{
  try {
    thread_->join();
    segment *s = r->current_;
    while(s != r->root_) {
      for (auto p = s->written_.begin(); p != s->written_.end(); ++p)
        (*p)->merge(*this, *r, *s);
      s = s->parent_;
    }
  } catch(...) {
  }
  r->current_->release();
  current_->collapse(*this);
}

//-----

template <typename F>
inline revision *fork(F action)
{
  if (!revision::current_revision) {
    segment *root_segment = new segment(NULL);

    revision::current_revision = new revision(root_segment, root_segment);
  }

  return revision::current_revision->fork(action);
}

inline void join(revision *r)
{
  revision::current_revision->join(r);
}

} // concurrent_revisions
