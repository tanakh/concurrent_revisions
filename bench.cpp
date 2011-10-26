#include "concurrent_revisions.h"

#include <sys/time.h>
#include <unistd.h>
#include <stdarg.h>
 
struct __bench__ {
  double start;
  char msg[100];
  __bench__(const char* format, ...)
  __attribute__((format(printf, 2, 3)))
  {
    va_list args;
    va_start(args, format);
    vsnprintf(msg, sizeof(msg), format, args);
    va_end(args);
 
    start = sec();
  }
  ~__bench__() {
    fprintf(stderr, "%s: %.6f sec\n", msg, sec() - start);
  }
  double sec() {
    struct timeval tv;
    gettimeofday(&tv, nullptr);
    return tv.tv_sec + tv.tv_usec * 1e-6;
  }
  operator bool() { return false; }
};
 
#define bench(...) if(__bench__ __b__ = __bench__(__VA_ARGS__));else

using namespace concurrent_revisions;
using namespace std;

template <class Iterator>
void sequential_sum(Iterator p, Iterator q, int64_t &sum)
{
  while(p != q) sum = sum + *p++;
}

template <class Iterator>
void parallel_sum(Iterator p, Iterator q, versioned<int64_t, add_merger<int64_t> > &sum)
{
  size_t len = q - p;
  if (len < 100000000) {
    bench("local sum (%lu)", len) {
    int64_t local_sum = sum;
    while(p != q) local_sum = local_sum + *p++;
    sum = local_sum;
    return;
    }
  } 
  else {
    revision r1 = fork([&]{parallel_sum(p, p + len / 2, sum);});
    revision r2 = fork([&]{parallel_sum(p + len / 2, q, sum);});
    join(r1);
    join(r2);
  }
}

int sequential_fib(int n)
{
  if (n <= 1) return n;
  return
    sequential_fib(n - 1) + 
    sequential_fib(n - 2);
}

void parallel_fib(int n, versioned<int, add_merger<int> > &sum)
{
  if (n < 40) {
    sum = sum + sequential_fib(n);
  }
  else {
    revision r1 = fork([&]{ parallel_fib(n - 1, sum); });
    revision r2 = fork([&]{ parallel_fib(n - 2, sum); });
    join(r1);
    join(r2);
  }
}

int main(int argc, char *argv[])
{
  if (argc <= 1)
    return 1;

  bench("par") {
    versioned<int, add_merger<int> > sum;
    parallel_fib(atoi(argv[1]), sum);
    cout << (int)sum << endl;
  }

  bench("seq") {
    cout << sequential_fib(atoi(argv[1])) << endl;
  }

  /*
  vector<int64_t> v(atoi(argv[1]));
  for (size_t i = 0; i < v.size(); ++i)
    v[i] = i;

  bench("seq") {
    int64_t sum = 0;
    sequential_sum(v.begin(), v.end(), sum);
    cout << sum << endl;
  }

  bench("par") {
    versioned<int64_t, add_merger<int64_t> > sum;
    parallel_sum(v.begin(), v.end(), sum);
    cout << (int64_t)sum << endl;
  }
  */

  return 0;
}
