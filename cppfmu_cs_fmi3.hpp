/* Copyright 2016-2026, SINTEF Ocean.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
#ifndef CPPFMU_CS_FMI3_HPP
#define CPPFMU_CS_FMI3_HPP

#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include "cppfmu_common.hpp"

namespace cppfmu
{

/* ============================================================================
 * FMI 3.0 CO-SIMULATION INTERFACE
 * ============================================================================
 */

/* A base class for FMI 3.0 co-simulation slave instances.
 *
 * To implement a co-simulation slave, create a class which publicly derives
 * from this one and override its virtual methods as required.  DoStep() is
 * the only function which it is mandatory to override.
 *
 * The methods map directly to the C functions defined by FMI 3.0 for
 * Co-Simulation, so the documentation here is intentionally sparse.
 * We refer to the FMI 3.0 specification for detailed information.
 */
class SlaveInstance3
{
public:
    /* Called from fmi3EnterInitializationMode().
     * Does nothing by default.
     */
    virtual void EnterInitializationMode(
        FMIBoolean toleranceDefined,
        FMIReal tolerance,
        FMIReal startTime,
        FMIBoolean stopTimeDefined,
        FMIReal stopTime);

    /* Called from fmi3ExitInitializationMode().
     * Does nothing by default.
     */
    virtual void ExitInitializationMode();

    /* Called from fmi3Terminate().
     * Does nothing by default.
     */
    virtual void Terminate();

    /* Called from fmi3Reset().
     * Does nothing by default.
     */
    virtual void Reset();

    /* Called from fmi3SetFloat32().
     * Throws std::logic_error by default.
     */
    virtual void SetFloat32(
        const FMIValueReference vr[],
        std::size_t nvr,
        const FMIFloat32 value[],
        std::size_t nValues);

    /* Called from fmi3SetFloat64().
     * Throws std::logic_error by default.
     */
    virtual void SetFloat64(
        const FMIValueReference vr[],
        std::size_t nvr,
        const FMIReal value[],
        std::size_t nValues);

    /* Called from fmi3SetInt8().
     * Throws std::logic_error by default.
     */
    virtual void SetInt8(
        const FMIValueReference vr[],
        std::size_t nvr,
        const FMIInt8 value[],
        std::size_t nValues);

    /* Called from fmi3SetUInt8().
     * Throws std::logic_error by default.
     */
    virtual void SetUInt8(
        const FMIValueReference vr[],
        std::size_t nvr,
        const FMIUInt8 value[],
        std::size_t nValues);

    /* Called from fmi3SetInt16().
     * Throws std::logic_error by default.
     */
    virtual void SetInt16(
        const FMIValueReference vr[],
        std::size_t nvr,
        const FMIInt16 value[],
        std::size_t nValues);

    /* Called from fmi3SetUInt16().
     * Throws std::logic_error by default.
     */
    virtual void SetUInt16(
        const FMIValueReference vr[],
        std::size_t nvr,
        const FMIUInt16 value[],
        std::size_t nValues);

    /* Called from fmi3SetInt32().
     * Throws std::logic_error by default.
     */
    virtual void SetInt32(
        const FMIValueReference vr[],
        std::size_t nvr,
        const FMIInteger value[],
        std::size_t nValues);

    /* Called from fmi3SetUInt32().
     * Throws std::logic_error by default.
     */
    virtual void SetUInt32(
        const FMIValueReference vr[],
        std::size_t nvr,
        const FMIUInt32 value[],
        std::size_t nValues);

    /* Called from fmi3SetInt64().
     * Throws std::logic_error by default.
     */
    virtual void SetInt64(
        const FMIValueReference vr[],
        std::size_t nvr,
        const FMIInt64 value[],
        std::size_t nValues);

    /* Called from fmi3SetUInt64().
     * Throws std::logic_error by default.
     */
    virtual void SetUInt64(
        const FMIValueReference vr[],
        std::size_t nvr,
        const FMIUInt64 value[],
        std::size_t nValues);

    /* Called from fmi3SetBoolean().
     * Throws std::logic_error by default.
     */
    virtual void SetBoolean(
        const FMIValueReference vr[],
        std::size_t nvr,
        const FMIBoolean value[],
        std::size_t nValues);

    /* Called from fmi3SetString().
     * Throws std::logic_error by default.
     */
    virtual void SetString(
        const FMIValueReference vr[],
        std::size_t nvr,
        const FMIString value[],
        std::size_t nValues);

    /* Called from fmi3SetBinary().
     * Throws std::logic_error by default.
     */
    virtual void SetBinary(
        const FMIValueReference vr[],
        std::size_t nvr,
        const std::size_t sizes[],
        const FMIBinary value[],
        std::size_t nValues);

    /* Called from fmi3SetClock().
     * Throws std::logic_error by default.
     */
    virtual void SetClock(
        const FMIValueReference vr[],
        std::size_t nvr,
        const FMIClock value[]);

    /* Called from fmi3GetFloat32().
     * Throws std::logic_error by default.
     */
    virtual void GetFloat32(
        const FMIValueReference vr[],
        std::size_t nvr,
        FMIFloat32 value[],
        std::size_t nValues) const;

    /* Called from fmi3GetFloat64().
     * Throws std::logic_error by default.
     */
    virtual void GetFloat64(
        const FMIValueReference vr[],
        std::size_t nvr,
        FMIReal value[],
        std::size_t nValues) const;

    /* Called from fmi3GetInt8().
     * Throws std::logic_error by default.
     */
    virtual void GetInt8(
        const FMIValueReference vr[],
        std::size_t nvr,
        FMIInt8 value[],
        std::size_t nValues) const;

    /* Called from fmi3GetUInt8().
     * Throws std::logic_error by default.
     */
    virtual void GetUInt8(
        const FMIValueReference vr[],
        std::size_t nvr,
        FMIUInt8 value[],
        std::size_t nValues) const;

    /* Called from fmi3GetInt16().
     * Throws std::logic_error by default.
     */
    virtual void GetInt16(
        const FMIValueReference vr[],
        std::size_t nvr,
        FMIInt16 value[],
        std::size_t nValues) const;

    /* Called from fmi3GetUInt16().
     * Throws std::logic_error by default.
     */
    virtual void GetUInt16(
        const FMIValueReference vr[],
        std::size_t nvr,
        FMIUInt16 value[],
        std::size_t nValues) const;

    /* Called from fmi3GetInt32().
     * Throws std::logic_error by default.
     */
    virtual void GetInt32(
        const FMIValueReference vr[],
        std::size_t nvr,
        FMIInteger value[],
        std::size_t nValues) const;

    /* Called from fmi3GetUInt32().
     * Throws std::logic_error by default.
     */
    virtual void GetUInt32(
        const FMIValueReference vr[],
        std::size_t nvr,
        FMIUInt32 value[],
        std::size_t nValues) const;

    /* Called from fmi3GetInt64().
     * Throws std::logic_error by default.
     */
    virtual void GetInt64(
        const FMIValueReference vr[],
        std::size_t nvr,
        FMIInt64 value[],
        std::size_t nValues) const;

    /* Called from fmi3GetUInt64().
     * Throws std::logic_error by default.
     */
    virtual void GetUInt64(
        const FMIValueReference vr[],
        std::size_t nvr,
        FMIUInt64 value[],
        std::size_t nValues) const;

    /* Called from fmi3GetBoolean().
     * Throws std::logic_error by default.
     */
    virtual void GetBoolean(
        const FMIValueReference vr[],
        std::size_t nvr,
        FMIBoolean value[],
        std::size_t nValues) const;

    /* Called from fmi3GetString().
     * Throws std::logic_error by default.
     */
    virtual void GetString(
        const FMIValueReference vr[],
        std::size_t nvr,
        FMIString value[],
        std::size_t nValues) const;

    /* Called from fmi3GetBinary().
     * Throws std::logic_error by default.
     */
    virtual void GetBinary(
        const FMIValueReference vr[],
        std::size_t nvr,
        std::size_t sizes[],
        FMIBinary value[],
        std::size_t nValues) const;

    /* Called from fmi3GetClock().
     * Throws std::logic_error by default.
     */
    virtual void GetClock(
        const FMIValueReference vr[],
        std::size_t nvr,
        FMIClock value[]) const;

    /* Called from fmi3GetFMUState().
     * Throws std::logic_error by default.
     */
    virtual void GetFMUState(FMIFMUState* state);

    /* Called from fmi3SetFMUState().
     * Throws std::logic_error by default.
     */
    virtual void SetFMUState(FMIFMUState state);

    /* Called from fmi3FreeFMUState().
     * Throws std::logic_error by default.
     */
    virtual void FreeFMUState(FMIFMUState state);

    /* Called from fmi3SerializedFMUStateSize().
     * Throws std::logic_error by default.
     */
    virtual std::size_t SerializedFMUStateSize(FMIFMUState state);

    /* Called from fmi3SerializeFMUState().
     * Throws std::logic_error by default.
     */
    virtual void SerializeFMUState(
        FMIFMUState state,
        FMIByte data[],
        std::size_t size);

    /* Called from fmi3DeserializeFMUState().
     * Throws std::logic_error by default.
     */
    virtual FMIFMUState DeserializeFMUState(
        const FMIByte data[],
        std::size_t size);

    /* Called from fmi3GetDirectionalDerivative().
     * Throws std::logic_error by default.
     */
    virtual void GetDirectionalDerivative(
        const FMIValueReference unknowns[],
        std::size_t nUnknowns,
        const FMIValueReference knowns[],
        std::size_t nKnowns,
        const FMIReal seed[],
        std::size_t nSeed,
        FMIReal sensitivity[],
        std::size_t nSensitivity) const;

    /* Called from fmi3GetAdjointDerivative().
     * Throws std::logic_error by default.
     */
    virtual void GetAdjointDerivative(
        const FMIValueReference unknowns[],
        std::size_t nUnknowns,
        const FMIValueReference knowns[],
        std::size_t nKnowns,
        const FMIReal seed[],
        std::size_t nSeed,
        FMIReal sensitivity[],
        std::size_t nSensitivity) const;

    /* Called from fmi3GetVariableDependencies().
     * Throws std::logic_error by default.
     */
    virtual void GetVariableDependencies(
        FMIValueReference dependent,
        const std::size_t elementIndicesOfDependent[],
        FMIValueReference independents[],
        std::size_t elementIndicesOfIndependents[],
        FMIDependencyKind dependencyKinds[],
        std::size_t nDependencies) const;

    /* Called from fmi3GetOutputDerivatives().
     * Throws std::logic_error by default.
     */
    virtual void GetOutputDerivatives(
        const FMIValueReference vr[],
        std::size_t nvr,
        const FMIInteger orders[],
        FMIReal values[],
        std::size_t nValues) const;

    /* Called from fmi3GetNumberOfVariableDependencies().
     * Throws std::logic_error by default.
     */
    virtual std::size_t GetNumberOfVariableDependencies(
        FMIValueReference valueReference) const;

    /* Called from fmi3GetNumberOfEventIndicators().
     * Returns 0 by default.
     */
    virtual std::size_t GetNumberOfEventIndicators() const;

    /* Called from fmi3GetNumberOfContinuousStates().
     * Returns 0 by default.
     */
    virtual std::size_t GetNumberOfContinuousStates() const;

    // Called from fmi3DoStep(). Must be implemented in model code.
    // Returns true for OK, false for discard (early return).
    // Output parameters are set by the implementation.
    virtual bool DoStep(
        FMIReal currentCommunicationPoint,
        FMIReal communicationStepSize,
        FMIBoolean noSetFMUStatePriorToCurrentPoint,
        FMIBoolean& eventHandlingNeeded,
        FMIBoolean& terminateSimulation,
        FMIBoolean& earlyReturn,
        FMIReal& lastSuccessfulTime) = 0;

    // The instance is destroyed in fmi3FreeInstance().
    virtual ~SlaveInstance3() CPPFMU_NOEXCEPT;
};

} // namespace cppfmu


/* A function which must be defined by model code, and which should create
 * and return a new slave instance.
 *
 * The returned instance must be managed by a std::unique_ptr with a deleter
 * of type std::function<void(void*)> that takes care of freeing the memory.
 * The simplest way to set this up is to use cppfmu::AllocateUnique3() to
 * create the slave instance.
 *
 * Most of its parameters correspond to those of fmi3InstantiateCoSimulation,
 * except that the callback functions have been replaced with more convenient
 * types:
 *
 *     logger = A std::function which the model code can use to log messages.
 *              The messages are forwarded to the simulation environment's
 *              logging facilities.
 *
 * Note that this function is declared in the global namespace.
 */
cppfmu::UniquePtr<cppfmu::SlaveInstance3> CppfmuInstantiateSlave(
    cppfmu::FMIString instanceName,
    cppfmu::FMIString instantiationToken,
    cppfmu::FMIString resourceLocation,
    cppfmu::FMIBoolean visible,
    cppfmu::FMIBoolean loggingOn,
    cppfmu::FMIBoolean eventModeUsed,
    cppfmu::FMIBoolean earlyReturnAllowed,
    const cppfmu::FMIValueReference requiredIntermediateVariables[],
    std::size_t nRequiredIntermediateVariables,
    cppfmu::FMIComponentEnvironment instanceEnvironment,
    std::function<void(cppfmu::FMIStatus, cppfmu::FMIString, cppfmu::FMIString)> logger);


namespace cppfmu
{

/* Creates an object of type T which is managed by a std::unique_ptr.
 * For FMI 3.0, memory is managed by standard C++ (no FMI memory callbacks).
 */
template<typename T, typename... Args>
UniquePtr<T> AllocateUnique3(Args&&... args)
{
    return UniquePtr<T>{
        new T(std::forward<Args>(args)...),
        [] (void* ptr) { delete reinterpret_cast<T*>(ptr); }};
}

} // namespace cppfmu


#endif // header guard
