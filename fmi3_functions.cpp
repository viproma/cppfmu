/* Copyright 2016-2026, SINTEF Ocean.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
#include <exception>
#include <functional>
#include <limits>
#include <memory>

#include "cppfmu_cs_fmi3.hpp"


namespace
{
    struct Component
    {
        Component(
            cppfmu::FMIComponentEnvironment instanceEnvironment,
            fmi3LogMessageCallback logMessage,
            cppfmu::FMIBoolean loggingOn)
            : instanceEnvironment{instanceEnvironment}
            , logMessage{logMessage}
            , debugLoggingEnabled{loggingOn == fmi3True}
            , lastSuccessfulTime{std::numeric_limits<cppfmu::FMIReal>::quiet_NaN()}
        {
        }

        bool IsCategoryLogged(const std::string& category) const
        {
            if (loggedCategories.empty()) return true;
            for (const auto& c : loggedCategories) {
                if (c == category) return true;
            }
            return false;
        }

        void Log(fmi3Status status, cppfmu::FMIString category, cppfmu::FMIString message) const
        {
            if ((status == fmi3Fatal || status == fmi3Error || debugLoggingEnabled) &&
                IsCategoryLogged(category)) {
                if (logMessage) {
                    logMessage(instanceEnvironment, status, category, message);
                }
            }
        }

        cppfmu::FMIComponentEnvironment instanceEnvironment;
        fmi3LogMessageCallback logMessage;
        bool debugLoggingEnabled;
        std::vector<std::string> loggedCategories;
        cppfmu::UniquePtr<cppfmu::SlaveInstance3> slave;
        cppfmu::FMIReal lastSuccessfulTime;
    };
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
    auto component = reinterpret_cast<Component*>(instance);
    component->debugLoggingEnabled = (loggingOn == fmi3True);
    component->loggedCategories.clear();
    for (size_t i = 0; i < nCategories; ++i) {
        component->loggedCategories.emplace_back(categories[i]);
    }
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
    std::unique_ptr<Component> component;
    try {
        component.reset(new Component(
            instanceEnvironment, logMessage, loggingOn));

        auto* comp = component.get();
        auto loggerFn = [comp](cppfmu::FMIStatus status, cppfmu::FMIString category, cppfmu::FMIString message) {
            comp->Log(status, category, message);
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

        return component.release();
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
    if (instance == nullptr) return;
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
    auto component = reinterpret_cast<Component*>(instance);
    try {
        *nEventIndicators = component->slave->GetNumberOfEventIndicators();
        return fmi3OK;
    } catch (const cppfmu::FatalError& e) {
        component->Log(fmi3Fatal, "cppfmu", e.what());
        return fmi3Fatal;
    } catch (const std::exception& e) {
        component->Log(fmi3Error, "cppfmu", e.what());
        return fmi3Error;
    }
}


fmi3Status fmi3GetNumberOfContinuousStates(
    fmi3Instance instance,
    size_t* nContinuousStates)
{
    auto component = reinterpret_cast<Component*>(instance);
    try {
        *nContinuousStates = component->slave->GetNumberOfContinuousStates();
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
// Configuration mode stubs
// ============================================================================


fmi3Status fmi3EnterConfigurationMode(fmi3Instance instance)
{
    auto component = reinterpret_cast<Component*>(instance);
    component->Log(fmi3Error, "cppfmu", "FMI function not supported: fmi3EnterConfigurationMode");
    return fmi3Error;
}


fmi3Status fmi3ExitConfigurationMode(fmi3Instance instance)
{
    auto component = reinterpret_cast<Component*>(instance);
    component->Log(fmi3Error, "cppfmu", "FMI function not supported: fmi3ExitConfigurationMode");
    return fmi3Error;
}


// ============================================================================
// Discrete states stubs
// ============================================================================


fmi3Status fmi3EvaluateDiscreteStates(fmi3Instance instance)
{
    auto component = reinterpret_cast<Component*>(instance);
    component->Log(fmi3Error, "cppfmu", "FMI function not supported: fmi3EvaluateDiscreteStates");
    return fmi3Error;
}


fmi3Status fmi3UpdateDiscreteStates(
    fmi3Instance instance,
    fmi3Boolean* discreteStatesNeedUpdate,
    fmi3Boolean* terminateSimulation,
    fmi3Boolean* nominalsOfContinuousStatesChanged,
    fmi3Boolean* valuesOfContinuousStatesChanged,
    fmi3Boolean* nextEventTimeDefined,
    fmi3Float64* nextEventTime)
{
    auto component = reinterpret_cast<Component*>(instance);
    component->Log(fmi3Error, "cppfmu", "FMI function not supported: fmi3UpdateDiscreteStates");
    if (discreteStatesNeedUpdate) *discreteStatesNeedUpdate = fmi3False;
    if (terminateSimulation) *terminateSimulation = fmi3False;
    if (nominalsOfContinuousStatesChanged) *nominalsOfContinuousStatesChanged = fmi3False;
    if (valuesOfContinuousStatesChanged) *valuesOfContinuousStatesChanged = fmi3False;
    if (nextEventTimeDefined) *nextEventTimeDefined = fmi3False;
    if (nextEventTime) *nextEventTime = 0.0;
    return fmi3Error;
}


// ============================================================================
// Clock interval/shift stubs
// ============================================================================


fmi3Status fmi3GetIntervalDecimal(
    fmi3Instance instance,
    const fmi3ValueReference clocks[],
    size_t nClocks,
    fmi3Float64 intervals[],
    fmi3IntervalQualifier qualifiers[])
{
    auto component = reinterpret_cast<Component*>(instance);
    component->Log(fmi3Error, "cppfmu", "FMI function not supported: fmi3GetIntervalDecimal");
    for (size_t i = 0; i < nClocks; ++i) {
        if (intervals) intervals[i] = 0.0;
        if (qualifiers) qualifiers[i] = fmi3IntervalUnchanged;
    }
    return fmi3Error;
}


fmi3Status fmi3GetIntervalFraction(
    fmi3Instance instance,
    const fmi3ValueReference clocks[],
    size_t nClocks,
    fmi3UInt64 intervalCounters[],
    fmi3UInt64 resolutions[],
    fmi3IntervalQualifier qualifiers[])
{
    auto component = reinterpret_cast<Component*>(instance);
    component->Log(fmi3Error, "cppfmu", "FMI function not supported: fmi3GetIntervalFraction");
    for (size_t i = 0; i < nClocks; ++i) {
        if (intervalCounters) intervalCounters[i] = 0;
        if (resolutions) resolutions[i] = 1;
        if (qualifiers) qualifiers[i] = fmi3IntervalUnchanged;
    }
    return fmi3Error;
}


fmi3Status fmi3GetShiftDecimal(
    fmi3Instance instance,
    const fmi3ValueReference clocks[],
    size_t nClocks,
    fmi3Float64 shifts[])
{
    auto component = reinterpret_cast<Component*>(instance);
    component->Log(fmi3Error, "cppfmu", "FMI function not supported: fmi3GetShiftDecimal");
    for (size_t i = 0; i < nClocks; ++i) {
        if (shifts) shifts[i] = 0.0;
    }
    return fmi3Error;
}


fmi3Status fmi3GetShiftFraction(
    fmi3Instance instance,
    const fmi3ValueReference clocks[],
    size_t nClocks,
    fmi3UInt64 shiftCounters[],
    fmi3UInt64 resolutions[])
{
    auto component = reinterpret_cast<Component*>(instance);
    component->Log(fmi3Error, "cppfmu", "FMI function not supported: fmi3GetShiftFraction");
    for (size_t i = 0; i < nClocks; ++i) {
        if (shiftCounters) shiftCounters[i] = 0;
        if (resolutions) resolutions[i] = 1;
    }
    return fmi3Error;
}


fmi3Status fmi3SetIntervalDecimal(
    fmi3Instance instance,
    const fmi3ValueReference clocks[],
    size_t nClocks,
    const fmi3Float64 intervals[])
{
    auto component = reinterpret_cast<Component*>(instance);
    component->Log(fmi3Error, "cppfmu", "FMI function not supported: fmi3SetIntervalDecimal");
    return fmi3Error;
}


fmi3Status fmi3SetIntervalFraction(
    fmi3Instance instance,
    const fmi3ValueReference clocks[],
    size_t nClocks,
    const fmi3UInt64 intervalCounters[],
    const fmi3UInt64 resolutions[])
{
    auto component = reinterpret_cast<Component*>(instance);
    component->Log(fmi3Error, "cppfmu", "FMI function not supported: fmi3SetIntervalFraction");
    return fmi3Error;
}


fmi3Status fmi3SetShiftDecimal(
    fmi3Instance instance,
    const fmi3ValueReference clocks[],
    size_t nClocks,
    const fmi3Float64 shifts[])
{
    auto component = reinterpret_cast<Component*>(instance);
    component->Log(fmi3Error, "cppfmu", "FMI function not supported: fmi3SetShiftDecimal");
    return fmi3Error;
}


fmi3Status fmi3SetShiftFraction(
    fmi3Instance instance,
    const fmi3ValueReference clocks[],
    size_t nClocks,
    const fmi3UInt64 shiftCounters[],
    const fmi3UInt64 resolutions[])
{
    auto component = reinterpret_cast<Component*>(instance);
    component->Log(fmi3Error, "cppfmu", "FMI function not supported: fmi3SetShiftFraction");
    return fmi3Error;
}


} // extern "C"
