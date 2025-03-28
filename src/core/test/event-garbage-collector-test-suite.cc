/*
 * Copyright (c) 2007 INESC Porto
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: Gustavo J. A. M. Carneiro  <gjc@inescporto.pt>
 */

#include "ns3/event-garbage-collector.h"
#include "ns3/test.h"

/**
 * @file
 * @ingroup core-tests
 * @ingroup events
 * @ingroup event-garbage-tests
 * EventGarbageCollector test suite.
 */

/**
 * @ingroup core-tests
 * @defgroup event-garbage-tests EventGarbageCollector test suite
 */

namespace ns3
{

namespace tests
{

/**
 * @ingroup event-garbage-tests
 * Event garbage collector test.
 */
class EventGarbageCollectorTestCase : public TestCase
{
    int m_counter;                   //!< Counter to trigger deletion of events.
    EventGarbageCollector* m_events; //!< Object under test.

    /** Callback to record event invocations. */
    void EventGarbageCollectorCallback();

  public:
    /** Constructor. */
    EventGarbageCollectorTestCase();
    /** Destructor. */
    ~EventGarbageCollectorTestCase() override;
    void DoRun() override;
};

EventGarbageCollectorTestCase::EventGarbageCollectorTestCase()
    : TestCase("EventGarbageCollector"),
      m_counter(0),
      m_events(nullptr)
{
}

EventGarbageCollectorTestCase::~EventGarbageCollectorTestCase()
{
}

void
EventGarbageCollectorTestCase::EventGarbageCollectorCallback()
{
    m_counter++;
    if (m_counter == 50)
    {
        // this should cause the remaining (50) events to be cancelled
        delete m_events;
        m_events = nullptr;
    }
}

void
EventGarbageCollectorTestCase::DoRun()
{
    m_events = new EventGarbageCollector();

    for (int n = 0; n < 100; n++)
    {
        m_events->Track(
            Simulator::Schedule(Simulator::Now(),
                                &EventGarbageCollectorTestCase::EventGarbageCollectorCallback,
                                this));
    }
    Simulator::Run();
    NS_TEST_EXPECT_MSG_EQ(m_events, 0, "");
    NS_TEST_EXPECT_MSG_EQ(m_counter, 50, "");
    Simulator::Destroy();
}

/**
 * @ingroup event-garbage-tests
 * Event garbage collector test suite.
 */
class EventGarbageCollectorTestSuite : public TestSuite
{
  public:
    EventGarbageCollectorTestSuite()
        : TestSuite("event-garbage-collector")
    {
        AddTestCase(new EventGarbageCollectorTestCase());
    }
};

/**
 * @ingroup event-garbage-tests
 * EventGarbageCollectorTestSuite instance variable.
 */
static EventGarbageCollectorTestSuite g_eventGarbageCollectorTestSuite;

} // namespace tests

} // namespace ns3
