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

  revision *r = fork([&]{
      cout << revision::current_revision << endl;
      v = 2;
    });
  v = 1;

  EXPECT_EQ(1, (int)v);
  join(r);
  EXPECT_EQ(2, (int)v);
}

TEST(gtest, determin)
{
  versioned<int> x, y;
  x = 0;
  y = 0;

  revision *r = fork([&] {
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

TEST(gtest, determin2)
{
  versioned<int> x, y;
  x = 5;
  y = 7;

  revision *r1 = fork([&] {
      EXPECT_EQ(5, x);
      if (x == 5) y = 1;
      EXPECT_EQ(1, y);
    });

  revision *r2 = fork([&] {
      EXPECT_EQ(7, y);
      if (y == 7) x = 10;
      EXPECT_EQ(10, x);
    });

  // EXPECT_EQ(0, x);
  // EXPECT_EQ(0, y);

  sleep(1);

  x.dump();
  y.dump();
  puts("==================================");
  
  join(r1);

  x.dump();
  y.dump();
  puts("==================================");
  
  // EXPECT_EQ(0, x);
  // EXPECT_EQ(1, y);
  
  join(r2);

  x.dump();
  y.dump();

  EXPECT_EQ(10, x);
  EXPECT_EQ(1, y);
}
