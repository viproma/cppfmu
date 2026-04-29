/* Copyright 2016-2026, SINTEF Ocean.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
#include <exception>
#include <functional>
#include <limits>
#include <string>

#include "cppfmu_cs_fmi3.hpp"


namespace
{
    struct Component
    {
        Component(
            cppfmu::FMIString instanceName,
            cppfmu::FMIComponentEnvironment instanceEnvironment,
            fmi3LogMessageCallback logMessage,
            cppfmu::FMIBoolean loggingOn)
            : instanceName{instanceName ? instanceName : ""}
            , instanceEnvironment{instanceEnvironment}
            , logMessage{logMessage}
            , debugLoggingEnabled{loggingOn == fmi3True}
            , lastSuccessfulTime{std::numeric_limits<cppfmu::FMIReal>::quiet_NaN()}
        {
        }

        void Log(fmi3Status status, cppfmu::FMIString category, cppfmu::FMIString message) const
        {
            if (logMessage) {
                logMessage(instanceEnvironment, status, category, message);
            }
        }

        std::string instanceName;
        cppfmu::FMIComponentEnvironment instanceEnvironment;
        fmi3LogMessageCallback logMessage;
        bool debugLoggingEnabled;
        cppfmu::UniquePtr<cppfmu::SlaveInstance3> slave;
        cppfmu::FMIReal lastSuccessfulTime;
    };

    cppfmu::FMIStatus toCppfmuStatus(fmi3Status s)
    {
        switch (s) {
            case fmi3OK:      return cppfmu::FMIOK;
            case fmi3Warning: return cppfmu::FMIWarning;
            case fmi3Discard: return cppfmu::FMIDiscard;
            case fmi3Error:   return cppfmu::FMIError;
            case fmi3Fatal:   return cppfmu::FMIFatal;
            default:          return cppfmu::FMIError;
        }
    }
}


extern "C"
{

// =============================================================================
// FMI 3.0 functions
// =============================================================================


const char* fmi3GetVersion()
{
    return fmi3Version;
}


fmi3Status fmi3SetDebugLogging(
    fmi3Instance instance,
    fmi3Boolean loggingOn,
    size_t nCategories,
    const fmi3String categories[])
{
    (void) nCategories;
    (void) categories;
    auto component = reinterpret_cast<Component*>(instance);
    component->debugLoggingEnabled = (loggingOn == fmi3True);
    return fmi3OK;
}


fmi3Instance fmi3InstantiateCoSimulation(
    fmi3String instanceName,
    fmi3String instantiationToken,
    fmi3String resourceLocation,
    fmi3Boolean visible,
    fmi3Boolean loggingOn,
    fmi3Boolean eventModeUsed,
    fmi3Boolean earlyReturnAllowed,
    const fmi3ValueReference requiredIntermediateVariables[],
    size_t nRequiredIntermediateVariables,
    fmi3InstanceEnvironment instanceEnvironment,
    fmi3LogMessageCallback logMessage,
    fmi3IntermediateUpdateCallback intermediateUpdate)
{
    (void) intermediateUpdate;
    try {
        auto component = new Component(
            instanceName, instanceEnvironment, logMessage, loggingOn);

        auto loggerFn = [component](cppfmu::FMIStatus status, cppfmu::FMIString category, cppfmu::FMIString message) {
            component->Log(toCppfmuStatus(status), category, message);
        };

        component->slave = CppfmuInstantiateSlave(
            instanceName,
            instantiationToken,
            resourceLocation,
            visible,
            loggingOn,
            eventModeUsed,
            earlyReturnAllowed,
            requiredIntermediateVariables,
            nRequiredIntermediateVariables,
            instanceEnvironment,
            loggerFn);

        return component;
    } catch (const cppfmu::FatalError& e) {
        if (logMessage) {
            logMessage(instanceEnvironment, fmi3Fatal, "cppfmu", e.what());
        }
        return nullptr;
    } catch (const std::exception& e) {
        if (logMessage) {
            logMessage(instanceEnvironment, fmi3Error, "cppfmu", e.what());
        }
        return nullptr;
    }
}


void fmi3FreeInstance(fmi3Instance instance)
{
    auto component = reinterpret_cast<Component*>(instance);
    delete component;
}


fmi3Status fmi3EnterInitializationMode(
    fmi3Instance instance,
    fmi3Boolean toleranceDefined,
    fmi3Float64 tolerance,
    fmi3Float64 startTime,
    fmi3Boolean stopTimeDefined,
    fmi3Float64 stopTime)
{
    auto component = reinterpret_cast<Component*>(instance);
    try {
        component->slave->EnterInitializationMode(
            toleranceDefined,
            static_cast<cppfmu::FMIReal>(tolerance),
            static_cast<cppfmu::FMIReal>(startTime),
            stopTimeDefined,
            static_cast<cppfmu::FMIReal>(stopTime));
        return fmi3OK;
    } catch (const cppfmu::FatalError& e) {
        component->Log(fmi3Fatal, "cppfmu", e.what());
        return fmi3Fatal;
    } catch (const std::exception& e) {
        component->Log(fmi3Error, "cppfmu", e.what());
        return fmi3Error;
    }
}


fmi3Status fmi3ExitInitializationMode(fmi3Instance instance)
{
    auto component = reinterpret_cast<Component*>(instance);
    try {
        component->slave->ExitInitializationMode();
        return fmi3OK;
    } catch (const cppfmu::FatalError& e) {
        component->Log(fmi3Fatal, "cppfmu", e.what());
        return fmi3Fatal;
    } catch (const std::exception& e) {
        component->Log(fmi3Error, "cppfmu", e.what());
        return fmi3Error;
    }
}


fmi3Status fmi3EnterEventMode(fmi3Instance instance)
{
    auto component = reinterpret_cast<Component*>(instance);
    component->Log(fmi3Error, "cppfmu", "FMI function not supported: fmi3EnterEventMode");
    return fmi3Error;
}


fmi3Status fmi3EnterStepMode(fmi3Instance instance)
{
    auto component = reinterpret_cast<Component*>(instance);
    component->Log(fmi3Error, "cppfmu", "FMI function not supported: fmi3EnterStepMode");
    return fmi3Error;
}


fmi3Status fmi3Terminate(fmi3Instance instance)
{
    auto component = reinterpret_cast<Component*>(instance);
    try {
        component->slave->Terminate();
        return fmi3OK;
    } catch (const cppfmu::FatalError& e) {
        component->Log(fmi3Fatal, "cppfmu", e.what());
        return fmi3Fatal;
    } catch (const std::exception& e) {
        component->Log(fmi3Error, "cppfmu", e.what());
        return fmi3Error;
    }
}


fmi3Status fmi3Reset(fmi3Instance instance)
{
    auto component = reinterpret_cast<Component*>(instance);
    try {
        component->slave->Reset();
        return fmi3OK;
    } catch (const cppfmu::FatalError& e) {
        component->Log(fmi3Fatal, "cppfmu", e.what());
        return fmi3Fatal;
    } catch (const std::exception& e) {
        component->Log(fmi3Error, "cppfmu", e.what());
        return fmi3Error;
    }
}


// ============================================================================
// Get functions
// ============================================================================


fmi3Status fmi3GetFloat32(
    fmi3Instance instance,
    const fmi3ValueReference vr[],
    size_t nvr,
    fmi3Float32 value[],
    size_t nValues)
{
    auto component = reinterpret_cast<Component*>(instance);
    try {
        component->slave->GetFloat32(vr, nvr, value, nValues);
        return fmi3OK;
    } catch (const cppfmu::FatalError& e) {
        component->Log(fmi3Fatal, "cppfmu", e.what());
        return fmi3Fatal;
    } catch (const std::exception& e) {
        component->Log(fmi3Error, "cppfmu", e.what());
        return fmi3Error;
    }
}


fmi3Status fmi3GetFloat64(
    fmi3Instance instance,
    const fmi3ValueReference vr[],
    size_t nvr,
    fmi3Float64 value[],
    size_t nValues)
{
    auto component = reinterpret_cast<Component*>(instance);
    try {
        component->slave->GetFloat64(vr, nvr, value, nValues);
        return fmi3OK;
    } catch (const cppfmu::FatalError& e) {
        component->Log(fmi3Fatal, "cppfmu", e.what());
        return fmi3Fatal;
    } catch (const std::exception& e) {
        component->Log(fmi3Error, "cppfmu", e.what());
        return fmi3Error;
    }
}


fmi3Status fmi3GetInt8(
    fmi3Instance instance,
    const fmi3ValueReference vr[],
    size_t nvr,
    fmi3Int8 value[],
    size_t nValues)
{
    auto component = reinterpret_cast<Component*>(instance);
    try {
        component->slave->GetInt8(vr, nvr, value, nValues);
        return fmi3OK;
    } catch (const cppfmu::FatalError& e) {
        component->Log(fmi3Fatal, "cppfmu", e.what());
        return fmi3Fatal;
    } catch (const std::exception& e) {
        component->Log(fmi3Error, "cppfmu", e.what());
        return fmi3Error;
    }
}


fmi3Status fmi3GetUInt8(
    fmi3Instance instance,
    const fmi3ValueReference vr[],
    size_t nvr,
    fmi3UInt8 value[],
    size_t nValues)
{
    auto component = reinterpret_cast<Component*>(instance);
    try {
        component->slave->GetUInt8(vr, nvr, value, nValues);
        return fmi3OK;
    } catch (const cppfmu::FatalError& e) {
        component->Log(fmi3Fatal, "cppfmu", e.what());
        return fmi3Fatal;
    } catch (const std::exception& e) {
        component->Log(fmi3Error, "cppfmu", e.what());
        return fmi3Error;
    }
}


fmi3Status fmi3GetInt16(
    fmi3Instance instance,
    const fmi3ValueReference vr[],
    size_t nvr,
    fmi3Int16 value[],
    size_t nValues)
{
    auto component = reinterpret_cast<Component*>(instance);
    try {
        component->slave->GetInt16(vr, nvr, value, nValues);
        return fmi3OK;
    } catch (const cppfmu::FatalError& e) {
        component->Log(fmi3Fatal, "cppfmu", e.what());
        return fmi3Fatal;
    } catch (const std::exception& e) {
        component->Log(fmi3Error, "cppfmu", e.what());
        return fmi3Error;
    }
}


fmi3Status fmi3GetUInt16(
    fmi3Instance instance,
    const fmi3ValueReference vr[],
    size_t nvr,
    fmi3UInt16 value[],
    size_t nValues)
{
    auto component = reinterpret_cast<Component*>(instance);
    try {
        component->slave->GetUInt16(vr, nvr, value, nValues);
        return fmi3OK;
    } catch (const cppfmu::FatalError& e) {
        component->Log(fmi3Fatal, "cppfmu", e.what());
        return fmi3Fatal;
    } catch (const std::exception& e) {
        component->Log(fmi3Error, "cppfmu", e.what());
        return fmi3Error;
    }
}


fmi3Status fmi3GetInt32(
    fmi3Instance instance,
    const fmi3ValueReference vr[],
    size_t nvr,
    fmi3Int32 value[],
    size_t nValues)
{
    auto component = reinterpret_cast<Component*>(instance);
    try {
        component->slave->GetInt32(vr, nvr, value, nValues);
        return fmi3OK;
    } catch (const cppfmu::FatalError& e) {
        component->Log(fmi3Fatal, "cppfmu", e.what());
        return fmi3Fatal;
    } catch (const std::exception& e) {
        component->Log(fmi3Error, "cppfmu", e.what());
        return fmi3Error;
    }
}


fmi3Status fmi3GetUInt32(
    fmi3Instance instance,
    const fmi3ValueReference vr[],
    size_t nvr,
    fmi3UInt32 value[],
    size_t nValues)
{
    auto component = reinterpret_cast<Component*>(instance);
    try {
        component->slave->GetUInt32(vr, nvr, value, nValues);
        return fmi3OK;
    } catch (const cppfmu::FatalError& e) {
        component->Log(fmi3Fatal, "cppfmu", e.what());
        return fmi3Fatal;
    } catch (const std::exception& e) {
        component->Log(fmi3Error, "cppfmu", e.what());
        return fmi3Error;
    }
}


fmi3Status fmi3GetInt64(
    fmi3Instance instance,
    const fmi3ValueReference vr[],
    size_t nvr,
    fmi3Int64 value[],
    size_t nValues)
{
    auto component = reinterpret_cast<Component*>(instance);
    try {
        component->slave->GetInt64(vr, nvr, value, nValues);
        return fmi3OK;
    } catch (const cppfmu::FatalError& e) {
        component->Log(fmi3Fatal, "cppfmu", e.what());
        return fmi3Fatal;
    } catch (const std::exception& e) {
        component->Log(fmi3Error, "cppfmu", e.what());
        return fmi3Error;
    }
}


fmi3Status fmi3GetUInt64(
    fmi3Instance instance,
    const fmi3ValueReference vr[],
    size_t nvr,
    fmi3UInt64 value[],
    size_t nValues)
{
    auto component = reinterpret_cast<Component*>(instance);
    try {
        component->slave->GetUInt64(vr, nvr, value, nValues);
        return fmi3OK;
    } catch (const cppfmu::FatalError& e) {
        component->Log(fmi3Fatal, "cppfmu", e.what());
        return fmi3Fatal;
    } catch (const std::exception& e) {
        component->Log(fmi3Error, "cppfmu", e.what());
        return fmi3Error;
    }
}


fmi3Status fmi3GetBoolean(
    fmi3Instance instance,
    const fmi3ValueReference vr[],
    size_t nvr,
    fmi3Boolean value[],
    size_t nValues)
{
    auto component = reinterpret_cast<Component*>(instance);
    try {
        component->slave->GetBoolean(vr, nvr, value, nValues);
        return fmi3OK;
    } catch (const cppfmu::FatalError& e) {
        component->Log(fmi3Fatal, "cppfmu", e.what());
        return fmi3Fatal;
    } catch (const std::exception& e) {
        component->Log(fmi3Error, "cppfmu", e.what());
        return fmi3Error;
    }
}


fmi3Status fmi3GetString(
    fmi3Instance instance,
    const fmi3ValueReference vr[],
    size_t nvr,
    fmi3String value[],
    size_t nValues)
{
    auto component = reinterpret_cast<Component*>(instance);
    try {
        component->slave->GetString(vr, nvr, value, nValues);
        return fmi3OK;
    } catch (const cppfmu::FatalError& e) {
        component->Log(fmi3Fatal, "cppfmu", e.what());
        return fmi3Fatal;
    } catch (const std::exception& e) {
        component->Log(fmi3Error, "cppfmu", e.what());
        return fmi3Error;
    }
}


fmi3Status fmi3GetBinary(
    fmi3Instance instance,
    const fmi3ValueReference vr[],
    size_t nvr,
    size_t sizes[],
    fmi3Binary value[],
    size_t nValues)
{
    auto component = reinterpret_cast<Component*>(instance);
    try {
        component->slave->GetBinary(vr, nvr, sizes, value, nValues);
        return fmi3OK;
    } catch (const cppfmu::FatalError& e) {
        component->Log(fmi3Fatal, "cppfmu", e.what());
        return fmi3Fatal;
    } catch (const std::exception& e) {
        component->Log(fmi3Error, "cppfmu", e.what());
        return fmi3Error;
    }
}


fmi3Status fmi3GetClock(
    fmi3Instance instance,
    const fmi3ValueReference vr[],
    size_t nvr,
    fmi3Clock value[])
{
    auto component = reinterpret_cast<Component*>(instance);
    try {
        component->slave->GetClock(vr, nvr, value);
        return fmi3OK;
    } catch (const cppfmu::FatalError& e) {
        component->Log(fmi3Fatal, "cppfmu", e.what());
        return fmi3Fatal;
    } catch (const std::exception& e) {
        component->Log(fmi3Error, "cppfmu", e.what());
        return fmi3Error;
    }
}


// ============================================================================
// Set functions
// ============================================================================


fmi3Status fmi3SetFloat32(
    fmi3Instance instance,
    const fmi3ValueReference vr[],
    size_t nvr,
    const fmi3Float32 value[],
    size_t nValues)
{
    auto component = reinterpret_cast<Component*>(instance);
    try {
        component->slave->SetFloat32(vr, nvr, value, nValues);
        return fmi3OK;
    } catch (const cppfmu::FatalError& e) {
        component->Log(fmi3Fatal, "cppfmu", e.what());
        return fmi3Fatal;
    } catch (const std::exception& e) {
        component->Log(fmi3Error, "cppfmu", e.what());
        return fmi3Error;
    }
}


fmi3Status fmi3SetFloat64(
    fmi3Instance instance,
    const fmi3ValueReference vr[],
    size_t nvr,
    const fmi3Float64 value[],
    size_t nValues)
{
    auto component = reinterpret_cast<Component*>(instance);
    try {
        component->slave->SetFloat64(vr, nvr, value, nValues);
        return fmi3OK;
    } catch (const cppfmu::FatalError& e) {
        component->Log(fmi3Fatal, "cppfmu", e.what());
        return fmi3Fatal;
    } catch (const std::exception& e) {
        component->Log(fmi3Error, "cppfmu", e.what());
        return fmi3Error;
    }
}


fmi3Status fmi3SetInt8(
    fmi3Instance instance,
    const fmi3ValueReference vr[],
    size_t nvr,
    const fmi3Int8 value[],
    size_t nValues)
{
    auto component = reinterpret_cast<Component*>(instance);
    try {
        component->slave->SetInt8(vr, nvr, value, nValues);
        return fmi3OK;
    } catch (const cppfmu::FatalError& e) {
        component->Log(fmi3Fatal, "cppfmu", e.what());
        return fmi3Fatal;
    } catch (const std::exception& e) {
        component->Log(fmi3Error, "cppfmu", e.what());
        return fmi3Error;
    }
}


fmi3Status fmi3SetUInt8(
    fmi3Instance instance,
    const fmi3ValueReference vr[],
    size_t nvr,
    const fmi3UInt8 value[],
    size_t nValues)
{
    auto component = reinterpret_cast<Component*>(instance);
    try {
        component->slave->SetUInt8(vr, nvr, value, nValues);
        return fmi3OK;
    } catch (const cppfmu::FatalError& e) {
        component->Log(fmi3Fatal, "cppfmu", e.what());
        return fmi3Fatal;
    } catch (const std::exception& e) {
        component->Log(fmi3Error, "cppfmu", e.what());
        return fmi3Error;
    }
}


fmi3Status fmi3SetInt16(
    fmi3Instance instance,
    const fmi3ValueReference vr[],
    size_t nvr,
    const fmi3Int16 value[],
    size_t nValues)
{
    auto component = reinterpret_cast<Component*>(instance);
    try {
        component->slave->SetInt16(vr, nvr, value, nValues);
        return fmi3OK;
    } catch (const cppfmu::FatalError& e) {
        component->Log(fmi3Fatal, "cppfmu", e.what());
        return fmi3Fatal;
    } catch (const std::exception& e) {
        component->Log(fmi3Error, "cppfmu", e.what());
        return fmi3Error;
    }
}


fmi3Status fmi3SetUInt16(
    fmi3Instance instance,
    const fmi3ValueReference vr[],
    size_t nvr,
    const fmi3UInt16 value[],
    size_t nValues)
{
    auto component = reinterpret_cast<Component*>(instance);
    try {
        component->slave->SetUInt16(vr, nvr, value, nValues);
        return fmi3OK;
    } catch (const cppfmu::FatalError& e) {
        component->Log(fmi3Fatal, "cppfmu", e.what());
        return fmi3Fatal;
    } catch (const std::exception& e) {
        component->Log(fmi3Error, "cppfmu", e.what());
        return fmi3Error;
    }
}


fmi3Status fmi3SetInt32(
    fmi3Instance instance,
    const fmi3ValueReference vr[],
    size_t nvr,
    const fmi3Int32 value[],
    size_t nValues)
{
    auto component = reinterpret_cast<Component*>(instance);
    try {
        component->slave->SetInt32(vr, nvr, value, nValues);
        return fmi3OK;
    } catch (const cppfmu::FatalError& e) {
        component->Log(fmi3Fatal, "cppfmu", e.what());
        return fmi3Fatal;
    } catch (const std::exception& e) {
        component->Log(fmi3Error, "cppfmu", e.what());
        return fmi3Error;
    }
}


fmi3Status fmi3SetUInt32(
    fmi3Instance instance,
    const fmi3ValueReference vr[],
    size_t nvr,
    const fmi3UInt32 value[],
    size_t nValues)
{
    auto component = reinterpret_cast<Component*>(instance);
    try {
        component->slave->SetUInt32(vr, nvr, value, nValues);
        return fmi3OK;
    } catch (const cppfmu::FatalError& e) {
        component->Log(fmi3Fatal, "cppfmu", e.what());
        return fmi3Fatal;
    } catch (const std::exception& e) {
        component->Log(fmi3Error, "cppfmu", e.what());
        return fmi3Error;
    }
}


fmi3Status fmi3SetInt64(
    fmi3Instance instance,
    const fmi3ValueReference vr[],
    size_t nvr,
    const fmi3Int64 value[],
    size_t nValues)
{
    auto component = reinterpret_cast<Component*>(instance);
    try {
        component->slave->SetInt64(vr, nvr, value, nValues);
        return fmi3OK;
    } catch (const cppfmu::FatalError& e) {
        component->Log(fmi3Fatal, "cppfmu", e.what());
        return fmi3Fatal;
    } catch (const std::exception& e) {
        component->Log(fmi3Error, "cppfmu", e.what());
        return fmi3Error;
    }
}


fmi3Status fmi3SetUInt64(
    fmi3Instance instance,
    const fmi3ValueReference vr[],
    size_t nvr,
    const fmi3UInt64 value[],
    size_t nValues)
{
    auto component = reinterpret_cast<Component*>(instance);
    try {
        component->slave->SetUInt64(vr, nvr, value, nValues);
        return fmi3OK;
    } catch (const cppfmu::FatalError& e) {
        component->Log(fmi3Fatal, "cppfmu", e.what());
        return fmi3Fatal;
    } catch (const std::exception& e) {
        component->Log(fmi3Error, "cppfmu", e.what());
        return fmi3Error;
    }
}


fmi3Status fmi3SetBoolean(
    fmi3Instance instance,
    const fmi3ValueReference vr[],
    size_t nvr,
    const fmi3Boolean value[],
    size_t nValues)
{
    auto component = reinterpret_cast<Component*>(instance);
    try {
        component->slave->SetBoolean(vr, nvr, value, nValues);
        return fmi3OK;
    } catch (const cppfmu::FatalError& e) {
        component->Log(fmi3Fatal, "cppfmu", e.what());
        return fmi3Fatal;
    } catch (const std::exception& e) {
        component->Log(fmi3Error, "cppfmu", e.what());
        return fmi3Error;
    }
}


fmi3Status fmi3SetString(
    fmi3Instance instance,
    const fmi3ValueReference vr[],
    size_t nvr,
    const fmi3String value[],
    size_t nValues)
{
    auto component = reinterpret_cast<Component*>(instance);
    try {
        component->slave->SetString(vr, nvr, value, nValues);
        return fmi3OK;
    } catch (const cppfmu::FatalError& e) {
        component->Log(fmi3Fatal, "cppfmu", e.what());
        return fmi3Fatal;
    } catch (const std::exception& e) {
        component->Log(fmi3Error, "cppfmu", e.what());
        return fmi3Error;
    }
}


fmi3Status fmi3SetBinary(
    fmi3Instance instance,
    const fmi3ValueReference vr[],
    size_t nvr,
    const size_t sizes[],
    const fmi3Binary value[],
    size_t nValues)
{
    auto component = reinterpret_cast<Component*>(instance);
    try {
        component->slave->SetBinary(vr, nvr, sizes, value, nValues);
        return fmi3OK;
    } catch (const cppfmu::FatalError& e) {
        component->Log(fmi3Fatal, "cppfmu", e.what());
        return fmi3Fatal;
    } catch (const std::exception& e) {
        component->Log(fmi3Error, "cppfmu", e.what());
        return fmi3Error;
    }
}


fmi3Status fmi3SetClock(
    fmi3Instance instance,
    const fmi3ValueReference vr[],
    size_t nvr,
    const fmi3Clock value[])
{
    auto component = reinterpret_cast<Component*>(instance);
    try {
        component->slave->SetClock(vr, nvr, value);
        return fmi3OK;
    } catch (const cppfmu::FatalError& e) {
        component->Log(fmi3Fatal, "cppfmu", e.what());
        return fmi3Fatal;
    } catch (const std::exception& e) {
        component->Log(fmi3Error, "cppfmu", e.what());
        return fmi3Error;
    }
}


// ============================================================================
// FMU state functions
// ============================================================================


fmi3Status fmi3GetFMUState(
    fmi3Instance instance,
    fmi3FMUState* state)
{
    auto component = reinterpret_cast<Component*>(instance);
    try {
        component->slave->GetFMUState(state);
        return fmi3OK;
    } catch (const cppfmu::FatalError& e) {
        component->Log(fmi3Fatal, "cppfmu", e.what());
        return fmi3Fatal;
    } catch (const std::exception& e) {
        component->Log(fmi3Error, "cppfmu", e.what());
        return fmi3Error;
    }
}


fmi3Status fmi3SetFMUState(
    fmi3Instance instance,
    fmi3FMUState state)
{
    auto component = reinterpret_cast<Component*>(instance);
    try {
        component->slave->SetFMUState(state);
        return fmi3OK;
    } catch (const cppfmu::FatalError& e) {
        component->Log(fmi3Fatal, "cppfmu", e.what());
        return fmi3Fatal;
    } catch (const std::exception& e) {
        component->Log(fmi3Error, "cppfmu", e.what());
        return fmi3Error;
    }
}


fmi3Status fmi3FreeFMUState(
    fmi3Instance instance,
    fmi3FMUState* state)
{
    if (state == nullptr || *state == nullptr) return fmi3OK;
    auto component = reinterpret_cast<Component*>(instance);
    try {
        component->slave->FreeFMUState(*state);
        *state = nullptr;
        return fmi3OK;
    } catch (const cppfmu::FatalError& e) {
        component->Log(fmi3Fatal, "cppfmu", e.what());
        return fmi3Fatal;
    } catch (const std::exception& e) {
        component->Log(fmi3Error, "cppfmu", e.what());
        return fmi3Error;
    }
}


fmi3Status fmi3SerializedFMUStateSize(
    fmi3Instance instance,
    fmi3FMUState state,
    size_t* size)
{
    auto component = reinterpret_cast<Component*>(instance);
    try {
        *size = component->slave->SerializedFMUStateSize(state);
        return fmi3OK;
    } catch (const cppfmu::FatalError& e) {
        component->Log(fmi3Fatal, "cppfmu", e.what());
        return fmi3Fatal;
    } catch (const std::exception& e) {
        component->Log(fmi3Error, "cppfmu", e.what());
        return fmi3Error;
    }
}


fmi3Status fmi3SerializeFMUState(
    fmi3Instance instance,
    fmi3FMUState state,
    fmi3Byte data[],
    size_t size)
{
    auto component = reinterpret_cast<Component*>(instance);
    try {
        component->slave->SerializeFMUState(state, data, size);
        return fmi3OK;
    } catch (const cppfmu::FatalError& e) {
        component->Log(fmi3Fatal, "cppfmu", e.what());
        return fmi3Fatal;
    } catch (const std::exception& e) {
        component->Log(fmi3Error, "cppfmu", e.what());
        return fmi3Error;
    }
}


fmi3Status fmi3DeserializeFMUState(
    fmi3Instance instance,
    const fmi3Byte data[],
    size_t size,
    fmi3FMUState* state)
{
    auto component = reinterpret_cast<Component*>(instance);
    try {
        *state = component->slave->DeserializeFMUState(data, size);
        return fmi3OK;
    } catch (const cppfmu::FatalError& e) {
        component->Log(fmi3Fatal, "cppfmu", e.what());
        return fmi3Fatal;
    } catch (const std::exception& e) {
        component->Log(fmi3Error, "cppfmu", e.what());
        return fmi3Error;
    }
}


// ============================================================================
// Derivative and dependency functions
// ============================================================================


fmi3Status fmi3GetDirectionalDerivative(
    fmi3Instance instance,
    const fmi3ValueReference unknowns[],
    size_t nUnknowns,
    const fmi3ValueReference knowns[],
    size_t nKnowns,
    const fmi3Float64 seed[],
    size_t nSeed,
    fmi3Float64 sensitivity[],
    size_t nSensitivity)
{
    auto component = reinterpret_cast<Component*>(instance);
    try {
        component->slave->GetDirectionalDerivative(
            unknowns, nUnknowns,
            knowns, nKnowns,
            seed, nSeed,
            sensitivity, nSensitivity);
        return fmi3OK;
    } catch (const cppfmu::FatalError& e) {
        component->Log(fmi3Fatal, "cppfmu", e.what());
        return fmi3Fatal;
    } catch (const std::exception& e) {
        component->Log(fmi3Error, "cppfmu", e.what());
        return fmi3Error;
    }
}


fmi3Status fmi3GetAdjointDerivative(
    fmi3Instance instance,
    const fmi3ValueReference unknowns[],
    size_t nUnknowns,
    const fmi3ValueReference knowns[],
    size_t nKnowns,
    const fmi3Float64 seed[],
    size_t nSeed,
    fmi3Float64 sensitivity[],
    size_t nSensitivity)
{
    auto component = reinterpret_cast<Component*>(instance);
    try {
        component->slave->GetAdjointDerivative(
            unknowns, nUnknowns,
            knowns, nKnowns,
            seed, nSeed,
            sensitivity, nSensitivity);
        return fmi3OK;
    } catch (const cppfmu::FatalError& e) {
        component->Log(fmi3Fatal, "cppfmu", e.what());
        return fmi3Fatal;
    } catch (const std::exception& e) {
        component->Log(fmi3Error, "cppfmu", e.what());
        return fmi3Error;
    }
}


fmi3Status fmi3GetVariableDependencies(
    fmi3Instance instance,
    fmi3ValueReference dependent,
    size_t elementIndicesOfDependent[],
    fmi3ValueReference independents[],
    size_t elementIndicesOfIndependents[],
    fmi3DependencyKind dependencyKinds[],
    size_t nDependencies)
{
    auto component = reinterpret_cast<Component*>(instance);
    try {
        component->slave->GetVariableDependencies(
            dependent,
            elementIndicesOfDependent,
            independents,
            elementIndicesOfIndependents,
            dependencyKinds,
            nDependencies);
        return fmi3OK;
    } catch (const cppfmu::FatalError& e) {
        component->Log(fmi3Fatal, "cppfmu", e.what());
        return fmi3Fatal;
    } catch (const std::exception& e) {
        component->Log(fmi3Error, "cppfmu", e.what());
        return fmi3Error;
    }
}


fmi3Status fmi3GetNumberOfVariableDependencies(
    fmi3Instance instance,
    fmi3ValueReference valueReference,
    size_t* nDependencies)
{
    auto component = reinterpret_cast<Component*>(instance);
    try {
        *nDependencies = component->slave->GetNumberOfVariableDependencies(valueReference);
        return fmi3OK;
    } catch (const cppfmu::FatalError& e) {
        component->Log(fmi3Fatal, "cppfmu", e.what());
        return fmi3Fatal;
    } catch (const std::exception& e) {
        component->Log(fmi3Error, "cppfmu", e.what());
        return fmi3Error;
    }
}


fmi3Status fmi3GetOutputDerivatives(
    fmi3Instance instance,
    const fmi3ValueReference vr[],
    size_t nvr,
    const fmi3Int32 orders[],
    fmi3Float64 values[],
    size_t nValues)
{
    auto component = reinterpret_cast<Component*>(instance);
    try {
        component->slave->GetOutputDerivatives(vr, nvr, orders, values, nValues);
        return fmi3OK;
    } catch (const cppfmu::FatalError& e) {
        component->Log(fmi3Fatal, "cppfmu", e.what());
        return fmi3Fatal;
    } catch (const std::exception& e) {
        component->Log(fmi3Error, "cppfmu", e.what());
        return fmi3Error;
    }
}


// ============================================================================
// Co-simulation functions
// ============================================================================


fmi3Status fmi3DoStep(
    fmi3Instance instance,
    fmi3Float64 currentCommunicationPoint,
    fmi3Float64 communicationStepSize,
    fmi3Boolean noSetFMUStatePriorToCurrentPoint,
    fmi3Boolean* eventHandlingNeeded,
    fmi3Boolean* terminateSimulation,
    fmi3Boolean* earlyReturn,
    fmi3Float64* lastSuccessfulTime)
{
    auto component = reinterpret_cast<Component*>(instance);
    try {
        fmi3Boolean eventNeeded = fmi3False;
        fmi3Boolean termSim = fmi3False;
        fmi3Boolean earlyRet = fmi3False;
        fmi3Float64 lastTime = currentCommunicationPoint + communicationStepSize;

        const auto ok = component->slave->DoStep(
            static_cast<cppfmu::FMIReal>(currentCommunicationPoint),
            static_cast<cppfmu::FMIReal>(communicationStepSize),
            noSetFMUStatePriorToCurrentPoint,
            eventNeeded,
            termSim,
            earlyRet,
            lastTime);

        if (eventHandlingNeeded) *eventHandlingNeeded = eventNeeded;
        if (terminateSimulation) *terminateSimulation = termSim;
        if (earlyReturn) *earlyReturn = earlyRet;
        if (lastSuccessfulTime) *lastSuccessfulTime = lastTime;

        component->lastSuccessfulTime = static_cast<cppfmu::FMIReal>(lastTime);

        return ok ? fmi3OK : fmi3Discard;
    } catch (const cppfmu::FatalError& e) {
        component->Log(fmi3Fatal, "cppfmu", e.what());
        return fmi3Fatal;
    } catch (const std::exception& e) {
        component->Log(fmi3Error, "cppfmu", e.what());
        return fmi3Error;
    }
}


// ============================================================================
// Unsupported stub functions
// ============================================================================


fmi3Status fmi3GetNumberOfEventIndicators(
    fmi3Instance instance,
    size_t* nEventIndicators)
{
    (void) instance;
    *nEventIndicators = 0;
    return fmi3OK;
}


fmi3Status fmi3GetNumberOfContinuousStates(
    fmi3Instance instance,
    size_t* nContinuousStates)
{
    (void) instance;
    *nContinuousStates = 0;
    return fmi3OK;
}


} // extern "C"
