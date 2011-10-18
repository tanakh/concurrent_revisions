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
  
  // EXPECT_EQ(1, v.get());
  // join(r);
  // EXPECT_EQ(2, v.get());
}
