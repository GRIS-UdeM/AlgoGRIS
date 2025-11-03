#include "sg_ParallelSpatAlgorithm.hpp"
namespace gris
{

ParallelAlgorithm::ParallelAlgorithm(unsigned int numberOfThreads)
{
#if THREAD_WAIT_METHOD == SPIN_SLEEP
    SpinSleepWait::allocateStateVector(numberOfThreads);
#endif
    isValid = threadPool.try_spawn(numberOfThreads);

    if (!isValid) {
        std::fprintf(stderr, "Failed to spawn the threadpool\n");
        jassertfalse;
    }
}

} // namespace gris
