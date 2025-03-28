/*
 * Copyright (c) 2016 Natale Patriciello <natale.patriciello@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 */

#include "tcp-error-model.h"
#include "tcp-general-test.h"

#include "ns3/log.h"
#include "ns3/node.h"
#include "ns3/rtt-estimator.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TcpRttEstimationTestSuite");

/**
 * @ingroup internet-test
 *
 * @brief Check Rtt calculations
 *
 * First check is that, for each ACK, we have a valid estimation of the RTT.
 * The second check is that, when updating RTT history, we should consider
 * retransmission only segments which sequence number is lower than the highest
 * already transmitted.
 */
class TcpRttEstimationTest : public TcpGeneralTest
{
  public:
    /**
     * @brief Constructor.
     * @param desc Test description.
     * @param enableTs Enable TimeStamp option.
     * @param pktCount Packet number.
     */
    TcpRttEstimationTest(const std::string& desc, bool enableTs, uint32_t pktCount);

  protected:
    Ptr<TcpSocketMsgBase> CreateReceiverSocket(Ptr<Node> node) override;
    Ptr<TcpSocketMsgBase> CreateSenderSocket(Ptr<Node> node) override;

    void Rx(const Ptr<const Packet> p, const TcpHeader& h, SocketWho who) override;
    void Tx(const Ptr<const Packet> p, const TcpHeader& h, SocketWho who) override;
    void UpdatedRttHistory(const SequenceNumber32& seq,
                           uint32_t sz,
                           bool isRetransmission,
                           SocketWho who) override;
    void RttTrace(Time oldTime, Time newTime) override;
    void FinalChecks() override;

    void ConfigureEnvironment() override;

  private:
    bool m_enableTs;                 //!< Enable TimeStamp option
    bool m_rttChanged;               //!< True if RTT has changed.
    SequenceNumber32 m_highestTxSeq; //!< Highest sequence number sent.
    uint32_t m_pktCount;             //!< Packet counter.
    uint32_t m_dataCount;            //!< Data counter.
};

TcpRttEstimationTest::TcpRttEstimationTest(const std::string& desc,
                                           bool enableTs,
                                           uint32_t pktCount)
    : TcpGeneralTest(desc),
      m_enableTs(enableTs),
      m_rttChanged(false),
      m_highestTxSeq(0),
      m_pktCount(pktCount),
      m_dataCount(0)
{
}

void
TcpRttEstimationTest::ConfigureEnvironment()
{
    TcpGeneralTest::ConfigureEnvironment();
    SetAppPktCount(m_pktCount);
    SetPropagationDelay(MilliSeconds(50));
    SetTransmitStart(Seconds(2));
    SetMTU(500);
}

Ptr<TcpSocketMsgBase>
TcpRttEstimationTest::CreateReceiverSocket(Ptr<Node> node)
{
    Ptr<TcpSocketMsgBase> s = TcpGeneralTest::CreateReceiverSocket(node);
    if (!m_enableTs)
    {
        s->SetAttribute("Timestamp", BooleanValue(false));
    }

    return s;
}

Ptr<TcpSocketMsgBase>
TcpRttEstimationTest::CreateSenderSocket(Ptr<Node> node)
{
    Ptr<TcpSocketMsgBase> s = TcpGeneralTest::CreateSenderSocket(node);
    if (!m_enableTs)
    {
        s->SetAttribute("Timestamp", BooleanValue(false));
    }

    return s;
}

void
TcpRttEstimationTest::Tx(const Ptr<const Packet> p, const TcpHeader& h, SocketWho who)
{
    if (who == SENDER && h.GetFlags() != TcpHeader::SYN)
    {
        if (m_highestTxSeq < h.GetSequenceNumber())
        {
            m_highestTxSeq = h.GetSequenceNumber();
            m_dataCount = 0;
        }

        Ptr<RttEstimator> rttEstimator = GetRttEstimator(SENDER);
        NS_TEST_ASSERT_MSG_NE(rttEstimator,
                              nullptr,
                              "rtt is 0 (and should be different from zero)");
        NS_LOG_DEBUG("S Tx: seq=" << h.GetSequenceNumber() << " ack=" << h.GetAckNumber());
        NS_TEST_ASSERT_MSG_NE(rttEstimator->GetEstimate(),
                              Seconds(1),
                              "Default Estimate for the RTT");
    }
}

void
TcpRttEstimationTest::Rx(const Ptr<const Packet> p, const TcpHeader& h, SocketWho who)
{
    if (who == RECEIVER)
    {
        NS_LOG_DEBUG("R Rx: seq=" << h.GetSequenceNumber() << " ack=" << h.GetAckNumber());
    }
}

void
TcpRttEstimationTest::UpdatedRttHistory(const SequenceNumber32& seq,
                                        uint32_t sz,
                                        bool isRetransmission,
                                        SocketWho who)
{
    if (sz == 0)
    {
        return;
    }

    if (seq < m_highestTxSeq)
    {
        NS_TEST_ASSERT_MSG_EQ(isRetransmission, true, "A retransmission is not flagged as such");
    }
    else if (seq == m_highestTxSeq && m_dataCount == 0)
    {
        NS_TEST_ASSERT_MSG_EQ(isRetransmission,
                              false,
                              "Incorrectly flagging seq as retransmission");
        m_dataCount++;
    }
    else if (seq == m_highestTxSeq && m_dataCount > 0)
    {
        NS_TEST_ASSERT_MSG_EQ(isRetransmission, true, "A retransmission is not flagged as such");
    }
}

void
TcpRttEstimationTest::RttTrace(Time oldTime, Time newTime)
{
    NS_LOG_DEBUG("Rtt changed to " << newTime.GetSeconds());
    m_rttChanged = true;
}

void
TcpRttEstimationTest::FinalChecks()
{
    NS_TEST_ASSERT_MSG_EQ(m_rttChanged, true, "Rtt was not updated");
}

/**
 * @ingroup internet-test
 *
 * @brief Check Rtt calculations with packet losses.
 *
 * @see TcpRttEstimationTest
 */
class TcpRttEstimationWithLossTest : public TcpRttEstimationTest
{
  public:
    /**
     * @brief Constructor.
     * @param desc Test description.
     * @param enableTs Enable TimeStamp option.
     * @param pktCount Packet number.
     * @param toDrop List of packet to drop.
     */
    TcpRttEstimationWithLossTest(const std::string& desc,
                                 bool enableTs,
                                 uint32_t pktCount,
                                 std::vector<uint32_t> toDrop);

  protected:
    Ptr<ErrorModel> CreateReceiverErrorModel() override;

  private:
    std::vector<uint32_t> m_toDrop; //!< Packets to drop.
};

TcpRttEstimationWithLossTest::TcpRttEstimationWithLossTest(const std::string& desc,
                                                           bool enableTs,
                                                           uint32_t pktCount,
                                                           std::vector<uint32_t> toDrop)
    : TcpRttEstimationTest(desc, enableTs, pktCount),
      m_toDrop(toDrop)
{
}

Ptr<ErrorModel>
TcpRttEstimationWithLossTest::CreateReceiverErrorModel()
{
    Ptr<TcpSeqErrorModel> errorModel = CreateObject<TcpSeqErrorModel>();

    for (auto it = m_toDrop.begin(); it != m_toDrop.end(); ++it)
    {
        errorModel->AddSeqToKill(SequenceNumber32(*it));
    }

    return errorModel;
}

/**
 * @ingroup internet-test
 *
 * @brief TCP RTT estimation TestSuite
 */
class TcpRttEstimationTestSuite : public TestSuite
{
  public:
    TcpRttEstimationTestSuite()
        : TestSuite("tcp-rtt-estimation-test", Type::UNIT)
    {
        AddTestCase(new TcpRttEstimationTest("RTT estimation, ts, no data", true, 0),
                    TestCase::Duration::QUICK);
        AddTestCase(new TcpRttEstimationTest("RTT estimation, no ts, no data", false, 0),
                    TestCase::Duration::QUICK);
        AddTestCase(new TcpRttEstimationTest("RTT estimation, ts, some data", true, 10),
                    TestCase::Duration::QUICK);
        AddTestCase(new TcpRttEstimationTest("RTT estimation, no ts, some data", false, 10),
                    TestCase::Duration::QUICK);

        std::vector<uint32_t> toDrop;
        toDrop.push_back(501);

        AddTestCase(new TcpRttEstimationWithLossTest("RTT estimation, no ts,"
                                                     " some data, with retr",
                                                     false,
                                                     10,
                                                     toDrop),
                    TestCase::Duration::QUICK);
        AddTestCase(new TcpRttEstimationWithLossTest("RTT estimation, ts,"
                                                     " some data, with retr",
                                                     true,
                                                     10,
                                                     toDrop),
                    TestCase::Duration::QUICK);

        toDrop.push_back(501);
        AddTestCase(new TcpRttEstimationWithLossTest("RTT estimation, no ts,"
                                                     " some data, with retr",
                                                     false,
                                                     10,
                                                     toDrop),
                    TestCase::Duration::QUICK);
        AddTestCase(new TcpRttEstimationWithLossTest("RTT estimation, ts,"
                                                     " some data, with retr",
                                                     true,
                                                     10,
                                                     toDrop),
                    TestCase::Duration::QUICK);

        toDrop.push_back(54001);
        toDrop.push_back(58001);
        toDrop.push_back(58501);
        toDrop.push_back(60001);
        toDrop.push_back(68501);
        AddTestCase(new TcpRttEstimationWithLossTest("RTT estimation, no ts,"
                                                     " a lot of data, with retr",
                                                     false,
                                                     1000,
                                                     toDrop),
                    TestCase::Duration::QUICK);
        AddTestCase(new TcpRttEstimationWithLossTest("RTT estimation, ts,"
                                                     " a lot of data, with retr",
                                                     true,
                                                     1000,
                                                     toDrop),
                    TestCase::Duration::QUICK);
    }
};

static TcpRttEstimationTestSuite
    g_tcpRttEstimationTestSuite; //!< Static variable for test initialization
