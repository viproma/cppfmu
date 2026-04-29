/* Copyright 2016-2026, SINTEF Ocean.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
#include "cppfmu_cs_fmi3.hpp"

#include <stdexcept>


namespace cppfmu
{

// =============================================================================
// SlaveInstance3
// =============================================================================


void SlaveInstance3::EnterInitializationMode(
    FMIBoolean /*toleranceDefined*/,
    FMIReal /*tolerance*/,
    FMIReal /*startTime*/,
    FMIBoolean /*stopTimeDefined*/,
    FMIReal /*stopTime*/)
{
    // Do nothing
}


void SlaveInstance3::ExitInitializationMode()
{
    // Do nothing
}


void SlaveInstance3::Terminate()
{
    // Do nothing
}


void SlaveInstance3::Reset()
{
    // Do nothing
}


void SlaveInstance3::SetFloat32(
    const FMIValueReference /*vr*/[],
    std::size_t nvr,
    const FMIFloat32 /*value*/[],
    std::size_t /*nValues*/)
{
    if (nvr != 0) {
        throw std::logic_error("Attempted to set nonexistent variable");
    }
}


void SlaveInstance3::SetFloat64(
    const FMIValueReference /*vr*/[],
    std::size_t nvr,
    const FMIReal /*value*/[],
    std::size_t /*nValues*/)
{
    if (nvr != 0) {
        throw std::logic_error("Attempted to set nonexistent variable");
    }
}


void SlaveInstance3::SetInt8(
    const FMIValueReference /*vr*/[],
    std::size_t nvr,
    const FMIInt8 /*value*/[],
    std::size_t /*nValues*/)
{
    if (nvr != 0) {
        throw std::logic_error("Attempted to set nonexistent variable");
    }
}


void SlaveInstance3::SetUInt8(
    const FMIValueReference /*vr*/[],
    std::size_t nvr,
    const FMIUInt8 /*value*/[],
    std::size_t /*nValues*/)
{
    if (nvr != 0) {
        throw std::logic_error("Attempted to set nonexistent variable");
    }
}


void SlaveInstance3::SetInt16(
    const FMIValueReference /*vr*/[],
    std::size_t nvr,
    const FMIInt16 /*value*/[],
    std::size_t /*nValues*/)
{
    if (nvr != 0) {
        throw std::logic_error("Attempted to set nonexistent variable");
    }
}


void SlaveInstance3::SetUInt16(
    const FMIValueReference /*vr*/[],
    std::size_t nvr,
    const FMIUInt16 /*value*/[],
    std::size_t /*nValues*/)
{
    if (nvr != 0) {
        throw std::logic_error("Attempted to set nonexistent variable");
    }
}


void SlaveInstance3::SetInt32(
    const FMIValueReference /*vr*/[],
    std::size_t nvr,
    const FMIInteger /*value*/[],
    std::size_t /*nValues*/)
{
    if (nvr != 0) {
        throw std::logic_error("Attempted to set nonexistent variable");
    }
}


void SlaveInstance3::SetUInt32(
    const FMIValueReference /*vr*/[],
    std::size_t nvr,
    const FMIUInt32 /*value*/[],
    std::size_t /*nValues*/)
{
    if (nvr != 0) {
        throw std::logic_error("Attempted to set nonexistent variable");
    }
}


void SlaveInstance3::SetInt64(
    const FMIValueReference /*vr*/[],
    std::size_t nvr,
    const FMIInt64 /*value*/[],
    std::size_t /*nValues*/)
{
    if (nvr != 0) {
        throw std::logic_error("Attempted to set nonexistent variable");
    }
}


void SlaveInstance3::SetUInt64(
    const FMIValueReference /*vr*/[],
    std::size_t nvr,
    const FMIUInt64 /*value*/[],
    std::size_t /*nValues*/)
{
    if (nvr != 0) {
        throw std::logic_error("Attempted to set nonexistent variable");
    }
}


void SlaveInstance3::SetBoolean(
    const FMIValueReference /*vr*/[],
    std::size_t nvr,
    const FMIBoolean /*value*/[],
    std::size_t /*nValues*/)
{
    if (nvr != 0) {
        throw std::logic_error("Attempted to set nonexistent variable");
    }
}


void SlaveInstance3::SetString(
    const FMIValueReference /*vr*/[],
    std::size_t nvr,
    const FMIString /*value*/[],
    std::size_t /*nValues*/)
{
    if (nvr != 0) {
        throw std::logic_error("Attempted to set nonexistent variable");
    }
}


void SlaveInstance3::SetBinary(
    const FMIValueReference /*vr*/[],
    std::size_t nvr,
    const std::size_t /*sizes*/[],
    const FMIBinary /*value*/[],
    std::size_t /*nValues*/)
{
    if (nvr != 0) {
        throw std::logic_error("Attempted to set nonexistent variable");
    }
}


void SlaveInstance3::SetClock(
    const FMIValueReference /*vr*/[],
    std::size_t nvr,
    const FMIClock /*value*/[])
{
    if (nvr != 0) {
        throw std::logic_error("Attempted to set nonexistent variable");
    }
}


void SlaveInstance3::GetFloat32(
    const FMIValueReference /*vr*/[],
    std::size_t nvr,
    FMIFloat32 /*value*/[],
    std::size_t /*nValues*/) const
{
    if (nvr != 0) {
        throw std::logic_error("Attempted to get nonexistent variable");
    }
}


void SlaveInstance3::GetFloat64(
    const FMIValueReference /*vr*/[],
    std::size_t nvr,
    FMIReal /*value*/[],
    std::size_t /*nValues*/) const
{
    if (nvr != 0) {
        throw std::logic_error("Attempted to get nonexistent variable");
    }
}


void SlaveInstance3::GetInt8(
    const FMIValueReference /*vr*/[],
    std::size_t nvr,
    FMIInt8 /*value*/[],
    std::size_t /*nValues*/) const
{
    if (nvr != 0) {
        throw std::logic_error("Attempted to get nonexistent variable");
    }
}


void SlaveInstance3::GetUInt8(
    const FMIValueReference /*vr*/[],
    std::size_t nvr,
    FMIUInt8 /*value*/[],
    std::size_t /*nValues*/) const
{
    if (nvr != 0) {
        throw std::logic_error("Attempted to get nonexistent variable");
    }
}


void SlaveInstance3::GetInt16(
    const FMIValueReference /*vr*/[],
    std::size_t nvr,
    FMIInt16 /*value*/[],
    std::size_t /*nValues*/) const
{
    if (nvr != 0) {
        throw std::logic_error("Attempted to get nonexistent variable");
    }
}


void SlaveInstance3::GetUInt16(
    const FMIValueReference /*vr*/[],
    std::size_t nvr,
    FMIUInt16 /*value*/[],
    std::size_t /*nValues*/) const
{
    if (nvr != 0) {
        throw std::logic_error("Attempted to get nonexistent variable");
    }
}


void SlaveInstance3::GetInt32(
    const FMIValueReference /*vr*/[],
    std::size_t nvr,
    FMIInteger /*value*/[],
    std::size_t /*nValues*/) const
{
    if (nvr != 0) {
        throw std::logic_error("Attempted to get nonexistent variable");
    }
}


void SlaveInstance3::GetUInt32(
    const FMIValueReference /*vr*/[],
    std::size_t nvr,
    FMIUInt32 /*value*/[],
    std::size_t /*nValues*/) const
{
    if (nvr != 0) {
        throw std::logic_error("Attempted to get nonexistent variable");
    }
}


void SlaveInstance3::GetInt64(
    const FMIValueReference /*vr*/[],
    std::size_t nvr,
    FMIInt64 /*value*/[],
    std::size_t /*nValues*/) const
{
    if (nvr != 0) {
        throw std::logic_error("Attempted to get nonexistent variable");
    }
}


void SlaveInstance3::GetUInt64(
    const FMIValueReference /*vr*/[],
    std::size_t nvr,
    FMIUInt64 /*value*/[],
    std::size_t /*nValues*/) const
{
    if (nvr != 0) {
        throw std::logic_error("Attempted to get nonexistent variable");
    }
}


void SlaveInstance3::GetBoolean(
    const FMIValueReference /*vr*/[],
    std::size_t nvr,
    FMIBoolean /*value*/[],
    std::size_t /*nValues*/) const
{
    if (nvr != 0) {
        throw std::logic_error("Attempted to get nonexistent variable");
    }
}


void SlaveInstance3::GetString(
    const FMIValueReference /*vr*/[],
    std::size_t nvr,
    FMIString /*value*/[],
    std::size_t /*nValues*/) const
{
    if (nvr != 0) {
        throw std::logic_error("Attempted to get nonexistent variable");
    }
}


void SlaveInstance3::GetBinary(
    const FMIValueReference /*vr*/[],
    std::size_t nvr,
    std::size_t /*sizes*/[],
    FMIBinary /*value*/[],
    std::size_t /*nValues*/) const
{
    if (nvr != 0) {
        throw std::logic_error("Attempted to get nonexistent variable");
    }
}


void SlaveInstance3::GetClock(
    const FMIValueReference /*vr*/[],
    std::size_t nvr,
    FMIClock /*value*/[]) const
{
    if (nvr != 0) {
        throw std::logic_error("Attempted to get nonexistent variable");
    }
}


void SlaveInstance3::GetFMUState(FMIFMUState* /*state*/)
{
    throw std::logic_error("Operation not supported: get FMU state");
}


void SlaveInstance3::SetFMUState(FMIFMUState /*state*/)
{
    throw std::logic_error("Operation not supported: set FMU state");
}


void SlaveInstance3::FreeFMUState(FMIFMUState /*state*/)
{
    throw std::logic_error("Operation not supported: free FMU state");
}


std::size_t SlaveInstance3::SerializedFMUStateSize(FMIFMUState /*state*/)
{
    throw std::logic_error("Operation not supported: get serialized FMU state size");
}


void SlaveInstance3::SerializeFMUState(
    FMIFMUState /*state*/,
    FMIByte /*data*/[],
    std::size_t /*size*/)
{
    throw std::logic_error("Operation not supported: serialize FMU state");
}


FMIFMUState SlaveInstance3::DeserializeFMUState(
    const FMIByte /*data*/[],
    std::size_t /*size*/)
{
    throw std::logic_error("Operation not supported: deserialize FMU state");
}


void SlaveInstance3::GetDirectionalDerivative(
    const FMIValueReference /*unknowns*/[],
    std::size_t /*nUnknowns*/,
    const FMIValueReference /*knowns*/[],
    std::size_t /*nKnowns*/,
    const FMIReal /*seed*/[],
    std::size_t /*nSeed*/,
    FMIReal /*sensitivity*/[],
    std::size_t /*nSensitivity*/) const
{
    throw std::logic_error("Operation not supported: get directional derivative");
}


void SlaveInstance3::GetAdjointDerivative(
    const FMIValueReference /*unknowns*/[],
    std::size_t /*nUnknowns*/,
    const FMIValueReference /*knowns*/[],
    std::size_t /*nKnowns*/,
    const FMIReal /*seed*/[],
    std::size_t /*nSeed*/,
    FMIReal /*sensitivity*/[],
    std::size_t /*nSensitivity*/) const
{
    throw std::logic_error("Operation not supported: get adjoint derivative");
}


void SlaveInstance3::GetVariableDependencies(
    FMIValueReference /*dependent*/,
    const std::size_t /*elementIndicesOfDependent*/[],
    FMIValueReference /*independents*/[],
    std::size_t /*elementIndicesOfIndependents*/[],
    FMIDependencyKind /*dependencyKinds*/[],
    std::size_t /*nDependencies*/) const
{
    throw std::logic_error("Operation not supported: get variable dependencies");
}


void SlaveInstance3::GetOutputDerivatives(
    const FMIValueReference /*vr*/[],
    std::size_t /*nvr*/,
    const FMIInteger /*orders*/[],
    FMIReal /*values*/[],
    std::size_t /*nValues*/) const
{
    throw std::logic_error("Operation not supported: get output derivatives");
}


std::size_t SlaveInstance3::GetNumberOfVariableDependencies(
    FMIValueReference /*valueReference*/) const
{
    throw std::logic_error("Operation not supported: get number of variable dependencies");
}


SlaveInstance3::~SlaveInstance3() CPPFMU_NOEXCEPT
{
    // Do nothing
}


} // namespace cppfmu
