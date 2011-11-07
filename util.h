#pragma once

#include <algorithm>
#include <iterator>
#include "concurrent_revisions.h"

namespace concurrent_revisions {

template <class Iterator, class F>
void parallel_foreach(Iterator begin, Iterator end, F f, size_t min_parallel = 1024)
{
  size_t len = end - begin;

  if (len <= min_parallel) {
    std::for_each(begin, end, f);
  }
  else {
    revision r = fork([&]{
        parallel_foreach(begin, begin + len / 2, f, min_parallel);
      });
    parallel_foreach(begin + len / 2, end, f, min_parallel);
    join(r);
  }
}

template <class InputIterator, class OutputRandomAccessIterator, class F>
void parallel_transform(InputIterator begin, InputIterator end, OutputRandomAccessIterator out, F f, size_t min_parallel = 1024)
{
  size_t const len = end - begin;

  if (len <= min_parallel) {
    std::transform(begin, end, out, f);
  }
  else {
    revision r = fork([&begin, &out, &len, &f, &min_parallel]{
        parallel_transform(begin, begin + len / 2, out, f, min_parallel);
      });
    parallel_transform(begin + len / 2, end, out + len / 2, f, min_parallel);
    join(r);
  }
}

template <class InputIterator1, class InputIterator2, class OutputRandomAccessIterator, class F>
void parallel_transform(InputIterator1 begin1, InputIterator1 end1, InputIterator2 begin2, InputIterator2 end2, OutputRandomAccessIterator out, F f, size_t min_parallel = 1024)
{
  size_t const len1 = end1 - begin1;
  size_t const len2 = end2 - begin2;
  size_t const len = std::min(len1, len2);

  if (len <= min_parallel) {
    std::transform(begin1, begin1+len, begin2, out, f);
  }
  else {
    revision r = fork([&len, &begin1, &begin2, &out, &f, &min_parallel]{
        parallel_transform(begin1, begin1 + len / 2, begin2, begin2 + len / 2, out, f, min_parallel);
      });

    parallel_transform(begin1 + len / 2, begin1 + len, begin2 + len / 2, begin2 + len, out + len / 2, f, min_parallel);
    join(r);
  }
}

template <class Iterator>
void parallel_max_element_impl(Iterator first, Iterator last, versioned<Iterator, max_iter_merger<Iterator> >& v, std::size_t min_parallel)
{
  std::size_t len = std::distance(first, last);

  if (len <= min_parallel) {
    v = std::max_element(first, last);
  } else {
    revision r = fork([&] {
        parallel_max_element_impl(first, first + len/2, v, min_parallel);
      });
    parallel_max_element_impl(first + len/2, last, v, min_parallel);
    join(r);
  }
}

template <class Iterator>
Iterator parallel_max_element(Iterator first, Iterator last, std::size_t min_parallel = 1024)
{
  versioned<Iterator, max_iter_merger<Iterator> > v;
  parallel_max_element_impl(first, last, v, min_parallel);
  return v;
}

template <class Iterator>
void parallel_min_element_impl(Iterator first, Iterator last, versioned<Iterator, min_iter_merger<Iterator> >& v, std::size_t min_parallel)
{
  std::size_t len = std::distance(first, last);

  if (len <= min_parallel) {
    v = std::min_element(first, last);
  } else {
    revision r = fork([&] {
        parallel_min_element_impl(first, first + len/2, v, min_parallel);
      });
    parallel_min_element_impl(first + len/2, last, v, min_parallel);
    join(r);
  }
}

template <class Iterator>
Iterator parallel_min_element(Iterator first, Iterator last, std::size_t min_parallel = 1024)
{
  versioned<Iterator, min_iter_merger<Iterator> > v;
  parallel_min_element_impl(first, last, v, min_parallel);
  return v;
}

template <class Iterator>
void parallel_stable_sort(Iterator first, Iterator last, std::size_t min_parallel = 1024)
{
  std::size_t len = std::distance(first, last);

  if (len <= min_parallel) {
    std::stable_sort(first, last);
  } else {
    Iterator mid = first + len/2;
    revision r = fork([&] {
        parallel_stable_sort(first, mid, min_parallel);
      });
    parallel_stable_sort(mid, last, min_parallel);
    join(r);
    std::inplace_merge(first, mid, last);
  }
}


} // namespace concurrent_revisions
