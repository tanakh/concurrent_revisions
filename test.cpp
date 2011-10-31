#include "concurrent_revisions.h"
#include "util.h"
#include <iostream>

#include <gtest/gtest.h>

using namespace std;
using namespace concurrent_revisions;

TEST(gtest, initialize)
{
  versioned<int> v;
  v = 0;
  EXPECT_EQ(0, (int)v);
}

TEST(gtest, simple)
{
  versioned<int> v;

  revision r = fork([&]{
      //      cout << revision::current_revision << endl;
      v = 2;
    });
  v = 1;

  EXPECT_EQ(1, (int)v);
  join(r);
  EXPECT_EQ(2, (int)v);
}

TEST(gtest, determine)
{
  versioned<int> x, y;
  x = 0;
  y = 0;

  revision r = fork([&] {
      EXPECT_EQ(0, x);
      x = 1;
      EXPECT_EQ(1, x);
    });

  EXPECT_EQ(0, x);
  y = x;
  EXPECT_EQ(0, y);

  x.dump();
  y.dump();

  join(r);

  x.dump();
  y.dump();

  EXPECT_EQ(1, x);
  EXPECT_EQ(0, y);
}

TEST(gtest, determine2)
{
  versioned<int> x, y;
  x = 5;
  y = 7;

  revision r1 = fork([&] {
      EXPECT_EQ(5, x);
      if (x == 5) y = 1;
      EXPECT_EQ(1, y);
    });

  revision r2 = fork([&] {
      EXPECT_EQ(7, y);
      if (y == 7) x = 10;
      EXPECT_EQ(10, x);
    });

  join(r1);
  join(r2);

  EXPECT_EQ(10, x);
  EXPECT_EQ(1, y);
}

TEST(gtest, determine3)
{
  versioned<int> x, y;
  x = 5;
  y = 7;

  revision r1 = fork([&] {
      EXPECT_EQ(5, x);
      revision r2 = fork([&] {
          EXPECT_EQ(7, y);
          if (y == 7) x = 10;
          EXPECT_EQ(10, x);
        });
      if (x == 5) y = 1;
      EXPECT_EQ(1, y);
      join(r2);
      EXPECT_EQ(10, x);
      EXPECT_EQ(1, y);
    });

  if(x == 5) y = 111;

  EXPECT_EQ(5, x);
  EXPECT_EQ(111, y);
  join(r1);
  EXPECT_EQ(10, x);
  EXPECT_EQ(1, y);
}

TEST(gtest, determine4)
{
  versioned<int> x, y;
  x = 5;
  y = 7;

  revision r1 = fork([&] {
      EXPECT_EQ(5, x);
      if (x == 5) y = 1;
      EXPECT_EQ(5, x);
      EXPECT_EQ(1, y);
    });

  if(x == 5) y = 111;

  EXPECT_EQ(5, x);
  EXPECT_EQ(111, y);
  join(r1);
  EXPECT_EQ(5, x);
  EXPECT_EQ(1, y);

  revision r2 = fork([&] {
      EXPECT_EQ(1, y);
      EXPECT_EQ(5, x);
      if (y == 1) x = 222;
      EXPECT_EQ(222, x);
    });
  
  if(y == 1) x = 2;

  EXPECT_EQ(2, x);
  EXPECT_EQ(1, y);
  join(r2);
  EXPECT_EQ(222, x);
  EXPECT_EQ(1, y);
}

TEST(gtest, merger)
{
  versioned<int, add_merger<int> > x_add;
  versioned<int, max_merger<int> > y_max;
  versioned<int, min_merger<int> > z_min;

  x_add = y_max = z_min = 0;

  revision r1 = fork([&] {
      x_add = x_add + 5;
      y_max = 10;
      z_min = -5;
    });

  x_add = x_add + 3;
  y_max = 5;
  z_min = -10;

  join(r1);

  EXPECT_EQ(8, x_add);
  EXPECT_EQ(10, y_max);
  EXPECT_EQ(-10, z_min);

  revision r2 = fork([&] {
      x_add = x_add + 5;
      y_max = 10;
      z_min = -5;
    });

  join(r2);
}

TEST(gtest, add_merger)
{
  versioned<int, add_merger<int> > x;
  x = 100;

  revision r2;
  
  revision r1 = fork([&] {
      x = x + 2;

      r2 = fork([&] {
          x = x + 4;
        });

      x = x + 3;
    });

  x = x + 1;

  join(r1);
  join(r2);

  EXPECT_EQ(110, x);
}

template <class Iterator>
void parallel_sum(Iterator p, Iterator q, versioned<int, add_merger<int> > &sum)
{
  size_t len = q - p;
  if (len < 1024) {
    while(p != q) sum = sum + *p++;
    return;
  }

  revision r1 = fork([&]{parallel_sum(p, p + len / 2, sum);});
  revision r2 = fork([&]{parallel_sum(p + len / 2, q, sum);});
  join(r1);
  join(r2);
}

TEST(gtest, parallel_sum)
{
  vector<int> dat(10000);
  for (size_t i=0; i< dat.size(); ++i)
    dat[i] = i;

  versioned<int, add_merger<int> > sum;
  parallel_sum(dat.begin(), dat.end(), sum);

  EXPECT_EQ(10000 * 9999 / 2, sum);
}

TEST(gtest, parallel_foreach)
{
  vector<int> v(10000, 1);
  versioned<int, add_merger<int> > sum;
  parallel_foreach(v.begin(), v.end(), [&](int n){ sum = sum + n; });

  EXPECT_EQ(10000, sum);
}

TEST(gtest, parallel_transform)
{
  vector<int> v(10000);
  for (size_t i = 0; i < v.size(); ++i)
    v[i] = i;

  parallel_transform(v.begin(), v.end(), v.begin(), [&](int n){ return n * 2; });

  for (size_t i = 0; i < v.size(); ++i) {
    EXPECT_EQ((int)i * 2, v[i]);
  }
}

TEST(gtest, parallel_transform2)
{
  vector<int> v(10000);
  for (size_t i = 0; i < v.size(); ++i)
    v[i] = i;

  parallel_transform(v.begin(), v.end(), v.begin(), v.end(), v.begin(), [](int x, int y){ return x + y; });

  for (size_t i = 0; i < v.size(); ++i) {
    EXPECT_EQ((int)i * 2, v[i]);
  }

}
