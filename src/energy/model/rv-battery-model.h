/*
 * Copyright (c) 2010 Network Security Lab, University of Washington, Seattle.
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Authors: Sidharth Nabar <snabar@uw.edu>, He Wu <mdzz@u.washington.edu>
 */

#ifndef RV_BATTERY_MODEL_H
#define RV_BATTERY_MODEL_H

#include "energy-source.h"

#include "ns3/event-id.h"
#include "ns3/nstime.h"
#include "ns3/traced-value.h"

namespace ns3
{
namespace energy
{

/**
 * @ingroup energy
 * @brief Rakhmatov Vrudhula non-linear battery model.
 *
 * This (energy source) model implements an analytical non-linear battery model.
 * It is capable of capturing load capacity and recovery effects of batteries.
 * Batteries are characterized by 2 parameters, alpha and beta, which can both
 * be obtained from the discharge curve of the batteries.
 *
 * The model is developed by Daler Rakhmatov & Sarma Vrudhula in: "Battery
 * Lifetime Prediction for Energy-Aware Computing" and "An Analytical High-Level
 * Battery Model for Use in Energy Management of Portable Electronic Systems".
 *
 * The real-time algorithm is developed by Matthias Handy & Dirk Timmermann in:
 * "Simulation of Mobile Wireless Networks with Accurate Modeling of non-linear
 * battery effects". The real-time algorithm is modified by the authors of this
 * code for improved accuracy and reduced computation (sampling) overhead.
 *
 */
class RvBatteryModel : public EnergySource
{
  public:
    /**
     * @brief Get the type ID.
     * @return The object TypeId.
     */
    static TypeId GetTypeId();
    RvBatteryModel();
    ~RvBatteryModel() override;

    /**
     * @return Initial energy stored (theoretical capacity) in the battery, in Joules.
     *
     * Implements GetInitialEnergy.
     */
    double GetInitialEnergy() const override;

    /**
     * @returns Supply voltage at the energy source.
     *
     * Implements GetSupplyVoltage.
     */
    double GetSupplyVoltage() const override;

    /**
     * @return Remaining energy in energy source, in Joules
     *
     * Implements GetRemainingEnergy.
     */
    double GetRemainingEnergy() override;

    /**
     * @returns Energy fraction.
     *
     * Implements GetEnergyFraction. For the RV battery model, energy fraction is
     * equivalent to battery level.
     */
    double GetEnergyFraction() override;

    /**
     * Implements UpdateEnergySource. This function samples the total load (total
     * current) from all devices to discharge the battery.
     */
    void UpdateEnergySource() override;

    /**
     * @param interval Energy update interval.
     *
     * This function sets the interval between each energy update.
     */
    void SetSamplingInterval(Time interval);

    /**
     * @returns The interval between each energy update.
     */
    Time GetSamplingInterval() const;

    /**
     * @brief Sets open circuit voltage of battery.
     *
     * @param voltage Open circuit voltage.
     */
    void SetOpenCircuitVoltage(double voltage);

    /**
     * @return Open circuit voltage of battery.
     */
    double GetOpenCircuitVoltage() const;

    /**
     * @brief Sets cutoff voltage of battery.
     *
     * @param voltage Cutoff voltage.
     */
    void SetCutoffVoltage(double voltage);

    /**
     * @returns Cutoff voltage of battery.
     */
    double GetCutoffVoltage() const;

    /**
     * @brief Sets the alpha value for the battery model.
     *
     * @param alpha Alpha.
     */
    void SetAlpha(double alpha);

    /**
     * @returns The alpha value used by the battery model.
     */
    double GetAlpha() const;

    /**
     * @brief Sets the beta value for the battery model.
     *
     * @param beta Beta.
     */
    void SetBeta(double beta);

    /**
     * @returns The beta value used by the battery model.
     */
    double GetBeta() const;

    /**
     * @returns Battery level [0, 1].
     */
    double GetBatteryLevel();

    /**
     * @returns Lifetime of the battery.
     */
    Time GetLifetime() const;

    /**
     * @brief Sets the number of terms of the infinite sum for estimating battery
     * level.
     *
     * @param num Number of terms.
     */
    void SetNumOfTerms(int num);

    /**
     * @returns The number of terms of the infinite sum for estimating battery
     * level.
     */
    int GetNumOfTerms() const;

  private:
    /// Defined in ns3::Object
    void DoInitialize() override;

    /// Defined in ns3::Object
    void DoDispose() override;

    /**
     * Handles the remaining energy going to zero event. This function notifies
     * all the energy models aggregated to the node about the energy being
     * depleted. Each energy model is then responsible for its own handler.
     */
    void HandleEnergyDrainedEvent();

    /**
     * @brief Discharges the battery.
     *
     * @param load Load value (total current form devices, in mA).
     * @param t Time stamp of the load value.
     * @returns Calculated alpha value.
     *
     * Discharge function calculates a value which is then compared to the alpha
     * value to determine if the battery is dead. It will also update the battery
     * level.
     *
     * Note that the load value passed to Discharge has to be in mA.
     */
    double Discharge(double load, Time t);

    /**
     * @brief RV model A function.
     *
     * @param t Current time.
     * @param sk Time stamp in array position k
     * @param sk_1 Time stamp in array position k-1
     * @param beta Beta value used by the battery model.
     * @returns Result of A function.
     *
     * This function computes alpha value using the recorded load profile.
     */
    double RvModelAFunction(Time t, Time sk, Time sk_1, double beta);

  private:
    double m_openCircuitVoltage; //!< Open circuit voltage (in Volts)
    double m_cutoffVoltage;      //!< Cutoff voltage (in Volts)
    double m_alpha;              //!< alpha value of RV model, in Coulomb
    double m_beta;               //!< beta value of RV model, in second^-1

    double m_previousLoad;          //!< load value (total current) of previous sampling
    std::vector<double> m_load;     //!< load profile
    std::vector<Time> m_timeStamps; //!< time stamps of load profile
    Time m_lastSampleTime;          //!< Last sample time

    int m_numOfTerms; //!< Number# of terms for infinite sum in battery level estimation

    /**
     * Battery level is defined as: output of Discharge function / alpha value
     *
     * The output of Discharge function is an estimated charge consumption of the
     * battery.
     *
     * The alpha value is the amount of charges stored in the battery, or battery
     * capacity (in Coulomb).
     *
     * When the battery is fully charged (no charge is consumed from the battery)
     * the battery level is 1. When the battery is fully discharged, the battery
     * level is 0.
     *
     * NOTE Note that the definition in Timmermann's paper is the inverse of this
     * definition. In the paper, battery level = 1 when the battery is drained.
     */
    TracedValue<double> m_batteryLevel;

    double m_lowBatteryTh; //!< low battery threshold, as a fraction of the initial energy

    /**
     * Sampling interval.
     * (1 / sampling interval) = sampling frequency
     */
    Time m_samplingInterval;
    EventId m_currentSampleEvent; //!< Current sample event

    TracedValue<Time> m_lifetime; //!< time of death of the battery
};

} // namespace energy
} // namespace ns3

#endif /* RV_BATTERY_MODEL_H */
