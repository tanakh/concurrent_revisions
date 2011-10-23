#include "concurrent_revisions.h"

namespace concurrent_revisions {

std::atomic_int segment::version_count_;

__thread revision_impl *revision_impl::current_revision = nullptr;

class initializer {
public:
  initializer() {
    revision r = fork([]{});
    join(r);
  }
} init ;

} // namespace concurrent_revisions
