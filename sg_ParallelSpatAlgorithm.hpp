#pragma once

// Disable numa in fork_union
// This disables NUMA related optimisation on linux
#if !defined(FU_ENABLE_NUMA)
    #define FU_ENABLE_NUMA 0
#endif
#include <JuceHeader.h>
#include "Data/sg_constants.hpp"
#include <cmath>
#include <fork_union.hpp>
#include <algorithm>
#include <atomic>
#include <thread>
#include <vector>

namespace gris
{

/**
 * ints aligned to fork_union's default alignement to prevent destructive interference when
 * different threads access adjacent values.
 */
struct AlignedInt {
    alignas(ashvardanian::fork_union::default_alignment_k) int64_t value = 0;
};

/**
 * This block defines CPU-dependant low power wait instructions that are more optimal than this_thread::yield
 */
#if defined(__x86_64__) || defined(_M_X64)
    #include <immintrin.h>
    #define cpuPause() _mm_pause()
#elif defined(__i386__)
    #define cpuPause() __asm__ __volatile__("rep; nop")
#elif defined(__ia64__)
    #define cpuPause() __asm__ __volatile__("hint @pause")
#elif defined(__aarch64__)
    #define cpuPause() __asm__ __volatile__("dmb ishst\n\tyield" ::: "memory")
#elif defined(__arm__)
    #define cpuPause() __asm__ __volatile__("yield")
#elif defined(__sparc) || defined(__sparc__)
    #define cpuPause() __asm__ __volatile__("pause")
#elif defined(__ppc__) || defined(_ARCH_PPC) || defined(_ARCH_PWR) || defined(_ARCH_PWR2) || defined(_POWER)
    #define cpuPause() __asm__ __volatile__("or 27,27,27")
#elif defined(#elif defined(__riscv)
    #define cpuPause() __asm__ __volatile__("pause"))
#elif defined(_MSC_VER)
    #include <windows.h>
    #define cpuPause() YieldProcessor()
#else
    #define cpuPause()
#endif

/**
 * Struct that tracks how many time each of the fork_union worker thread has waited between job
 * and which busy waits an increasing amount of time before starting to put the thread to sleep.
 * This is meant to create a compromise between never sleeping (and using 100% of all cpus all the time)
 * and always sleeping which hurts performance because of thread wakeup latency.
 *
 *
 * Important note : This uses static methods to setup the threadStates vector and to reset it.
 * This means only one instance of SpinSleepWait can function properly. This means if we wanted
 * to use this for hybrid mode, some changes would have to be made in fork_union to allow use
 * to manually access the SpinSleepWait of the thread pool in between job. Since hybrid mode
 * does not seem to benefit from fork_union, this is an ok compromise.
 */
struct SpinSleepWait {
private:
    /**
     * Holds the wait state of every thread. Due to how fork_union works internally,
     * this needs to be static which isn't idal.
     */
    static inline std::vector<AlignedInt> threadStates;
    /**
     * This governs how many time a given thread will cpuPause instead of sleeping before
     * finally sleeping. Lower values means the threads go to sleep faster and tends to
     * use less CPU when idle and higher values means the threads will sleep less often leading
     * to better average latency.
     */
    static inline std::atomic<size_t> numberOfPausesBeforeSleep = 100;

public:
    /**
     *  The numbers 100 and 3000 were tested experimentally.
     *  3000 seems to give a pretty good average latency but uses +-30% of all cpus
     *  at all time on my laptop. 100 uses 5% cpu on my laptop (barely more than sleeping
     *  all the time) and still has performance improvement over sleeping all the time.
     */
    static inline void setPerformancePreset(int preset)
    {
        if (preset == OPTIMIZE_CPU_MULTICORE_PRESET) {
            numberOfPausesBeforeSleep = 100;
        } else if (preset == OPTIMIZE_LATENCY_MULTICORE_PRESET) {
            numberOfPausesBeforeSleep = 10000;
        }
    }

    static inline void allocateStateVector(size_t numElems)
    {
        // tries to increase the size of the vector. This function should never
        // reduce the size of the vector; we would need to have a lock to do that.
        threadStates.resize(std::max(threadStates.size(), numElems));
        resetStates();
    }
    static inline void resetStates()
    {
        for (size_t i = 0; i < threadStates.size(); i++) {
            std::atomic_ref<int64_t> val{ threadStates[i].value };
            val = 0;
        }
    }

    /**
     * This is the standard cpu sleep that will be called by fork_union outside of worker
     * threads.
     */
    inline void operator()() const noexcept { cpuPause(); }

    /**
     * This operator will be called by fork_union worker thread when they have nothing to do
     * @param idx : the index of the thread that called
     */
    inline void operator()(size_t idx) const noexcept
    {
        // nothing should ever decrease the size of the vector or the code after this
        // check is going to be crashing.
        if (idx > threadStates.size()) {
            // Falling into this branch probably means that allocateStateVector was not
            // called yet or that it was not called with the right number of threads.
            cpuPause();
            return;
        }
        std::atomic_ref<int64_t> currentIndex{ threadStates[idx].value };
        // 15 was taken from the original ossia score code and empirically seems like
        // a good value for the short pause.
        if (currentIndex < 15) {
            cpuPause();
            currentIndex += 1;
            return;
        } else if (currentIndex < numberOfPausesBeforeSleep) {
            // repetition is needed here because otherwise the compiler can
            // allegedly optimize some of this out.
            cpuPause();
            cpuPause();
            cpuPause();
            cpuPause();
            cpuPause();
            cpuPause();
            cpuPause();
            cpuPause();
            cpuPause();
            cpuPause();
            currentIndex += 1;
            return;
        } else {
            constexpr std::array<std::chrono::microseconds, 3> threadSleepTimes
                = { std::chrono::microseconds(10), std::chrono::microseconds(100), std::chrono::microseconds(500) };
            auto sleepIdx
                = std::min(static_cast<size_t>(currentIndex - numberOfPausesBeforeSleep), threadSleepTimes.size() - 1);
            std::this_thread::sleep_for(threadSleepTimes[sleepIdx]);
            currentIndex += 1;
        }
    }
};

// compile the code with inconditional sleeps after the threadpool is done with its
// current jobhotspot linux thread sleep time
#define SLEEP 0
// compiles the code so that the threadpool busy waits and use 100% of a core between jobs.
#define NO_SLEEP 1
// compiles the code so that the threadpool uses SpinSleepWait to alternate between busy waits and sleeping.
#define SPIN_SLEEP 2
// SPIN_SLEEP seems to be the best compromise.
#define THREAD_WAIT_METHOD SPIN_SLEEP

// We want to suppress RTSAN if we ever sleep in the audio thread.
// Sleeping is not real-time safe but its a trade off we made to make the algoirithms use
// less than 100% of all cores at all times.
// clang doesn't let us use "defined" in a define used in a #if so we must put the rest of the
// expression direction in the #if.
#define UNSAFE_SLEEP (THREAD_WAIT_METHOD == SPIN_SLEEP || THREAD_WAIT_METHOD == SLEEP)

class ParallelAlgorithm
{
public:
    /**
     * Starts a threadpool and sets the valid bool if it works.
     * It is up to the implemeter of this class to check valid and
     * deal with the failure appropriately.
     *
     * Unless you spawn may algorithms that are going to be processed at the same time
     * (like for the hybrid algorithm), you probably want std::thread::hardware_concurrency()
     * number of threads or very close to this.
     */
    ParallelAlgorithm(unsigned int numberOfThreads);
    /**
     * set to true after successfuly spawning the threadpool
     */
    bool isValid = false;

protected:
    /**
     * fork union threadpool.
     */
#if THREAD_WAIT_METHOD == SPIN_SLEEP
    ashvardanian::fork_union::basic_pool<std::allocator<std::thread>,
                                         SpinSleepWait,
                                         std::size_t,
                                         ashvardanian::fork_union::default_alignment_k>
        threadPool;
#else
    ashvardanian::fork_union::basic_pool_t threadPool;
#endif
};
} // namespace gris
