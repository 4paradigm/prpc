#include "SpinLock.h"

namespace paradigm4 {
namespace pico {
namespace core {

const int RWSpinLock::MAX_TESTS = 1 << 10;
const int RWSpinLock::MAX_COLLISION = 20;

} // namespace core
} // namespace pico
} // namespace paradigm4
