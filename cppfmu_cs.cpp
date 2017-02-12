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


// =============================================================================
// BasicSlaveInstance
// =============================================================================


BasicSlaveInstance::BasicSlaveInstance(
    std::size_t realVarCount,
    std::size_t integerVarCount,
    std::size_t booleanVarCount,
    std::size_t stringVarCount,
    Memory memory)
    : m_realVariables(
        realVarCount,
        0.0,
        Allocator<double>{memory})
    , m_integerVariables(
        integerVarCount,
        0,
        Allocator<int>{memory})
    , m_booleanVariablesSize(booleanVarCount)
    , m_booleanVariables(
        Allocator<bool>{memory}.allocate(booleanVarCount),
        std::function<void(bool*)>([=] (bool* p) {
            Allocator<bool>{memory}.deallocate(p, booleanVarCount);
        }))
    , m_stringVariables(
        stringVarCount,
        String(Allocator<char>{memory}),
        Allocator<String>{memory})
{

}


void BasicSlaveInstance::Reset()
{
    ZeroVars();
    PostReset();
}


void BasicSlaveInstance::SetReal(
    const fmiValueReference vr[],
    std::size_t nvr,
    const fmiReal value[])
{
    for (std::size_t i = 0; i < nvr; ++i) {
        m_realVariables.at(vr[i]) = value[i];
    }
}


void BasicSlaveInstance::SetInteger(
    const fmiValueReference vr[],
    std::size_t nvr,
    const fmiInteger value[])
{
    for (std::size_t i = 0; i < nvr; ++i) {
        m_integerVariables.at(vr[i]) = value[i];
    }
}


void BasicSlaveInstance::SetBoolean(
    const fmiValueReference vr[],
    std::size_t nvr,
    const fmiBoolean value[])
{
    for (std::size_t i = 0; i < nvr; ++i) {
        if (vr[i] >= m_booleanVariablesSize) {
            throw std::out_of_range("Value reference out of range");
        }
        m_booleanVariables[vr[i]] = !!value[i];
    }
}


void BasicSlaveInstance::SetString(
    const fmiValueReference vr[],
    std::size_t nvr,
    const fmiString value[])
{
    for (std::size_t i = 0; i < nvr; ++i) {
        m_stringVariables.at(vr[i]) = value[i];
    }
}


void BasicSlaveInstance::GetReal(
    const fmiValueReference vr[],
    std::size_t nvr,
    fmiReal value[]) const
{
    for (std::size_t i = 0; i < nvr; ++i) {
        value[i] = m_realVariables.at(vr[i]);
    }
}


void BasicSlaveInstance::GetInteger(
    const fmiValueReference vr[],
    std::size_t nvr,
    fmiInteger value[]) const
{
    for (std::size_t i = 0; i < nvr; ++i) {
        value[i] = m_integerVariables.at(vr[i]);
    }
}


void BasicSlaveInstance::GetBoolean(
    const fmiValueReference vr[],
    std::size_t nvr,
    fmiBoolean value[]) const
{
    for (std::size_t i = 0; i < nvr; ++i) {
        if (vr[i] >= m_booleanVariablesSize) {
            throw std::out_of_range("Value reference out of range");
        }
        value[i] = m_booleanVariables[vr[i]] ? fmiTrue : fmiFalse;
    }
}


void BasicSlaveInstance::GetString(
    const fmiValueReference vr[],
    std::size_t nvr,
    fmiString value[]) const
{
    for (std::size_t i = 0; i < nvr; ++i) {
        value[i] = m_stringVariables.at(vr[i]).c_str();
    }
}


void BasicSlaveInstance::PostReset()
{
    // Do nothing
}


double& BasicSlaveInstance::RealVariable(std::size_t i)
{
    return m_realVariables[i];
}


const double& BasicSlaveInstance::RealVariable(std::size_t i) const
{
    return m_realVariables[i];
}


int& BasicSlaveInstance::IntegerVariable(std::size_t i)
{
    return m_integerVariables[i];
}


const int& BasicSlaveInstance::IntegerVariable(std::size_t i) const
{
    return m_integerVariables[i];
}


bool& BasicSlaveInstance::BooleanVariable(std::size_t i)
{
    return m_booleanVariables[i];
}


const bool& BasicSlaveInstance::BooleanVariable(std::size_t i) const
{
    return m_booleanVariables[i];
}


String& BasicSlaveInstance::StringVariable(std::size_t i)
{
    return m_stringVariables[i];
}


const String& BasicSlaveInstance::StringVariable(std::size_t i) const
{
    return m_stringVariables[i];
}


void BasicSlaveInstance::ZeroVars()
{
    for (auto& v : m_realVariables) v = 0.0;
    for (auto& v : m_integerVariables) v = 0;
    for (std::size_t i = 0; i < m_booleanVariablesSize; ++i) {
        m_booleanVariables[i] = false;
    }
    for (auto& v : m_stringVariables) v.clear();
}


} // namespace
