/*
 * Copyright (c) 2011 Centre Tecnologic de Telecomunicacions de Catalunya (CTTC)
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: Marco Miozzo <marco.miozzo@cttc.es>
 * Modification: Dizhi Zhou <dizhi.zhou@gmail.com>    // modify codes related to downlink scheduler
 */

#ifndef TDTBFQ_FF_MAC_SCHEDULER_H
#define TDTBFQ_FF_MAC_SCHEDULER_H

#include "ff-mac-csched-sap.h"
#include "ff-mac-sched-sap.h"
#include "ff-mac-scheduler.h"
#include "lte-amc.h"
#include "lte-common.h"
#include "lte-ffr-sap.h"

#include "ns3/nstime.h"

#include <map>
#include <vector>

namespace ns3
{

/**
 *  Flow information
 */
struct tdtbfqsFlowPerf_t
{
    Time flowStart;               ///< flow start time
    uint64_t packetArrivalRate;   ///< packet arrival rate( byte/s)
    uint64_t tokenGenerationRate; ///< token generation rate ( byte/s )
    uint32_t tokenPoolSize;       ///< current size of token pool (byte)
    uint32_t maxTokenPoolSize;    ///< maximum size of token pool (byte)
    int counter;                  ///< the number of token borrow or given to token bank
    uint32_t burstCredit; ///< the maximum number of tokens connection i can borrow from the bank
                          ///< each time
    int debtLimit; ///< counter threshold that the flow cannot further borrow tokens from bank
    uint32_t creditableThreshold; ///< the flow cannot borrow token from bank until the number of
                                  ///< token it has deposited to bank reaches this threshold
};

/**
 * @ingroup ff-api
 * @brief Implements the SCHED SAP and CSCHED SAP for a Time Domain Token Bank Fair Queue  scheduler
 *
 * This class implements the interface defined by the FfMacScheduler abstract class
 */

class TdTbfqFfMacScheduler : public FfMacScheduler
{
  public:
    /**
     * @brief Constructor
     *
     * Creates the MAC Scheduler interface implementation
     */
    TdTbfqFfMacScheduler();

    /**
     * Destructor
     */
    ~TdTbfqFfMacScheduler() override;

    // inherited from Object
    void DoDispose() override;
    /**
     * @brief Get the type ID.
     * @return the object TypeId
     */
    static TypeId GetTypeId();

    // inherited from FfMacScheduler
    void SetFfMacCschedSapUser(FfMacCschedSapUser* s) override;
    void SetFfMacSchedSapUser(FfMacSchedSapUser* s) override;
    FfMacCschedSapProvider* GetFfMacCschedSapProvider() override;
    FfMacSchedSapProvider* GetFfMacSchedSapProvider() override;

    // FFR SAPs
    void SetLteFfrSapProvider(LteFfrSapProvider* s) override;
    LteFfrSapUser* GetLteFfrSapUser() override;

    /// allow MemberCschedSapProvider<TdTbfqFfMacScheduler> class friend access
    friend class MemberCschedSapProvider<TdTbfqFfMacScheduler>;
    /// allow MemberSchedSapProvider<TdTbfqFfMacScheduler> class friend access
    friend class MemberSchedSapProvider<TdTbfqFfMacScheduler>;

    /**
     * @brief Transmission mde configuration update function
     * @param rnti the RNTI
     * @param txMode the transmission mode
     */
    void TransmissionModeConfigurationUpdate(uint16_t rnti, uint8_t txMode);

  private:
    //
    // Implementation of the CSCHED API primitives
    // (See 4.1 for description of the primitives)
    //

    /**
     * @brief CSched cell config request
     * @param params FfMacCschedSapProvider::CschedCellConfigReqParameters
     */
    void DoCschedCellConfigReq(const FfMacCschedSapProvider::CschedCellConfigReqParameters& params);

    /**
     * @brief CSched UE config request
     * @param params FfMacCschedSapProvider::CschedUeConfigReqParameters
     */
    void DoCschedUeConfigReq(const FfMacCschedSapProvider::CschedUeConfigReqParameters& params);

    /**
     * @brief CSched LC config request
     * @param params FfMacCschedSapProvider::CschedLcConfigReqParameters
     */
    void DoCschedLcConfigReq(const FfMacCschedSapProvider::CschedLcConfigReqParameters& params);

    /**
     * @brief CSched LC release request
     * @param params FfMacCschedSapProvider::CschedLcReleaseReqParameters
     */
    void DoCschedLcReleaseReq(const FfMacCschedSapProvider::CschedLcReleaseReqParameters& params);

    /**
     * @brief CSched UE release request
     * @param params FfMacCschedSapProvider::CschedUeReleaseReqParameters
     */
    void DoCschedUeReleaseReq(const FfMacCschedSapProvider::CschedUeReleaseReqParameters& params);

    //
    // Implementation of the SCHED API primitives
    // (See 4.2 for description of the primitives)
    //

    /**
     * @brief Sched DL RLC buffer request
     * @param params FfMacSchedSapProvider::SchedDlRlcBufferReqParameters
     */
    void DoSchedDlRlcBufferReq(const FfMacSchedSapProvider::SchedDlRlcBufferReqParameters& params);

    /**
     * @brief Sched DL paging buffer request
     * @param params FfMacSchedSapProvider::SchedDlPagingBufferReqParameters
     */
    void DoSchedDlPagingBufferReq(
        const FfMacSchedSapProvider::SchedDlPagingBufferReqParameters& params);

    /**
     * @brief Sched DL MAC buffer request
     * @param params FfMacSchedSapProvider::SchedDlMacBufferReqParameters
     */
    void DoSchedDlMacBufferReq(const FfMacSchedSapProvider::SchedDlMacBufferReqParameters& params);

    /**
     * @brief Sched DL trigger request
     * @param params FfMacSchedSapProvider::SchedDlTriggerReqParameters
     */
    void DoSchedDlTriggerReq(const FfMacSchedSapProvider::SchedDlTriggerReqParameters& params);

    /**
     * @brief Sched DL RACH info request
     * @param params FfMacSchedSapProvider::SchedDlRachInfoReqParameters
     */
    void DoSchedDlRachInfoReq(const FfMacSchedSapProvider::SchedDlRachInfoReqParameters& params);

    /**
     * @brief Sched DL CQI info request
     * @param params FfMacSchedSapProvider::SchedDlCqiInfoReqParameters
     */
    void DoSchedDlCqiInfoReq(const FfMacSchedSapProvider::SchedDlCqiInfoReqParameters& params);

    /**
     * @brief Sched UL trigger request
     * @param params FfMacSchedSapProvider::SchedUlTriggerReqParameters
     */
    void DoSchedUlTriggerReq(const FfMacSchedSapProvider::SchedUlTriggerReqParameters& params);

    /**
     * @brief Sched UL noise interference request
     * @param params FfMacSchedSapProvider::SchedUlNoiseInterferenceReqParameters
     */
    void DoSchedUlNoiseInterferenceReq(
        const FfMacSchedSapProvider::SchedUlNoiseInterferenceReqParameters& params);

    /**
     * @brief Sched UL SR info request
     * @param params FfMacSchedSapProvider::SchedUlSrInfoReqParameters
     */
    void DoSchedUlSrInfoReq(const FfMacSchedSapProvider::SchedUlSrInfoReqParameters& params);

    /**
     * @brief Sched UL MAC control info request
     * @param params FfMacSchedSapProvider::SchedUlMacCtrlInfoReqParameters
     */
    void DoSchedUlMacCtrlInfoReq(
        const FfMacSchedSapProvider::SchedUlMacCtrlInfoReqParameters& params);

    /**
     * @brief Sched UL CQI info request
     * @param params FfMacSchedSapProvider::SchedUlCqiInfoReqParameters
     */
    void DoSchedUlCqiInfoReq(const FfMacSchedSapProvider::SchedUlCqiInfoReqParameters& params);

    /**
     * @brief Get RBG size
     * @param dlbandwidth he DL bandwidth
     * @returns the RBG size
     */
    int GetRbgSize(int dlbandwidth);

    /**
     * @brief LC active flow size
     * @param rnti the RNTI
     * @returns the LC active flow
     */
    unsigned int LcActivePerFlow(uint16_t rnti);

    /**
     * @brief Estimate UL SINR function
     * @param rnti the RNTI
     * @param rb the RB
     * @returns the SINR
     */
    double EstimateUlSinr(uint16_t rnti, uint16_t rb);

    /// Refresh DL CQI maps function
    void RefreshDlCqiMaps();
    /// Refresh UL CQI maps function
    void RefreshUlCqiMaps();

    /**
     * @brief Update DL RLC buffer info function
     * @param rnti the RNTI
     * @param lcid the LCID
     * @param size the size
     */
    void UpdateDlRlcBufferInfo(uint16_t rnti, uint8_t lcid, uint16_t size);
    /**
     * @brief Update UL RLC buffer info function
     * @param rnti the RNTI
     * @param size the size
     */
    void UpdateUlRlcBufferInfo(uint16_t rnti, uint16_t size);

    /**
     * @brief Update and return a new process Id for the RNTI specified
     *
     * @param rnti the RNTI of the UE to be updated
     * @return the process id  value
     */
    uint8_t UpdateHarqProcessId(uint16_t rnti);

    /**
     * @brief Return the availability of free process for the RNTI specified
     *
     * @param rnti the RNTI of the UE to be updated
     * @return the availability
     */
    bool HarqProcessAvailability(uint16_t rnti);

    /**
     * @brief Refresh HARQ processes according to the timers
     *
     */
    void RefreshHarqProcesses();

    Ptr<LteAmc> m_amc; ///< AMC

    /**
     * Vectors of UE's LC info
     */
    std::map<LteFlowId_t, FfMacSchedSapProvider::SchedDlRlcBufferReqParameters> m_rlcBufferReq;

    /**
     * Map of UE statistics (per RNTI basis) in downlink
     */
    std::map<uint16_t, tdtbfqsFlowPerf_t> m_flowStatsDl;

    /**
     * Map of UE statistics (per RNTI basis)
     */
    std::map<uint16_t, tdtbfqsFlowPerf_t> m_flowStatsUl;

    /**
     * Map of UE's DL CQI P01 received
     */
    std::map<uint16_t, uint8_t> m_p10CqiRxed;
    /**
     * Map of UE's timers on DL CQI P01 received
     */
    std::map<uint16_t, uint32_t> m_p10CqiTimers;

    /**
     * Map of UE's DL CQI A30 received
     */
    std::map<uint16_t, SbMeasResult_s> m_a30CqiRxed;
    /**
     * Map of UE's timers on DL CQI A30 received
     */
    std::map<uint16_t, uint32_t> m_a30CqiTimers;

    /**
     * Map of previous allocated UE per RBG
     * (used to retrieve info from UL-CQI)
     */
    std::map<uint16_t, std::vector<uint16_t>> m_allocationMaps;

    /**
     * Map of UEs' UL-CQI per RBG
     */
    std::map<uint16_t, std::vector<double>> m_ueCqi;
    /**
     * Map of UEs' timers on UL-CQI per RBG
     */
    std::map<uint16_t, uint32_t> m_ueCqiTimers;

    /**
     * Map of UE's buffer status reports received
     */
    std::map<uint16_t, uint32_t> m_ceBsrRxed;

    // MAC SAPs
    FfMacCschedSapUser* m_cschedSapUser;         ///< CSched SAP user
    FfMacSchedSapUser* m_schedSapUser;           ///< A=Sched SAP user
    FfMacCschedSapProvider* m_cschedSapProvider; ///< CSched SAP provider
    FfMacSchedSapProvider* m_schedSapProvider;   ///< Sched SAP provider

    // FFR SAPs
    LteFfrSapUser* m_ffrSapUser;         ///< FFR SAP user
    LteFfrSapProvider* m_ffrSapProvider; ///< FFR SAP provider

    // Internal parameters
    FfMacCschedSapProvider::CschedCellConfigReqParameters
        m_cschedCellConfig; ///< CSched cell config

    uint16_t m_nextRntiUl; ///< RNTI of the next user to be served next scheduling in UL

    uint32_t m_cqiTimersThreshold; ///< # of TTIs for which a CQI can be considered valid

    std::map<uint16_t, uint8_t> m_uesTxMode; ///< txMode of the UEs

    uint64_t bankSize; ///< the number of bytes in token bank

    int m_debtLimit; ///< flow debt limit (byte)

    uint32_t m_creditLimit; ///< flow credit limit (byte)

    uint32_t m_tokenPoolSize; ///< maximum size of token pool (byte)

    uint32_t m_creditableThreshold; ///< threshold of flow credit

    // HARQ attributes
    /**
     * m_harqOn when false inhibit the HARQ mechanisms (by default active)
     */
    bool m_harqOn;
    std::map<uint16_t, uint8_t> m_dlHarqCurrentProcessId; ///< DL HARQ current process ID
    // HARQ status
    //  0: process Id available
    //  x>0: process Id equal to `x` transmission count
    std::map<uint16_t, DlHarqProcessesStatus_t> m_dlHarqProcessesStatus; ///< DL HARQ process status
    std::map<uint16_t, DlHarqProcessesTimer_t> m_dlHarqProcessesTimer;   ///< DL HARQ process timer
    std::map<uint16_t, DlHarqProcessesDciBuffer_t>
        m_dlHarqProcessesDciBuffer; ///< DL HARQ process DCI buffer
    std::map<uint16_t, DlHarqRlcPduListBuffer_t>
        m_dlHarqProcessesRlcPduListBuffer;                 ///< DL HARQ process RLC PDU list buffer
    std::vector<DlInfoListElement_s> m_dlInfoListBuffered; ///< HARQ retx buffered

    std::map<uint16_t, uint8_t> m_ulHarqCurrentProcessId; ///< UL HARQ current process ID
    // HARQ status
    //  0: process Id available
    //  x>0: process Id equal to `x` transmission count
    std::map<uint16_t, UlHarqProcessesStatus_t> m_ulHarqProcessesStatus; ///< UL HARQ process status
    std::map<uint16_t, UlHarqProcessesDciBuffer_t>
        m_ulHarqProcessesDciBuffer; ///< UL HARQ process DCI buffer

    // RACH attributes
    std::vector<RachListElement_s> m_rachList; ///< RACH list
    std::vector<uint16_t> m_rachAllocationMap; ///< RACH allocation map
    uint8_t m_ulGrantMcs;                      ///< MCS for UL grant (default 0)
};

} // namespace ns3

#endif /* TDTBFQ_FF_MAC_SCHEDULER_H */
