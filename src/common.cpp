#include "common.h"
#include <sys/prctl.h>
#include <sys/syscall.h>

namespace paradigm4 {
namespace pico {
namespace core {

static thread_local int _tid = syscall(SYS_gettid);
int gettid(){
    return _tid;
}
static int _pid = syscall(SYS_getpid);
int getpid(){
    return _pid;
}
void set_thread_name(const std::string& name){
#if __GLIBC__ > 2 || (__GLIBC__ == 2 && __GLIBC_MINOR >= 12
    pthread_setname_np(pthread_self(), name.data());
#else
    (void)name;
#endif
}

} // namespace core
} // namespace pico
} // namespace paradigm4
