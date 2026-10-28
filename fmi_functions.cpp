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
#ifndef CPPFMU_USE_FMI_1_0
/* The FMI 2.0 Co-Simulation states this wrapper tells apart (4.2.4).
 *
 * The tracking is lenient: it refuses only calls that FMI 2.0 allows in none
 * of the states the instance can be in, so it never refuses a legal call.
 * - stepFailed (after fmi2Discard), error and fatal are not tracked. The text
 *   and the state table of FMI 2.0 disagree on what may follow them, and an
 *   FMU may only refuse what is clearly illegal. A Discard leaves the
 *   instance in StepComplete, and an error or a fatal error leaves it where
 *   it was.
 * - stepInProgress and stepCanceled do not occur, since DoStep is
 *   synchronous.
 * - Restored follows fmi2SetFMUstate. The state may have been taken in any
 *   state, and FMI 2.0 FMU states do not record which, so every call is
 *   allowed until the next one that moves the instance to a known state.
 *   An importer can then restore a state into a fresh instance and step on
 *   (fmusim --initial-fmu-state-file, FMPy's fmu_state).
 * A call FMI 2.0 does not allow has undefined behaviour (3.2.3), so refusing
 * it with fmi2Error is the FMU's choice.
 */
enum class Mode
{
    Instantiated,
    InitializationMode,
    StepComplete,
    Terminated,
    Restored
};

// The calls whose legality depends on the mode, grouped by the columns of the
// FMI 2.0 table of allowed calls (4.2.4). fmi2FreeInstance,
// fmi2SetDebugLogging and the FMU state functions other than fmi2GetFMUstate
// are allowed in every tracked mode and are not guarded.
enum class Call
{
    SetupExperiment,
    EnterInitializationMode,
    ExitInitializationMode,
    Terminate,
    Reset,
    GetVariable,   // fmi2Get{Real,Integer,Boolean,String}
    SetVariable,   // fmi2Set{Real,Integer,Boolean,String}
    GetFMUstate,   // allowed in every tracked mode
    SetFMUstate,   // allowed in every tracked mode; leads to Restored
    OtherFMUstate, // free and (de)serialize; allowed in every mode
    DirectionalDerivative,
    SetInputDerivatives,
    GetOutputDerivatives,
    DoStep,
    GetStatus // fmi2Get{Real,Boolean}Status
};

bool Allows(Mode mode, Call call)
{
    if (mode == Mode::Restored) return true;
    const bool inst = mode == Mode::Instantiated;
    const bool init = mode == Mode::InitializationMode;
    const bool step = mode == Mode::StepComplete;
    const bool term = mode == Mode::Terminated;
    switch (call) {
    case Call::SetupExperiment:
    case Call::EnterInitializationMode:
        return inst;
    case Call::ExitInitializationMode:
        return init;
    case Call::Terminate:
    case Call::DoStep:
        return step;
    case Call::GetVariable:
    case Call::DirectionalDerivative:
        return init || step || term;
    case Call::SetVariable:
    case Call::SetInputDerivatives:
        return inst || init || step;
    case Call::GetOutputDerivatives:
    case Call::GetStatus:
        return step || term;
    case Call::Reset:
    case Call::GetFMUstate:
    case Call::SetFMUstate:
    case Call::OtherFMUstate:
        return true;
    }
    return false;
}

// The mode after a successful call. Other calls leave the mode as it was.
Mode After(Mode mode, Call call)
{
    switch (call) {
    case Call::EnterInitializationMode:
        return Mode::InitializationMode;
    case Call::ExitInitializationMode:
    case Call::DoStep:
        return Mode::StepComplete;
    case Call::Terminate:
        return Mode::Terminated;
    case Call::Reset:
        return Mode::Instantiated;
    case Call::SetFMUstate:
        return Mode::Restored;
    default:
        return mode;
    }
}
#endif

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
#ifndef CPPFMU_USE_FMI_1_0
    Mode mode = Mode::Instantiated;
#endif
};

/* Returns 'text' as a printf format that prints 'text': every '%' doubled.
 *
 * The FMI 1.0/2.0 logger takes its message as a printf format. Text that may
 * contain '%' (an exception message) is passed as the format in this form,
 * not as a "%s" argument: some importers (FMPy) print the format without
 * expanding the arguments, and would show "%s" instead of the text. They show
 * a '%' in the text as "%%" instead.
 */
std::string AsFormat(const std::string& text)
{
    std::string format;
    format.reserve(text.size());
    for (const char ch : text) {
        format += ch;
        if (ch == '%') format += '%';
    }
    return format;
}

// Logs "functionName: message" with CPPFMU's category.
void LogFailure(
    Component& component,
    cppfmu::FMIStatus status,
    const char* functionName,
    const char* message) CPPFMU_NOEXCEPT
{
    try {
        const auto format =
            AsFormat(std::string(functionName) + ": " + message);
        component.logger.Log(status, CPPFMU_LOG_CATEGORY, format.c_str());
    } catch (...) {
        // There is nothing left to report a failed report with.
    }
}

/* Logs why an instantiation failed. There is no Component yet, so this goes
 * to the importer's logger directly.
 */
template<typename LoggerCallback, typename Environment>
void LogInstantiationFailure(
    LoggerCallback logger,
    Environment environment,
    cppfmu::FMIString instanceName,
    cppfmu::FMIStatus status,
    const char* message) CPPFMU_NOEXCEPT
{
    if (logger == nullptr) return;
    try {
        const auto format = AsFormat(message);
        logger(
            environment,
            instanceName,
            status,
            CPPFMU_LOG_CATEGORY,
            format.c_str());
    } catch (...) {
        // There is nothing left to report a failed report with.
    }
}

// Runs one call on the slave behind the component handle, translating any
// exception into the matching FMI status code and logging its message.
//
// Centralises the cast, null-check, try/catch and logging that every
// uniform slave-delegating entry point would otherwise repeat.  A
// cppfmu::FatalError becomes FMIFatal. A cppfmu::DiscardError becomes
// FMIDiscard where the caller says the function may return it
// (discardAllowed), and FMIError elsewhere. Any other std::exception, or a
// throw carrying no std::exception, becomes FMIError instead of escaping
// across the C linkage boundary.
template<typename Operation>
cppfmu::FMIStatus RunCall(
    Component& component,
    const char* functionName,
    Operation operation,
    bool discardAllowed = false)
{
    try {
        operation(*component.slave);
        return cppfmu::FMIOK;
    } catch (const cppfmu::FatalError& e) {
        LogFailure(component, cppfmu::FMIFatal, functionName, e.what());
        return cppfmu::FMIFatal;
    } catch (const cppfmu::DiscardError& e) {
        const auto status =
            discardAllowed ? cppfmu::FMIDiscard : cppfmu::FMIError;
        LogFailure(component, status, functionName, e.what());
        return status;
    } catch (const std::exception& e) {
        LogFailure(component, cppfmu::FMIError, functionName, e.what());
        return cppfmu::FMIError;
    } catch (...) {
        LogFailure(
            component,
            cppfmu::FMIError,
            functionName,
            "exception carrying no message");
        return cppfmu::FMIError;
    }
}

#ifdef CPPFMU_USE_FMI_1_0
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
    return RunCall(*component, functionName, operation);
}
#else
/* Refuses a call FMI 2.0 does not allow in the instance's mode (see Mode). A
 * refused call never reaches the slave and changes nothing.
 */
bool RefuseIllegalCall(
    Component& component,
    const char* functionName,
    Call call) CPPFMU_NOEXCEPT
{
    if (Allows(component.mode, call)) return false;
    LogFailure(
        component,
        cppfmu::FMIError,
        functionName,
        "was called in a state that does not allow it");
    return true;
}

// RunCall() for FMI 2.0: the call runs only if the instance's mode allows it,
// and a successful call moves the instance to its next mode.  Mirrors
// RunLegalCall() in fmi3_functions.cpp.  Of these calls, only the variable
// setters may return fmi2Discard in Co-Simulation (2.1.3).
template<typename Operation>
cppfmu::FMIStatus RunCall(
    cppfmu::FMIComponent c,
    const char* functionName,
    Call call,
    Operation operation)
{
    const auto component = reinterpret_cast<Component*>(c);
    if (component == nullptr) {
        return cppfmu::FMIError;
    }
    if (RefuseIllegalCall(*component, functionName, call)) {
        return cppfmu::FMIError;
    }
    const auto status = RunCall(
        *component,
        functionName,
        operation,
        call == Call::SetVariable);
    if (status == cppfmu::FMIOK) {
        component->mode = After(component->mode, call);
    }
    return status;
}
#endif
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
        LogInstantiationFailure(
            functions.logger,
            nullptr,
            instanceName,
            fmiFatal,
            e.what());
        return nullptr;
    } catch (const std::exception& e) {
        LogInstantiationFailure(
            functions.logger,
            nullptr,
            instanceName,
            fmiError,
            e.what());
        return nullptr;
    } catch (...) {
        LogInstantiationFailure(
            functions.logger,
            nullptr,
            instanceName,
            fmiError,
            "exception carrying no message");
        return nullptr;
    }
}

void fmiFreeSlaveInstance(fmiComponent c)
{
    const auto component = reinterpret_cast<Component*>(c);
    if (component == nullptr) return;
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
        CPPFMU_LOG_CATEGORY,
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
    if (component == nullptr) return fmiError;
    bool completed = false;
    // A DiscardError means the slave did not step at all.
    component->lastSuccessfulTime = currentCommunicationPoint;
    const auto status = RunCall(
        *component,
        "fmiDoStep",
        [&](cppfmu::SlaveInstance& slave) {
            double endTime = currentCommunicationPoint;
            completed = slave.DoStep(
                currentCommunicationPoint,
                communicationStepSize,
                newStep,
                endTime);
            component->lastSuccessfulTime = completed
                ? currentCommunicationPoint + communicationStepSize
                : endTime;
        },
        true);
    if (status != fmiOK) return status;
    return completed ? fmiOK : fmiDiscard;
}

fmiStatus fmiGetStatus(
    fmiComponent c,
    const fmiStatusKind /*s*/,
    fmiStatus* /*value*/)
{
    reinterpret_cast<Component*>(c)->logger.Log(
        fmiError,
        CPPFMU_LOG_CATEGORY,
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
            CPPFMU_LOG_CATEGORY,
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
        CPPFMU_LOG_CATEGORY,
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
        CPPFMU_LOG_CATEGORY,
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
        CPPFMU_LOG_CATEGORY,
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
    if (functions == nullptr) return nullptr;
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
        if (loggingOn == fmi2True) {
            LogInstantiationFailure(
                functions->logger,
                functions->componentEnvironment,
                instanceName,
                fmi2Fatal,
                e.what());
        }
        return nullptr;
    } catch (const std::exception& e) {
        if (loggingOn == fmi2True) {
            LogInstantiationFailure(
                functions->logger,
                functions->componentEnvironment,
                instanceName,
                fmi2Error,
                e.what());
        }
        return nullptr;
    } catch (...) {
        if (loggingOn == fmi2True) {
            LogInstantiationFailure(
                functions->logger,
                functions->componentEnvironment,
                instanceName,
                fmi2Error,
                "exception carrying no message");
        }
        return nullptr;
    }
}

void fmi2FreeInstance(fmi2Component c)
{
    // "If a null pointer is provided for c, the function call is ignored"
    // (FMI 2.0, 2.1.5).
    const auto component = reinterpret_cast<Component*>(c);
    if (component == nullptr) return;
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
    return RunCall(
        c,
        "fmi2SetupExperiment",
        Call::SetupExperiment,
        [&](cppfmu::SlaveInstance& slave) {
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
        Call::EnterInitializationMode,
        [&](cppfmu::SlaveInstance& slave) { slave.EnterInitializationMode(); });
}

fmi2Status fmi2ExitInitializationMode(fmi2Component c)
{
    return RunCall(
        c,
        "fmi2ExitInitializationMode",
        Call::ExitInitializationMode,
        [&](cppfmu::SlaveInstance& slave) { slave.ExitInitializationMode(); });
}

fmi2Status fmi2Terminate(fmi2Component c)
{
    return RunCall(
        c,
        "fmi2Terminate",
        Call::Terminate,
        [&](cppfmu::SlaveInstance& slave) { slave.Terminate(); });
}

fmi2Status fmi2Reset(fmi2Component c)
{
    return RunCall(
        c,
        "fmi2Reset",
        Call::Reset,
        [&](cppfmu::SlaveInstance& slave) { slave.Reset(); });
}

fmi2Status fmi2GetReal(
    fmi2Component c,
    const fmi2ValueReference vr[],
    size_t nvr,
    fmi2Real value[])
{
    return RunCall(
        c,
        "fmi2GetReal",
        Call::GetVariable,
        [&](cppfmu::SlaveInstance& slave) { slave.GetReal(vr, nvr, value); });
}

fmi2Status fmi2GetInteger(
    fmi2Component c,
    const fmi2ValueReference vr[],
    size_t nvr,
    fmi2Integer value[])
{
    return RunCall(
        c,
        "fmi2GetInteger",
        Call::GetVariable,
        [&](cppfmu::SlaveInstance& slave) {
            slave.GetInteger(vr, nvr, value);
        });
}

fmi2Status fmi2GetBoolean(
    fmi2Component c,
    const fmi2ValueReference vr[],
    size_t nvr,
    fmi2Boolean value[])
{
    return RunCall(
        c,
        "fmi2GetBoolean",
        Call::GetVariable,
        [&](cppfmu::SlaveInstance& slave) {
            slave.GetBoolean(vr, nvr, value);
        });
}

fmi2Status fmi2GetString(
    fmi2Component c,
    const fmi2ValueReference vr[],
    size_t nvr,
    fmi2String value[])
{
    return RunCall(
        c,
        "fmi2GetString",
        Call::GetVariable,
        [&](cppfmu::SlaveInstance& slave) { slave.GetString(vr, nvr, value); });
}

fmi2Status fmi2SetReal(
    fmi2Component c,
    const fmi2ValueReference vr[],
    size_t nvr,
    const fmi2Real value[])
{
    return RunCall(
        c,
        "fmi2SetReal",
        Call::SetVariable,
        [&](cppfmu::SlaveInstance& slave) { slave.SetReal(vr, nvr, value); });
}

fmi2Status fmi2SetInteger(
    fmi2Component c,
    const fmi2ValueReference vr[],
    size_t nvr,
    const fmi2Integer value[])
{
    return RunCall(
        c,
        "fmi2SetInteger",
        Call::SetVariable,
        [&](cppfmu::SlaveInstance& slave) {
            slave.SetInteger(vr, nvr, value);
        });
}

fmi2Status fmi2SetBoolean(
    fmi2Component c,
    const fmi2ValueReference vr[],
    size_t nvr,
    const fmi2Boolean value[])
{
    return RunCall(
        c,
        "fmi2SetBoolean",
        Call::SetVariable,
        [&](cppfmu::SlaveInstance& slave) {
            slave.SetBoolean(vr, nvr, value);
        });
}

fmi2Status fmi2SetString(
    fmi2Component c,
    const fmi2ValueReference vr[],
    size_t nvr,
    const fmi2String value[])
{
    return RunCall(
        c,
        "fmi2SetString",
        Call::SetVariable,
        [&](cppfmu::SlaveInstance& slave) { slave.SetString(vr, nvr, value); });
}

fmi2Status fmi2GetFMUstate(fmi2Component c, fmi2FMUstate* state)
{
    return RunCall(
        c,
        "fmi2GetFMUstate",
        Call::GetFMUstate,
        [&](cppfmu::SlaveInstance& slave) { slave.GetFMUState(state); });
}

fmi2Status fmi2SetFMUstate(fmi2Component c, fmi2FMUstate state)
{
    return RunCall(
        c,
        "fmi2SetFMUstate",
        Call::SetFMUstate,
        [&](cppfmu::SlaveInstance& slave) { slave.SetFMUState(state); });
}

fmi2Status fmi2FreeFMUstate(fmi2Component c, fmi2FMUstate* state)
{
    // "If a null pointer is provided, the call is ignored" (FMI 2.0, 2.1.8).
    if (state == nullptr || *state == nullptr) return fmi2OK;
    return RunCall(
        c,
        "fmi2FreeFMUstate",
        Call::OtherFMUstate,
        [&](cppfmu::SlaveInstance& slave) {
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
        Call::OtherFMUstate,
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
        Call::OtherFMUstate,
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
        Call::OtherFMUstate,
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
        Call::DirectionalDerivative,
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
        Call::SetInputDerivatives,
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
        Call::GetOutputDerivatives,
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
    if (component == nullptr) return fmi2Error;
    if (RefuseIllegalCall(*component, "fmi2DoStep", Call::DoStep)) {
        return fmi2Error;
    }
    // FMI 2.0 requires a positive step (4.2.2). A NaN one is refused too, before
    // it reaches the slave.
    if (!(communicationStepSize > 0.0)) {
        LogFailure(
            *component,
            fmi2Error,
            "fmi2DoStep",
            "needs a communicationStepSize greater than zero");
        return fmi2Error;
    }
    bool completed = false;
    // A DiscardError means the slave did not step at all.
    component->lastSuccessfulTime = currentCommunicationPoint;
    const auto status = RunCall(
        *component,
        "fmi2DoStep",
        [&](cppfmu::SlaveInstance& slave) {
            double endTime = currentCommunicationPoint;
            completed = slave.DoStep(
                currentCommunicationPoint,
                communicationStepSize,
                fmi2True,
                endTime);
            component->lastSuccessfulTime = completed
                ? currentCommunicationPoint + communicationStepSize
                : endTime;
        },
        true);
    if (status != fmi2OK && status != fmi2Discard) return status;
    // A discarded step leaves the instance in stepComplete too (see Mode).
    component->mode = After(component->mode, Call::DoStep);
    return completed ? fmi2OK : fmi2Discard;
}

fmi2Status fmi2CancelStep(fmi2Component c)
{
    reinterpret_cast<Component*>(c)->logger.Log(
        fmi2Error,
        CPPFMU_LOG_CATEGORY,
        "FMI function not supported: fmi2CancelStep");
    return fmi2Error;
}

/* Inquire slave status */
fmi2Status fmi2GetStatus(fmi2Component c, const fmi2StatusKind, fmi2Status*)
{
    reinterpret_cast<Component*>(c)->logger.Log(
        fmi2Error,
        CPPFMU_LOG_CATEGORY,
        "FMI function not supported: fmi2GetStatus");
    return fmi2Error;
}

fmi2Status fmi2GetRealStatus(
    fmi2Component c,
    const fmi2StatusKind s,
    fmi2Real* value)
{
    const auto component = reinterpret_cast<Component*>(c);
    if (component == nullptr) return fmi2Error;
    if (RefuseIllegalCall(*component, "fmi2GetRealStatus", Call::GetStatus)) {
        return fmi2Error;
    }
    if (s == fmi2LastSuccessfulTime) {
        *value = component->lastSuccessfulTime;
        return fmi2OK;
    } else {
        component->logger.Log(
            fmi2Error,
            CPPFMU_LOG_CATEGORY,
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
        CPPFMU_LOG_CATEGORY,
        "FMI function not supported: fmi2GetIntegerStatus");
    return fmi2Error;
}

fmi2Status fmi2GetBooleanStatus(
    fmi2Component c,
    const fmi2StatusKind s,
    fmi2Boolean* value)
{
    const auto component = reinterpret_cast<Component*>(c);
    if (component == nullptr) return fmi2Error;
    if (RefuseIllegalCall(
            *component,
            "fmi2GetBooleanStatus",
            Call::GetStatus)) {
        return fmi2Error;
    }
    // Importers ask for fmi2Terminated after fmi2DoStep returned fmi2Discard
    // (4.2.3). The slave decides whether it wants the simulation to end.
    if (s == fmi2Terminated && value != nullptr) {
        bool requested = false;
        const auto status = RunCall(
            *component,
            "fmi2GetBooleanStatus",
            [&](cppfmu::SlaveInstance& slave) {
                requested = slave.TerminateRequested();
            });
        if (status != fmi2OK) return status;
        *value = requested ? fmi2True : fmi2False;
        return fmi2OK;
    }
    component->logger.Log(
        fmi2Error,
        CPPFMU_LOG_CATEGORY,
        "Invalid status inquiry for fmi2GetBooleanStatus");
    return fmi2Error;
}

fmi2Status fmi2GetStringStatus(
    fmi2Component c,
    const fmi2StatusKind,
    fmi2String*)
{
    reinterpret_cast<Component*>(c)->logger.Log(
        fmi2Error,
        CPPFMU_LOG_CATEGORY,
        "FMI function not supported: fmi2GetStringStatus");
    return fmi2Error;
}

#endif // CPPFMU_USE_FMI_1_0
}
