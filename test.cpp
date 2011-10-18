#include "concurrent_revisions.h"
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

TEST(gtest, merge)
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

  
}
