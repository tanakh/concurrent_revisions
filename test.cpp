#include "concurrent_revisions.h"
#include <iostream>

#include <gtest/gtest.h>

using namespace std;
using namespace concurrent_revisions;

TEST(gtest, simple)
{
  versioned<int> v;

  revision *r = fork([&]{
      cout << revision::current_revision << endl;
      v.set(2);
    });
  v.set(1);

  EXPECT_EQ(1, v.get());
  join(r);
  EXPECT_EQ(2, v.get());
}
