/*
 * Copyright (c) 2011 Centre Tecnologic de Telecomunicacions de Catalunya (CTTC)
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: Giuseppe Piro  <g.piro@poliba.it>
 *         Marco Miozzo <mmiozzo@cttc.es>
 */

#ifndef TRACE_FADING_LOSS_MODEL_H
#define TRACE_FADING_LOSS_MODEL_H

#include "spectrum-propagation-loss-model.h"

#include "ns3/nstime.h"
#include "ns3/object.h"
#include "ns3/random-variable-stream.h"

#include <map>

namespace ns3
{

class MobilityModel;

/**
 * @ingroup spectrum
 *
 * @brief fading loss model based on precalculated fading traces
 */
class TraceFadingLossModel : public SpectrumPropagationLossModel
{
  public:
    TraceFadingLossModel();
    ~TraceFadingLossModel() override;

    /**
     * @brief Get the type ID.
     * @return the object TypeId
     */
    static TypeId GetTypeId();

    void DoInitialize() override;

    /**
     * @brief The couple of mobility node that form a fading channel realization
     */
    typedef std::pair<Ptr<const MobilityModel>, Ptr<const MobilityModel>> ChannelRealizationId_t;

  protected:
    int64_t DoAssignStreams(int64_t stream) override;

  private:
    /**
     * @param params the spectrum signal parameters.
     * @param a sender mobility
     * @param b receiver mobility
     * @return set of values vs frequency representing the received
     *         power in the same units used for the txPsd parameter.
     */
    Ptr<SpectrumValue> DoCalcRxPowerSpectralDensity(Ptr<const SpectrumSignalParameters> params,
                                                    Ptr<const MobilityModel> a,
                                                    Ptr<const MobilityModel> b) const override;

    /**
     * @brief Get the value for a particular sub channel and a given speed
     * @param subChannel the sub channel for which a value is requested
     * @param speed the relative speed of the two devices
     * @return the loss for a particular sub channel
     */
    double GetValue(int subChannel, double speed);

    /**
     * @brief Set the trace file name
     * @param fileName the trace file
     */
    void SetTraceFileName(std::string fileName);
    /**
     * @brief Set the trace time
     * @param t the trace time
     */
    void SetTraceLength(Time t);

    /// Load trace function
    void LoadTrace();

    mutable std::map<ChannelRealizationId_t, int> m_windowOffsetsMap; ///< windows offsets map

    mutable std::map<ChannelRealizationId_t, Ptr<UniformRandomVariable>>
        m_startVariableMap; ///< start variable map

    /**
     * Vector with fading samples in time domain (for a fixed RB)
     */
    typedef std::vector<double> FadingTraceSample;
    /**
     * Vector collecting the time fading traces in the frequency domain (per RB)
     */
    typedef std::vector<FadingTraceSample> FadingTrace;

    std::string m_traceFile; ///< the trace file name

    FadingTrace m_fadingTrace; ///< fading trace

    Time m_traceLength;               ///< the trace time
    uint32_t m_samplesNum;            ///< number of samples
    Time m_windowSize;                ///< window size
    uint32_t m_rbNum;                 ///< RB number
    mutable Time m_lastWindowUpdate;  ///< time of last window update
    uint32_t m_timeGranularity;       ///< time granularity
    mutable uint64_t m_currentStream; ///< the current stream
    mutable uint64_t m_lastStream;    ///< the last stream
    uint64_t m_streamSetSize;         ///< stream set size
    mutable bool m_streamsAssigned;   ///< is streams assigned?
};

} // namespace ns3

#endif /* TRACE_FADING_LOSS_MODEL_H */
