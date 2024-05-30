/* Copyright 2016-2019, SINTEF Ocean.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
#include "cppfmu_cs.hpp"

#include <stdexcept>


namespace cppfmu
{

// =============================================================================
// SlaveInstance
// =============================================================================


void SlaveInstance::SetupExperiment(
    FMIBoolean /*toleranceDefined*/,
    FMIReal /*tolerance*/,
    FMIReal /*tStart*/,
    FMIBoolean /*stopTimeDefined*/,
    FMIReal /*tStop*/)
{
    // Do nothing
}


void SlaveInstance::EnterInitializationMode()
{
    // Do nothing
}


void SlaveInstance::ExitInitializationMode()
{
    // Do nothing
}


void SlaveInstance::Terminate()
{
    // Do nothing
}


void SlaveInstance::Reset()
{
    // Do nothing
}


void SlaveInstance::SetReal(
    const FMIValueReference /*vr*/[],
    std::size_t nvr,
    const FMIReal /*value*/[])
{
    if (nvr != 0) {
        throw std::logic_error("Attempted to set nonexistent variable");
    }
}


void SlaveInstance::SetInteger(
    const FMIValueReference /*vr*/[],
    std::size_t nvr,
    const FMIInteger /*value*/[])
{
    if (nvr != 0) {
        throw std::logic_error("Attempted to set nonexistent variable");
    }
}


void SlaveInstance::SetBoolean(
    const FMIValueReference /*vr*/[],
    std::size_t nvr,
    const FMIBoolean /*value*/[])
{
    if (nvr != 0) {
        throw std::logic_error("Attempted to set nonexistent variable");
    }
}


void SlaveInstance::SetString(
    const FMIValueReference /*vr*/[],
    std::size_t nvr,
    const FMIString /*value*/[])
{
    if (nvr != 0) {
        throw std::logic_error("Attempted to set nonexistent variable");
    }
}


void SlaveInstance::GetReal(
    const FMIValueReference /*vr*/[],
    std::size_t nvr,
    FMIReal /*value*/[]) const
{
    if (nvr != 0) {
        throw std::logic_error("Attempted to get nonexistent variable");
    }
}


void SlaveInstance::GetInteger(
    const FMIValueReference /*vr*/[],
    std::size_t nvr,
    FMIInteger /*value*/[]) const
{
    if (nvr != 0) {
        throw std::logic_error("Attempted to get nonexistent variable");
    }
}


void SlaveInstance::GetBoolean(
    const FMIValueReference /*vr*/[],
    std::size_t nvr,
    FMIBoolean /*value*/[]) const
{
    if (nvr != 0) {
        throw std::logic_error("Attempted to set nonexistent variable");
    }
}


void SlaveInstance::GetString(
    const FMIValueReference /*vr*/[],
    std::size_t nvr,
    FMIString /*value*/[]) const
{
    if (nvr != 0) {
        throw std::logic_error("Attempted to set nonexistent variable");
    }
}


void SlaveInstance::GetFMUState(FMIFMUState* state)
{
    throw std::logic_error("Operation not supported: get FMU state");
}


void SlaveInstance::SetFMUState(FMIFMUState state)
{
    throw std::logic_error("Operation not supported: set FMU state");
}


void SlaveInstance::FreeFMUState(FMIFMUState state)
{
    throw std::logic_error("Operation not supported: free FMU state");
}


std::size_t SlaveInstance::SerializedFMUStateSize(FMIFMUState state)
{
    throw std::logic_error("Operation not supported: get serialized FMU state size");
}


void SlaveInstance::SerializeFMUState(
    FMIFMUState state,
    FMIByte data[],
    std::size_t size)
{
    throw std::logic_error("Operation not supported: serialize FMU state");
}


FMIFMUState SlaveInstance::DeserializeFMUState(
    const FMIByte data[],
    std::size_t size)
{
    throw std::logic_error("Operation not supported: deserialize FMU state");
}



SlaveInstance::~SlaveInstance() CPPFMU_NOEXCEPT
{
    // Do nothing
}


} // namespace
