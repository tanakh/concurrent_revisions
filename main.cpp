#include "concurrent-revisions.h"
#include <iostream>

using namespace std;
using namespace concurrent_revisions;

int main(int argc, char *argv[])
{
  versioned<int> v;

  revision *r = fork([&]{
      v.set(1);
    });

  v.set(2);
  cout << v.get() << endl;
  join(r);
  cout << v.get() << endl;

  return 0;
}
