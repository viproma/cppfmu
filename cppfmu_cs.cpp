/* Copyright 2016-2017, SINTEF Ocean.
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


void SlaveInstance::Initialize(
    fmiReal tStart,
    fmiBoolean stopTimeDefined,
    fmiReal tStop)
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
    const fmiValueReference vr[],
    std::size_t nvr,
    const fmiReal value[])
{
    if (nvr != 0) {
        throw std::logic_error("Attempted to set nonexistent variable");
    }
}


void SlaveInstance::SetInteger(
    const fmiValueReference vr[],
    std::size_t nvr,
    const fmiInteger value[])
{
    if (nvr != 0) {
        throw std::logic_error("Attempted to set nonexistent variable");
    }
}


void SlaveInstance::SetBoolean(
    const fmiValueReference vr[],
    std::size_t nvr,
    const fmiBoolean value[])
{
    if (nvr != 0) {
        throw std::logic_error("Attempted to set nonexistent variable");
    }
}


void SlaveInstance::SetString(
    const fmiValueReference vr[],
    std::size_t nvr,
    const fmiString value[])
{
    if (nvr != 0) {
        throw std::logic_error("Attempted to set nonexistent variable");
    }
}


void SlaveInstance::GetReal(
    const fmiValueReference vr[],
    std::size_t nvr,
    fmiReal value[]) const
{
    if (nvr != 0) {
        throw std::logic_error("Attempted to get nonexistent variable");
    }
}


void SlaveInstance::GetInteger(
    const fmiValueReference vr[],
    std::size_t nvr,
    fmiInteger value[]) const
{
    if (nvr != 0) {
        throw std::logic_error("Attempted to get nonexistent variable");
    }
}


void SlaveInstance::GetBoolean(
    const fmiValueReference vr[],
    std::size_t nvr,
    fmiBoolean value[]) const
{
    if (nvr != 0) {
        throw std::logic_error("Attempted to set nonexistent variable");
    }
}


void SlaveInstance::GetString(
    const fmiValueReference vr[],
    std::size_t nvr,
    fmiString value[]) const
{
    if (nvr != 0) {
        throw std::logic_error("Attempted to set nonexistent variable");
    }
}


SlaveInstance::~SlaveInstance() CPPFMU_NOEXCEPT
{
    // Do nothing
}


} // namespace
