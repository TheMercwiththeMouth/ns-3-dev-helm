/*
 * Copyright (c) 2008 University of Washington
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#ifndef WALL_CLOCK_CLOCK_SYNCHRONIZER_H
#define WALL_CLOCK_CLOCK_SYNCHRONIZER_H

#include "synchronizer.h"

#include <condition_variable>
#include <mutex>

/**
 * @file
 * @ingroup realtime
 * ns3::WallClockSynchronizer declaration.
 */

namespace ns3
{

/**
 * @ingroup realtime
 * @brief Class used for synchronizing the simulation events to a real-time
 * "wall clock" using Posix clock functions.
 *
 * This synchronizer is used as part of the RealtimeSimulatorImpl.  It is
 * typically not explicitly enabled by users but instead is implicitly
 * enabled when the simulator implementation type is set to real-time; e.g.:
 *
 * @code
 *   GlobalValue::Bind ("SimulatorImplementationType",
 *                      StringValue ("ns3::RealtimeSimulatorImpl"));
 * @endcode
 *
 * before calling any simulator functions.
 *
 * There are a couple of more issues at this level.  Posix clocks provide
 * access to several clocks we could use as a wall clock.  We don't care about
 * time in the sense of 04:30 CEST, we care about some piece of hardware that
 * ticks at some regular period.  The most accurate posix clock in this
 * respect is the @c CLOCK_PROCESS_CPUTIME_ID clock.  This is a high-resolution
 * register in the CPU.  For example, on Intel machines this corresponds to
 * the timestamp counter (TSC) register.  The resolution of this counter will
 * be on the order of nanoseconds.
 *
 * Now, just because we can measure time in nanoseconds doesn't mean we can
 * put our process to sleep to nanosecond resolution.  We are eventually going
 * to use the function @c clock_nanosleep() to sleep until a simulation Time
 * specified by the caller.
 *
 * @todo Add more on jiffies, sleep, processes, etc.
 *
 */
class WallClockSynchronizer : public Synchronizer
{
  public:
    /**
     * Get the registered TypeId for this class.
     * @returns The TypeId.
     */
    static TypeId GetTypeId();

    /** Constructor. */
    WallClockSynchronizer();
    /** Destructor. */
    ~WallClockSynchronizer() override;

    /** Conversion constant between &mu;s and ns. */
    static const uint64_t US_PER_NS = (uint64_t)1000;
    /** Conversion constant between &mu;s and seconds. */
    static const uint64_t US_PER_SEC = (uint64_t)1000000;
    /** Conversion constant between ns and s. */
    static const uint64_t NS_PER_SEC = (uint64_t)1000000000;

  protected:
    /**
     * @brief Do a busy-wait until the normalized realtime equals the argument
     * or the condition variable becomes @c true.

     * The condition becomes @c true if an outside entity (a network device
     * receives a packet), sets the condition and signals the scheduler
     * it needs to re-evaluate.
     *
     * @param [in] ns The target normalized real time we should wait for.
     * @returns @c true if we reached the target time,
     *          @c false if we returned because the condition was set.
     */
    bool SpinWait(uint64_t ns);
    /**
     * Put our process to sleep for some number of nanoseconds.
     *
     * Typically this will be some time equal to an integral number of jiffies.
     * We will usually follow a call to SleepWait with a call to SpinWait
     * to get the kind of accuracy we want.
     *
     * We have to have some mechanism to wake up this sleep in case an external
     * event happens that causes a Schedule event in the simulator.  This newly
     * scheduled event might be before the time we are waiting until, so we have
     * to break out of both the SleepWait and the following SpinWait to go back
     * and reschedule/resynchronize taking the new event into account.  The
     * condition we have saved in m_condition, along with the condition variable
     * m_conditionVariable take care of this for us.
     *
     * This call will return if the timeout expires OR if the condition is
     * set @c true by a call to SetCondition (true) followed by a call to
     * Signal().  In either case, we are done waiting.  If the timeout happened,
     * we return @c true; if a Signal happened we return @c false.
     *
     * @param [in] ns The target normalized real time we should wait for.
     * @returns @c true if we reached the target time,
     *          @c false if we returned because the condition was set.
     */
    bool SleepWait(uint64_t ns);

    // Inherited from Synchronizer
    void DoSetOrigin(uint64_t ns) override;
    bool DoRealtime() override;
    uint64_t DoGetCurrentRealtime() override;
    bool DoSynchronize(uint64_t nsCurrent, uint64_t nsDelay) override;
    void DoSignal() override;
    void DoSetCondition(bool cond) override;
    int64_t DoGetDrift(uint64_t ns) override;
    void DoEventStart() override;
    uint64_t DoEventEnd() override;

    /**
     * @brief Compute a correction to the nominal delay to account for
     * realtime drift since the last DoSynchronize.
     *
     * @param [in] nsNow The current simulation time (in nanosecond units).
     * @param [in] nsDelay The simulation time we need to wait for (normalized to
     * nanosecond units).
     * @returns The corrected delay.
     */
    uint64_t DriftCorrect(uint64_t nsNow, uint64_t nsDelay);

    /**
     * @brief Get the current absolute real time (in ns since the epoch).
     *
     * @returns The current real time, in ns.
     */
    uint64_t GetRealtime();
    /**
     * @brief Get the current normalized real time, in ns.
     *
     * @returns The current normalized real time, in ns.
     */
    uint64_t GetNormalizedRealtime();

    /** Size of the system clock tick, as reported by @c clock_getres, in ns. */
    uint64_t m_jiffy;
    /** Time recorded by DoEventStart. */
    uint64_t m_nsEventStart;

    /** Condition variable for thread synchronizer. */
    std::condition_variable m_conditionVariable;
    /** Mutex controlling access to the condition variable. */
    std::mutex m_mutex;
    /** The condition state. */
    bool m_condition;
};

} // namespace ns3

#endif /* WALL_CLOCK_SYNCHRONIZER_H */
