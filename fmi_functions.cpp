/* Copyright 2016-2024, SINTEF Ocean.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
#include <exception>
#include <limits>
#include <string>

#include "cppfmu_cs.hpp"

namespace
{
// A struct that holds all the data for one model instance.
struct Component
{
    Component(
        cppfmu::FMIString instanceName,
        cppfmu::FMICallbackFunctions callbackFunctions,
        cppfmu::FMIBoolean loggingOn)
        : memory{callbackFunctions}
        , loggerSettings{std::make_shared<cppfmu::Logger::Settings>(memory)}
#ifdef CPPFMU_USE_FMI_1_0
        , logger{this, cppfmu::CopyString(memory, instanceName), callbackFunctions, loggerSettings}
#else
        , logger{callbackFunctions.componentEnvironment, cppfmu::CopyString(memory, instanceName), callbackFunctions, loggerSettings}
#endif
        , lastSuccessfulTime{std::numeric_limits<cppfmu::FMIReal>::quiet_NaN()}
    {
        loggerSettings->debugLoggingEnabled = (loggingOn == cppfmu::FMITrue);
    }

    // General
    cppfmu::Memory memory;
    std::shared_ptr<cppfmu::Logger::Settings> loggerSettings;
    cppfmu::Logger logger;

    // Co-simulation
    cppfmu::UniquePtr<cppfmu::SlaveInstance> slave;
    cppfmu::FMIReal lastSuccessfulTime;
};

// Runs one call on the slave behind the component handle, translating any
// exception into the matching FMI status code and logging its message.
//
// Centralises the cast, null-check, try/catch and logging that every
// uniform slave-delegating entry point would otherwise repeat.  Mirrors
// RunLegalCall() in fmi3_functions.cpp, minus the lifecycle state guard,
// which FMI 1.0/2.0 do not model.  A cppfmu::FatalError becomes FMIFatal;
// any other std::exception, or a throw carrying no std::exception, becomes
// FMIError instead of escaping across the C linkage boundary.
template<typename Operation>
cppfmu::FMIStatus RunCall(
    cppfmu::FMIComponent c,
    const char* functionName,
    Operation operation)
{
    const auto component = reinterpret_cast<Component*>(c);
    if (component == nullptr) {
        return cppfmu::FMIError;
    }
    try {
        operation(*component->slave);
        return cppfmu::FMIOK;
    } catch (const cppfmu::FatalError& e) {
        component->logger.Log(
            cppfmu::FMIFatal,
            "cppfmu",
            (std::string(functionName) + ": " + e.what()).c_str());
        return cppfmu::FMIFatal;
    } catch (const std::exception& e) {
        component->logger.Log(
            cppfmu::FMIError,
            "cppfmu",
            (std::string(functionName) + ": " + e.what()).c_str());
        return cppfmu::FMIError;
    } catch (...) {
        component->logger.Log(
            cppfmu::FMIError,
            "cppfmu",
            (std::string(functionName) + ": exception carrying no message")
                .c_str());
        return cppfmu::FMIError;
    }
}
} // namespace

// FMI functions
extern "C"
{
#ifdef CPPFMU_USE_FMI_1_0
// =============================================================================
// FMI 1.0 functions
// =============================================================================

const char* fmiGetTypesPlatform()
{
    return fmiPlatform;
}

const char* fmiGetVersion()
{
    return fmiVersion;
}

fmiComponent fmiInstantiateSlave(
    fmiString instanceName,
    fmiString fmuGUID,
    fmiString fmuLocation,
    fmiString mimeType,
    fmiReal timeout,
    fmiBoolean visible,
    fmiBoolean interactive,
    fmiCallbackFunctions functions,
    fmiBoolean loggingOn)
{
    try {
        auto component = cppfmu::AllocateUnique<Component>(
            cppfmu::Memory{functions},
            instanceName,
            functions,
            loggingOn);
        component->slave = CppfmuInstantiateSlave(
            instanceName,
            fmuGUID,
            fmuLocation,
            mimeType,
            timeout,
            visible,
            interactive,
            component->memory,
            component->logger);
        return component.release();
    } catch (const cppfmu::FatalError& e) {
        functions.logger(nullptr, instanceName, fmiFatal, "", e.what());
        return nullptr;
    } catch (const std::exception& e) {
        functions.logger(nullptr, instanceName, fmiError, "", e.what());
        return nullptr;
    }
}

void fmiFreeSlaveInstance(fmiComponent c)
{
    const auto component = reinterpret_cast<Component*>(c);
    // The Component object was allocated using cppfmu::AllocateUnique(),
    // which uses cppfmu::New() internally, so we use cppfmu::Delete() to
    // release it again.
    cppfmu::Delete(component->memory, component);
}

fmiStatus fmiInitializeSlave(
    fmiComponent c,
    fmiReal tStart,
    fmiBoolean stopTimeDefined,
    fmiReal tStop)
{
    return RunCall(c, "fmiInitializeSlave", [&](cppfmu::SlaveInstance& slave) {
        slave.SetupExperiment(fmiFalse, 0.0, tStart, stopTimeDefined, tStop);
        slave.EnterInitializationMode();
        slave.ExitInitializationMode();
    });
}

fmiStatus fmiResetSlave(fmiComponent c)
{
    return RunCall(c, "fmiResetSlave", [&](cppfmu::SlaveInstance& slave) {
        slave.Reset();
    });
}

fmiStatus fmiTerminateSlave(fmiComponent c)
{
    return RunCall(c, "fmiTerminateSlave", [&](cppfmu::SlaveInstance& slave) {
        slave.Terminate();
    });
}

fmiStatus fmiSetDebugLogging(fmiComponent c, fmiBoolean loggingOn)
{
    reinterpret_cast<Component*>(c)->loggerSettings->debugLoggingEnabled =
        (loggingOn == fmiTrue);
    return fmiOK;
}

fmiStatus fmiGetReal(
    fmiComponent c,
    const fmiValueReference vr[],
    size_t nvr,
    fmiReal value[])
{
    return RunCall(c, "fmiGetReal", [&](cppfmu::SlaveInstance& slave) {
        slave.GetReal(vr, nvr, value);
    });
}

fmiStatus fmiGetInteger(
    fmiComponent c,
    const fmiValueReference vr[],
    size_t nvr,
    fmiInteger value[])
{
    return RunCall(c, "fmiGetInteger", [&](cppfmu::SlaveInstance& slave) {
        slave.GetInteger(vr, nvr, value);
    });
}

fmiStatus fmiGetBoolean(
    fmiComponent c,
    const fmiValueReference vr[],
    size_t nvr,
    fmiBoolean value[])
{
    return RunCall(c, "fmiGetBoolean", [&](cppfmu::SlaveInstance& slave) {
        slave.GetBoolean(vr, nvr, value);
    });
}

fmiStatus fmiGetString(
    fmiComponent c,
    const fmiValueReference vr[],
    size_t nvr,
    fmiString value[])
{
    return RunCall(c, "fmiGetString", [&](cppfmu::SlaveInstance& slave) {
        slave.GetString(vr, nvr, value);
    });
}

fmiStatus fmiSetReal(
    fmiComponent c,
    const fmiValueReference vr[],
    size_t nvr,
    const fmiReal value[])
{
    return RunCall(c, "fmiSetReal", [&](cppfmu::SlaveInstance& slave) {
        slave.SetReal(vr, nvr, value);
    });
}

fmiStatus fmiSetInteger(
    fmiComponent c,
    const fmiValueReference vr[],
    size_t nvr,
    const fmiInteger value[])
{
    return RunCall(c, "fmiSetInteger", [&](cppfmu::SlaveInstance& slave) {
        slave.SetInteger(vr, nvr, value);
    });
}

fmiStatus fmiSetBoolean(
    fmiComponent c,
    const fmiValueReference vr[],
    size_t nvr,
    const fmiBoolean value[])
{
    return RunCall(c, "fmiSetBoolean", [&](cppfmu::SlaveInstance& slave) {
        slave.SetBoolean(vr, nvr, value);
    });
}

fmiStatus fmiSetString(
    fmiComponent c,
    const fmiValueReference vr[],
    size_t nvr,
    const fmiString value[])
{
    return RunCall(c, "fmiSetString", [&](cppfmu::SlaveInstance& slave) {
        slave.SetString(vr, nvr, value);
    });
}

fmiStatus fmiSetRealInputDerivatives(
    fmiComponent c,
    const fmiValueReference vr[],
    size_t nvr,
    const fmiInteger order[],
    const fmiReal value[])
{
    return RunCall(
        c,
        "fmiSetRealInputDerivatives",
        [&](cppfmu::SlaveInstance& slave) {
            slave.SetRealInputDerivatives(vr, nvr, order, value);
        });
}

fmiStatus fmiGetRealOutputDerivatives(
    fmiComponent c,
    const fmiValueReference vr[],
    size_t nvr,
    const fmiInteger order[],
    fmiReal value[])
{
    return RunCall(
        c,
        "fmiGetRealOutputDerivatives",
        [&](cppfmu::SlaveInstance& slave) {
            slave.GetRealOutputDerivatives(vr, nvr, order, value);
        });
}

fmiStatus fmiCancelStep(fmiComponent c)
{
    reinterpret_cast<Component*>(c)->logger.Log(
        fmiError,
        "cppfmu",
        "FMI function not supported: fmiCancelStep");
    return fmiError;
}

fmiStatus fmiDoStep(
    fmiComponent c,
    fmiReal currentCommunicationPoint,
    fmiReal communicationStepSize,
    fmiBoolean newStep)
{
    const auto component = reinterpret_cast<Component*>(c);
    try {
        double endTime = currentCommunicationPoint;
        const auto ok = component->slave->DoStep(
            currentCommunicationPoint,
            communicationStepSize,
            newStep,
            endTime);
        if (ok) {
            component->lastSuccessfulTime =
                currentCommunicationPoint + communicationStepSize;
            return fmiOK;
        } else {
            component->lastSuccessfulTime = endTime;
            return fmiDiscard;
        }
    } catch (const cppfmu::FatalError& e) {
        component->logger.Log(fmiFatal, "", e.what());
        return fmiFatal;
    } catch (const std::exception& e) {
        component->logger.Log(fmiError, "", e.what());
        return fmiError;
    }
}

fmiStatus fmiGetStatus(
    fmiComponent c,
    const fmiStatusKind /*s*/,
    fmiStatus* /*value*/)
{
    reinterpret_cast<Component*>(c)->logger.Log(
        fmiError,
        "cppfmu",
        "FMI function not supported: fmiGetStatus");
    return fmiError;
}

fmiStatus fmiGetRealStatus(
    fmiComponent c,
    const fmiStatusKind s,
    fmiReal* value)
{
    const auto component = reinterpret_cast<Component*>(c);
    if (s == fmiLastSuccessfulTime) {
        *value = component->lastSuccessfulTime;
        return fmiOK;
    } else {
        component->logger.Log(
            fmiError,
            "cppfmu",
            "Invalid status inquiry for fmiGetRealStatus");
        return fmiError;
    }
}

fmiStatus fmiGetIntegerStatus(
    fmiComponent c,
    const fmiStatusKind /*s*/,
    fmiInteger* /*value*/)
{
    reinterpret_cast<Component*>(c)->logger.Log(
        fmiError,
        "cppfmu",
        "FMI function not supported: fmiGetIntegerStatus");
    return fmiError;
}

fmiStatus fmiGetBooleanStatus(
    fmiComponent c,
    const fmiStatusKind /*s*/,
    fmiBoolean* /*value*/)
{
    reinterpret_cast<Component*>(c)->logger.Log(
        fmiError,
        "cppfmu",
        "FMI function not supported: fmiGetBooleanStatus");
    return fmiError;
}

fmiStatus fmiGetStringStatus(
    fmiComponent c,
    const fmiStatusKind /*s*/,
    fmiString* /*value*/)
{
    reinterpret_cast<Component*>(c)->logger.Log(
        fmiError,
        "cppfmu",
        "FMI function not supported: fmiGetStringStatus");
    return fmiError;
}

#else // CPPFMU_USE_FMI_1_0 not defined

// =============================================================================
// FMI 2.0 functions
// =============================================================================

const char* fmi2GetTypesPlatform()
{
    return fmi2TypesPlatform;
}

const char* fmi2GetVersion()
{
    return "2.0";
}

fmi2Component fmi2Instantiate(
    fmi2String instanceName,
    fmi2Type fmuType,
    fmi2String fmuGUID,
    fmi2String fmuResourceLocation,
    const fmi2CallbackFunctions* functions,
    fmi2Boolean visible,
    fmi2Boolean loggingOn)
{
    try {
        if (fmuType != fmi2CoSimulation) {
            throw std::logic_error(
                "Unsupported FMU instance type requested (only "
                "co-simulation is supported)");
        }
        auto component = cppfmu::AllocateUnique<Component>(
            cppfmu::Memory{*functions},
            instanceName,
            *functions,
            loggingOn);
        component->slave = CppfmuInstantiateSlave(
            instanceName,
            fmuGUID,
            fmuResourceLocation,
            "application/x-fmu-sharedlibrary",
            0.0,
            visible,
            cppfmu::FMIFalse,
            component->memory,
            component->logger);
        return component.release();
    } catch (const cppfmu::FatalError& e) {
        functions->logger(nullptr, instanceName, fmi2Fatal, "", e.what());
        return nullptr;
    } catch (const std::exception& e) {
        functions->logger(nullptr, instanceName, fmi2Error, "", e.what());
        return nullptr;
    }
}

void fmi2FreeInstance(fmi2Component c)
{
    const auto component = reinterpret_cast<Component*>(c);
    // The Component object was allocated using cppfmu::AllocateUnique(),
    // which uses cppfmu::New() internally, so we use cppfmu::Delete() to
    // release it again.
    cppfmu::Delete(component->memory, component);
}

fmi2Status fmi2SetDebugLogging(
    fmi2Component c,
    fmi2Boolean loggingOn,
    size_t nCategories,
    const fmi2String categories[])
{
    const auto component = reinterpret_cast<Component*>(c);

    std::vector<cppfmu::String, cppfmu::Allocator<cppfmu::String>>
        newCategories(cppfmu::Allocator<cppfmu::String>(component->memory));
    for (size_t i = 0; i < nCategories; ++i) {
        newCategories.push_back(
            cppfmu::CopyString(component->memory, categories[i]));
    }

    component->loggerSettings->debugLoggingEnabled = (loggingOn == fmi2True);
    component->loggerSettings->loggedCategories.swap(newCategories);
    return fmi2OK;
}

fmi2Status fmi2SetupExperiment(
    fmi2Component c,
    fmi2Boolean toleranceDefined,
    fmi2Real tolerance,
    fmi2Real startTime,
    fmi2Boolean stopTimeDefined,
    fmi2Real stopTime)
{
    return RunCall(c, "fmi2SetupExperiment", [&](cppfmu::SlaveInstance& slave) {
        slave.SetupExperiment(
            toleranceDefined,
            tolerance,
            startTime,
            stopTimeDefined,
            stopTime);
    });
}

fmi2Status fmi2EnterInitializationMode(fmi2Component c)
{
    return RunCall(
        c,
        "fmi2EnterInitializationMode",
        [&](cppfmu::SlaveInstance& slave) { slave.EnterInitializationMode(); });
}

fmi2Status fmi2ExitInitializationMode(fmi2Component c)
{
    return RunCall(
        c,
        "fmi2ExitInitializationMode",
        [&](cppfmu::SlaveInstance& slave) { slave.ExitInitializationMode(); });
}

fmi2Status fmi2Terminate(fmi2Component c)
{
    return RunCall(c, "fmi2Terminate", [&](cppfmu::SlaveInstance& slave) {
        slave.Terminate();
    });
}

fmi2Status fmi2Reset(fmi2Component c)
{
    return RunCall(c, "fmi2Reset", [&](cppfmu::SlaveInstance& slave) {
        slave.Reset();
    });
}

fmi2Status fmi2GetReal(
    fmi2Component c,
    const fmi2ValueReference vr[],
    size_t nvr,
    fmi2Real value[])
{
    return RunCall(c, "fmi2GetReal", [&](cppfmu::SlaveInstance& slave) {
        slave.GetReal(vr, nvr, value);
    });
}

fmi2Status fmi2GetInteger(
    fmi2Component c,
    const fmi2ValueReference vr[],
    size_t nvr,
    fmi2Integer value[])
{
    return RunCall(c, "fmi2GetInteger", [&](cppfmu::SlaveInstance& slave) {
        slave.GetInteger(vr, nvr, value);
    });
}

fmi2Status fmi2GetBoolean(
    fmi2Component c,
    const fmi2ValueReference vr[],
    size_t nvr,
    fmi2Boolean value[])
{
    return RunCall(c, "fmi2GetBoolean", [&](cppfmu::SlaveInstance& slave) {
        slave.GetBoolean(vr, nvr, value);
    });
}

fmi2Status fmi2GetString(
    fmi2Component c,
    const fmi2ValueReference vr[],
    size_t nvr,
    fmi2String value[])
{
    return RunCall(c, "fmi2GetString", [&](cppfmu::SlaveInstance& slave) {
        slave.GetString(vr, nvr, value);
    });
}

fmi2Status fmi2SetReal(
    fmi2Component c,
    const fmi2ValueReference vr[],
    size_t nvr,
    const fmi2Real value[])
{
    return RunCall(c, "fmi2SetReal", [&](cppfmu::SlaveInstance& slave) {
        slave.SetReal(vr, nvr, value);
    });
}

fmi2Status fmi2SetInteger(
    fmi2Component c,
    const fmi2ValueReference vr[],
    size_t nvr,
    const fmi2Integer value[])
{
    return RunCall(c, "fmi2SetInteger", [&](cppfmu::SlaveInstance& slave) {
        slave.SetInteger(vr, nvr, value);
    });
}

fmi2Status fmi2SetBoolean(
    fmi2Component c,
    const fmi2ValueReference vr[],
    size_t nvr,
    const fmi2Boolean value[])
{
    return RunCall(c, "fmi2SetBoolean", [&](cppfmu::SlaveInstance& slave) {
        slave.SetBoolean(vr, nvr, value);
    });
}

fmi2Status fmi2SetString(
    fmi2Component c,
    const fmi2ValueReference vr[],
    size_t nvr,
    const fmi2String value[])
{
    return RunCall(c, "fmi2SetString", [&](cppfmu::SlaveInstance& slave) {
        slave.SetString(vr, nvr, value);
    });
}

fmi2Status fmi2GetFMUstate(fmi2Component c, fmi2FMUstate* state)
{
    return RunCall(c, "fmi2GetFMUstate", [&](cppfmu::SlaveInstance& slave) {
        slave.GetFMUState(state);
    });
}

fmi2Status fmi2SetFMUstate(fmi2Component c, fmi2FMUstate state)
{
    return RunCall(c, "fmi2SetFMUstate", [&](cppfmu::SlaveInstance& slave) {
        slave.SetFMUState(state);
    });
}

fmi2Status fmi2FreeFMUstate(fmi2Component c, fmi2FMUstate* state)
{
    if (state == nullptr) return fmi2OK;
    return RunCall(c, "fmi2FreeFMUstate", [&](cppfmu::SlaveInstance& slave) {
        slave.FreeFMUState(*state);
        *state = nullptr;
    });
}

fmi2Status fmi2SerializedFMUstateSize(
    fmi2Component c,
    fmi2FMUstate state,
    size_t* size)
{
    return RunCall(
        c,
        "fmi2SerializedFMUstateSize",
        [&](cppfmu::SlaveInstance& slave) {
            *size = slave.SerializedFMUStateSize(state);
        });
}

fmi2Status fmi2SerializeFMUstate(
    fmi2Component c,
    fmi2FMUstate state,
    fmi2Byte data[],
    size_t size)
{
    return RunCall(
        c,
        "fmi2SerializeFMUstate",
        [&](cppfmu::SlaveInstance& slave) {
            slave.SerializeFMUState(state, data, size);
        });
}

fmi2Status fmi2DeSerializeFMUstate(
    fmi2Component c,
    const fmi2Byte data[],
    size_t size,
    fmi2FMUstate* state)
{
    return RunCall(
        c,
        "fmi2DeSerializeFMUstate",
        [&](cppfmu::SlaveInstance& slave) {
            *state = slave.DeserializeFMUState(data, size);
        });
}

fmi2Status fmi2GetDirectionalDerivative(
    fmi2Component c,
    const fmi2ValueReference vUnknown_ref[],
    size_t nUnknown,
    const fmi2ValueReference vKnown_ref[],
    size_t nKnown,
    const fmi2Real dvKnown[],
    fmi2Real dvUnknown[])
{
    return RunCall(
        c,
        "fmi2GetDirectionalDerivative",
        [&](cppfmu::SlaveInstance& slave) {
            slave.GetDirectionalDerivative(
                vUnknown_ref,
                nUnknown,
                vKnown_ref,
                nKnown,
                dvKnown,
                dvUnknown);
        });
}

fmi2Status fmi2SetRealInputDerivatives(
    fmi2Component c,
    const fmi2ValueReference vr[],
    size_t nvr,
    const fmi2Integer order[],
    const fmi2Real value[])
{
    return RunCall(
        c,
        "fmi2SetRealInputDerivatives",
        [&](cppfmu::SlaveInstance& slave) {
            slave.SetRealInputDerivatives(vr, nvr, order, value);
        });
}

fmi2Status fmi2GetRealOutputDerivatives(
    fmi2Component c,
    const fmi2ValueReference vr[],
    size_t nvr,
    const fmi2Integer order[],
    fmi2Real value[])
{
    return RunCall(
        c,
        "fmi2GetRealOutputDerivatives",
        [&](cppfmu::SlaveInstance& slave) {
            slave.GetRealOutputDerivatives(vr, nvr, order, value);
        });
}

fmi2Status fmi2DoStep(
    fmi2Component c,
    fmi2Real currentCommunicationPoint,
    fmi2Real communicationStepSize,
    fmi2Boolean /*noSetFMUStatePriorToCurrentPoint*/)
{
    const auto component = reinterpret_cast<Component*>(c);
    try {
        double endTime = currentCommunicationPoint;
        const auto ok = component->slave->DoStep(
            currentCommunicationPoint,
            communicationStepSize,
            fmi2True,
            endTime);
        if (ok) {
            component->lastSuccessfulTime =
                currentCommunicationPoint + communicationStepSize;
            return fmi2OK;
        } else {
            component->lastSuccessfulTime = endTime;
            return fmi2Discard;
        }
    } catch (const cppfmu::FatalError& e) {
        component->logger.Log(fmi2Fatal, "", e.what());
        return fmi2Fatal;
    } catch (const std::exception& e) {
        component->logger.Log(fmi2Error, "", e.what());
        return fmi2Error;
    }
}

fmi2Status fmi2CancelStep(fmi2Component c)
{
    reinterpret_cast<Component*>(c)->logger.Log(
        fmi2Error,
        "cppfmu",
        "FMI function not supported: fmi2CancelStep");
    return fmi2Error;
}

/* Inquire slave status */
fmi2Status fmi2GetStatus(fmi2Component c, const fmi2StatusKind, fmi2Status*)
{
    reinterpret_cast<Component*>(c)->logger.Log(
        fmi2Error,
        "cppfmu",
        "FMI function not supported: fmi2GetStatus");
    return fmi2Error;
}

fmi2Status fmi2GetRealStatus(
    fmi2Component c,
    const fmi2StatusKind s,
    fmi2Real* value)
{
    const auto component = reinterpret_cast<Component*>(c);
    if (s == fmi2LastSuccessfulTime) {
        *value = component->lastSuccessfulTime;
        return fmi2OK;
    } else {
        component->logger.Log(
            fmi2Error,
            "cppfmu",
            "Invalid status inquiry for fmi2GetRealStatus");
        return fmi2Error;
    }
}

fmi2Status fmi2GetIntegerStatus(
    fmi2Component c,
    const fmi2StatusKind,
    fmi2Integer*)
{
    reinterpret_cast<Component*>(c)->logger.Log(
        fmi2Error,
        "cppfmu",
        "FMI function not supported: fmi2GetIntegerStatus");
    return fmi2Error;
}

fmi2Status fmi2GetBooleanStatus(
    fmi2Component c,
    const fmi2StatusKind,
    fmi2Boolean*)
{
    reinterpret_cast<Component*>(c)->logger.Log(
        fmi2Error,
        "cppfmu",
        "FMI function not supported: fmi2GetBooleanStatus");
    return fmi2Error;
}

fmi2Status fmi2GetStringStatus(
    fmi2Component c,
    const fmi2StatusKind,
    fmi2String*)
{
    reinterpret_cast<Component*>(c)->logger.Log(
        fmi2Error,
        "cppfmu",
        "FMI function not supported: fmi2GetStringStatus");
    return fmi2Error;
}

#endif // CPPFMU_USE_FMI_1_0
}
