#include "concurrent_revisions.h"

namespace concurrent_revisions {

int segment::version_count_ = 0;

__thread revision *revision::current_revision = NULL;

class initializer {
public:
  initializer() {
    revision *r = fork([]{});
    join(r);
  }
} init ;

} // namespace concurrent_revisions
