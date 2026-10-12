/* Copyright 2016-2026, SINTEF Ocean.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

/* The FMI 3.0 Co-Simulation C entry points, dispatching to SlaveInstance3.
 *
 * Every function fmi3Functions.h declares is defined here, because the
 * reference importers (fmpy, fmusim) resolve the whole function set when they
 * load a module, whatever interface the model description declares, and refuse
 * an FMU missing one. A function that belongs to an interface this layer does
 * not serve -- Model Exchange, Scheduled Execution, and the
 * clock/continuous-time calls -- is refused with fmi3Error (or a null instance)
 * rather than dispatched.
 *
 * A per-instance state machine (instantiated -> initialization -> step -> event
 * -> terminated, plus a latched failed state) confines every call to the states
 * FMI 3.0 allows it in: a call made in the wrong state is refused with
 * fmi3Error before it reaches a slave whose simulation may not exist yet or any
 * more, and a call the slave threw a non-logic exception out of latches the
 * instance failed, after which FMI 3.0 allows only freeing, resetting, and
 * restoring a saved state.
 */

#include <exception>
#include <functional>
#include <limits>
#include <memory>
#include <string>

#include "cppfmu_cs_fmi3.hpp"
#include "cppfmu_lifecycle_fmi3.hpp"

namespace
{
using cppfmu::Lifecycle;
using cppfmu::LifecycleCall;

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
    { }

    bool IsCategoryLogged(const std::string& category) const
    {
        if (loggedCategories.empty()) return true;
        for (const auto& c : loggedCategories) {
            if (c == category) return true;
        }
        return false;
    }

    void Log(
        fmi3Status status,
        cppfmu::FMIString category,
        cppfmu::FMIString message) const
    {
        if ((status == fmi3Fatal || status == fmi3Error ||
             debugLoggingEnabled) &&
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
    Lifecycle lifecycle;
    cppfmu::FMIReal lastSuccessfulTime;
};

Component* AsComponent(fmi3Instance instance)
{
    return reinterpret_cast<Component*>(instance);
}

/* Sends one fmi3Error message to the host, swallowing a failed report.
 *
 * Reporting runs on the way out of a call that has already failed, and
 * formatting the message allocates; letting that throw would carry an exception
 * out of a function with C linkage, the very thing the reporting is here to
 * prevent.
 */
void ReportError(const Component& component, const char* message)
{
    try {
        component.Log(fmi3Error, "cppfmu", message);
    } catch (...) {
        // There is nothing left to report a failed report with.
    }
}

/* Tells the host which call was refused and why. */
void ReportRefusal(
    const Component& component,
    const char* functionName,
    const char* reason)
{
    try {
        ReportError(
            component,
            (std::string(functionName) + " " + reason).c_str());
    } catch (...) {
        // There is nothing left to report a failed report with.
    }
}

/* Refuses a call whose arguments this FMU cannot work with. The instance keeps
 * the state it had: nothing was done with the arguments. */
fmi3Status ReportInvalidArgument(
    fmi3Instance instance,
    const char* functionName,
    const char* reason)
{
    Component* const component = AsComponent(instance);
    if (component != nullptr) {
        ReportRefusal(*component, functionName, reason);
    }
    return fmi3Error;
}

// The legal-state set and the transition of each call live in the Lifecycle
// module's table, not here. Each entry point names the LifecycleCall its call
// belongs to; RunLegalCall asks the instance's Lifecycle whether the current
// state allows it and, on success, lets the Lifecycle perform the transition.

/* Runs one call on the slave when the instance's state allows it.
 *
 * A call made in the wrong state never reaches the slave and changes nothing. A
 * call the slave threw a std::logic_error out of -- an unknown value reference,
 * a restore with nothing live to restore into -- is a violated precondition,
 * not a misbehaving simulation, and is refused the way an argument is:
 * fmi3Error, instance left as it was. Any other std::exception, or a
 * non-exception, leaves the instance failed. A cppfmu::FatalError takes the
 * whole environment down with fmi3Fatal.
 */
template<typename Operation>
fmi3Status RunLegalCall(
    fmi3Instance instance,
    const char* functionName,
    LifecycleCall call,
    Operation operation)
{
    Component* const component = AsComponent(instance);
    if (component == nullptr) {
        return fmi3Error;
    }
    if (!component->lifecycle.Allows(call)) {
        ReportRefusal(
            *component,
            functionName,
            "was called in a state that does not allow it");
        return fmi3Error;
    }
    try {
        operation(*component->slave);
        component->lifecycle.OnSuccess(call);
        return fmi3OK;
    } catch (const cppfmu::FatalError& e) {
        ReportError(*component, e.what());
        component->lifecycle.Fail();
        return fmi3Fatal;
    } catch (const std::logic_error& e) {
        // A precondition the call itself violated (std::out_of_range is a
        // std::logic_error, so this catches that too). Refused, instance left as
        // it was.
        ReportError(*component, e.what());
        return fmi3Error;
    } catch (const std::exception& e) {
        ReportError(*component, e.what());
        component->lifecycle.Fail();
        return fmi3Error;
    } catch (...) {
        ReportError(
            *component,
            "The FMU failed with an exception carrying no message");
        component->lifecycle.Fail();
        return fmi3Error;
    }
}

/* Refuses to instantiate an interface this layer does not serve. */
fmi3Instance RefuseInstantiation(
    cppfmu::FMIComponentEnvironment instanceEnvironment,
    fmi3LogMessageCallback logMessage,
    const char* functionName)
{
    if (logMessage) {
        logMessage(
            instanceEnvironment,
            fmi3Error,
            "cppfmu",
            (std::string(functionName) +
             " is not provided by this FMU, which declares Co-Simulation only")
                .c_str());
    }
    return nullptr;
}
} // namespace

extern "C"
{
// =============================================================================
// Inquire version numbers and set debug logging
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
    Component* const component = AsComponent(instance);
    if (component == nullptr) return fmi3Error;
    component->debugLoggingEnabled = (loggingOn == fmi3True);
    component->loggedCategories.clear();
    for (size_t i = 0; i < nCategories; ++i) {
        component->loggedCategories.emplace_back(categories[i]);
    }
    return fmi3OK;
}

// =============================================================================
// Creation and destruction of FMU instances
// =============================================================================

fmi3Instance fmi3InstantiateModelExchange(
    fmi3String /*instanceName*/,
    fmi3String /*instantiationToken*/,
    fmi3String /*resourcePath*/,
    fmi3Boolean /*visible*/,
    fmi3Boolean /*loggingOn*/,
    fmi3InstanceEnvironment instanceEnvironment,
    fmi3LogMessageCallback logMessage)
{
    return RefuseInstantiation(
        instanceEnvironment,
        logMessage,
        "fmi3InstantiateModelExchange");
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
        component.reset(
            new Component(instanceEnvironment, logMessage, loggingOn));

        auto* comp = component.get();
        auto loggerFn = [comp](
                            cppfmu::FMIStatus status,
                            cppfmu::FMIString category,
                            cppfmu::FMIString message) {
            comp->Log(static_cast<fmi3Status>(status), category, message);
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

fmi3Instance fmi3InstantiateScheduledExecution(
    fmi3String /*instanceName*/,
    fmi3String /*instantiationToken*/,
    fmi3String /*resourcePath*/,
    fmi3Boolean /*visible*/,
    fmi3Boolean /*loggingOn*/,
    fmi3InstanceEnvironment instanceEnvironment,
    fmi3LogMessageCallback logMessage,
    fmi3ClockUpdateCallback /*clockUpdate*/,
    fmi3LockPreemptionCallback /*lockPreemption*/,
    fmi3UnlockPreemptionCallback /*unlockPreemption*/)
{
    return RefuseInstantiation(
        instanceEnvironment,
        logMessage,
        "fmi3InstantiateScheduledExecution");
}

void fmi3FreeInstance(fmi3Instance instance)
{
    if (instance == nullptr) return;
    delete AsComponent(instance);
}

// =============================================================================
// Enter and exit initialization mode, terminate and reset
// =============================================================================

fmi3Status fmi3EnterInitializationMode(
    fmi3Instance instance,
    fmi3Boolean toleranceDefined,
    fmi3Float64 tolerance,
    fmi3Float64 startTime,
    fmi3Boolean stopTimeDefined,
    fmi3Float64 stopTime)
{
    return RunLegalCall(
        instance,
        "fmi3EnterInitializationMode",
        LifecycleCall::EnterInitialization,
        [&](cppfmu::SlaveInstance3& slave) {
            slave.EnterInitializationMode(
                toleranceDefined,
                static_cast<cppfmu::FMIReal>(tolerance),
                static_cast<cppfmu::FMIReal>(startTime),
                stopTimeDefined,
                static_cast<cppfmu::FMIReal>(stopTime));
        });
}

fmi3Status fmi3ExitInitializationMode(fmi3Instance instance)
{
    return RunLegalCall(
        instance,
        "fmi3ExitInitializationMode",
        LifecycleCall::ExitInitialization,
        [](cppfmu::SlaveInstance3& slave) { slave.ExitInitializationMode(); });
}

fmi3Status fmi3EnterEventMode(fmi3Instance instance)
{
    return RunLegalCall(
        instance,
        "fmi3EnterEventMode",
        LifecycleCall::EnterEventMode,
        [](cppfmu::SlaveInstance3& slave) { slave.EnterEventMode(); });
}

fmi3Status fmi3Terminate(fmi3Instance instance)
{
    return RunLegalCall(
        instance,
        "fmi3Terminate",
        LifecycleCall::Terminate,
        [](cppfmu::SlaveInstance3& slave) { slave.Terminate(); });
}

fmi3Status fmi3Reset(fmi3Instance instance)
{
    return RunLegalCall(
        instance,
        "fmi3Reset",
        LifecycleCall::Reset,
        [](cppfmu::SlaveInstance3& slave) { slave.Reset(); });
}

// =============================================================================
// Get functions
// =============================================================================

fmi3Status fmi3GetFloat32(
    fmi3Instance instance,
    const fmi3ValueReference vr[],
    size_t nvr,
    fmi3Float32 value[],
    size_t nValues)
{
    return RunLegalCall(
        instance,
        "fmi3GetFloat32",
        LifecycleCall::ReadVariable,
        [&](const cppfmu::SlaveInstance3& slave) {
            slave.GetFloat32(vr, nvr, value, nValues);
        });
}

fmi3Status fmi3GetFloat64(
    fmi3Instance instance,
    const fmi3ValueReference vr[],
    size_t nvr,
    fmi3Float64 value[],
    size_t nValues)
{
    return RunLegalCall(
        instance,
        "fmi3GetFloat64",
        LifecycleCall::ReadVariable,
        [&](const cppfmu::SlaveInstance3& slave) {
            slave.GetFloat64(vr, nvr, value, nValues);
        });
}

fmi3Status fmi3GetInt8(
    fmi3Instance instance,
    const fmi3ValueReference vr[],
    size_t nvr,
    fmi3Int8 value[],
    size_t nValues)
{
    return RunLegalCall(
        instance,
        "fmi3GetInt8",
        LifecycleCall::ReadVariable,
        [&](const cppfmu::SlaveInstance3& slave) {
            slave.GetInt8(vr, nvr, value, nValues);
        });
}

fmi3Status fmi3GetUInt8(
    fmi3Instance instance,
    const fmi3ValueReference vr[],
    size_t nvr,
    fmi3UInt8 value[],
    size_t nValues)
{
    return RunLegalCall(
        instance,
        "fmi3GetUInt8",
        LifecycleCall::ReadVariable,
        [&](const cppfmu::SlaveInstance3& slave) {
            slave.GetUInt8(vr, nvr, value, nValues);
        });
}

fmi3Status fmi3GetInt16(
    fmi3Instance instance,
    const fmi3ValueReference vr[],
    size_t nvr,
    fmi3Int16 value[],
    size_t nValues)
{
    return RunLegalCall(
        instance,
        "fmi3GetInt16",
        LifecycleCall::ReadVariable,
        [&](const cppfmu::SlaveInstance3& slave) {
            slave.GetInt16(vr, nvr, value, nValues);
        });
}

fmi3Status fmi3GetUInt16(
    fmi3Instance instance,
    const fmi3ValueReference vr[],
    size_t nvr,
    fmi3UInt16 value[],
    size_t nValues)
{
    return RunLegalCall(
        instance,
        "fmi3GetUInt16",
        LifecycleCall::ReadVariable,
        [&](const cppfmu::SlaveInstance3& slave) {
            slave.GetUInt16(vr, nvr, value, nValues);
        });
}

fmi3Status fmi3GetInt32(
    fmi3Instance instance,
    const fmi3ValueReference vr[],
    size_t nvr,
    fmi3Int32 value[],
    size_t nValues)
{
    return RunLegalCall(
        instance,
        "fmi3GetInt32",
        LifecycleCall::ReadVariable,
        [&](const cppfmu::SlaveInstance3& slave) {
            slave.GetInt32(vr, nvr, value, nValues);
        });
}

fmi3Status fmi3GetUInt32(
    fmi3Instance instance,
    const fmi3ValueReference vr[],
    size_t nvr,
    fmi3UInt32 value[],
    size_t nValues)
{
    return RunLegalCall(
        instance,
        "fmi3GetUInt32",
        LifecycleCall::ReadVariable,
        [&](const cppfmu::SlaveInstance3& slave) {
            slave.GetUInt32(vr, nvr, value, nValues);
        });
}

fmi3Status fmi3GetInt64(
    fmi3Instance instance,
    const fmi3ValueReference vr[],
    size_t nvr,
    fmi3Int64 value[],
    size_t nValues)
{
    return RunLegalCall(
        instance,
        "fmi3GetInt64",
        LifecycleCall::ReadVariable,
        [&](const cppfmu::SlaveInstance3& slave) {
            slave.GetInt64(vr, nvr, value, nValues);
        });
}

fmi3Status fmi3GetUInt64(
    fmi3Instance instance,
    const fmi3ValueReference vr[],
    size_t nvr,
    fmi3UInt64 value[],
    size_t nValues)
{
    return RunLegalCall(
        instance,
        "fmi3GetUInt64",
        LifecycleCall::ReadVariable,
        [&](const cppfmu::SlaveInstance3& slave) {
            slave.GetUInt64(vr, nvr, value, nValues);
        });
}

fmi3Status fmi3GetBoolean(
    fmi3Instance instance,
    const fmi3ValueReference vr[],
    size_t nvr,
    fmi3Boolean value[],
    size_t nValues)
{
    return RunLegalCall(
        instance,
        "fmi3GetBoolean",
        LifecycleCall::ReadVariable,
        [&](const cppfmu::SlaveInstance3& slave) {
            slave.GetBoolean(vr, nvr, value, nValues);
        });
}

fmi3Status fmi3GetString(
    fmi3Instance instance,
    const fmi3ValueReference vr[],
    size_t nvr,
    fmi3String value[],
    size_t nValues)
{
    return RunLegalCall(
        instance,
        "fmi3GetString",
        LifecycleCall::ReadVariable,
        [&](const cppfmu::SlaveInstance3& slave) {
            slave.GetString(vr, nvr, value, nValues);
        });
}

fmi3Status fmi3GetBinary(
    fmi3Instance instance,
    const fmi3ValueReference vr[],
    size_t nvr,
    size_t sizes[],
    fmi3Binary value[],
    size_t nValues)
{
    return RunLegalCall(
        instance,
        "fmi3GetBinary",
        LifecycleCall::ReadVariable,
        [&](const cppfmu::SlaveInstance3& slave) {
            slave.GetBinary(vr, nvr, sizes, value, nValues);
        });
}

fmi3Status fmi3GetClock(
    fmi3Instance instance,
    const fmi3ValueReference vr[],
    size_t nvr,
    fmi3Clock value[])
{
    return RunLegalCall(
        instance,
        "fmi3GetClock",
        LifecycleCall::ReadVariable,
        [&](const cppfmu::SlaveInstance3& slave) {
            slave.GetClock(vr, nvr, value);
        });
}

// =============================================================================
// Set functions
// =============================================================================

fmi3Status fmi3SetFloat32(
    fmi3Instance instance,
    const fmi3ValueReference vr[],
    size_t nvr,
    const fmi3Float32 value[],
    size_t nValues)
{
    return RunLegalCall(
        instance,
        "fmi3SetFloat32",
        LifecycleCall::WriteVariable,
        [&](cppfmu::SlaveInstance3& slave) {
            slave.SetFloat32(vr, nvr, value, nValues);
        });
}

fmi3Status fmi3SetFloat64(
    fmi3Instance instance,
    const fmi3ValueReference vr[],
    size_t nvr,
    const fmi3Float64 value[],
    size_t nValues)
{
    return RunLegalCall(
        instance,
        "fmi3SetFloat64",
        LifecycleCall::WriteVariable,
        [&](cppfmu::SlaveInstance3& slave) {
            slave.SetFloat64(vr, nvr, value, nValues);
        });
}

fmi3Status fmi3SetInt8(
    fmi3Instance instance,
    const fmi3ValueReference vr[],
    size_t nvr,
    const fmi3Int8 value[],
    size_t nValues)
{
    return RunLegalCall(
        instance,
        "fmi3SetInt8",
        LifecycleCall::WriteVariable,
        [&](cppfmu::SlaveInstance3& slave) {
            slave.SetInt8(vr, nvr, value, nValues);
        });
}

fmi3Status fmi3SetUInt8(
    fmi3Instance instance,
    const fmi3ValueReference vr[],
    size_t nvr,
    const fmi3UInt8 value[],
    size_t nValues)
{
    return RunLegalCall(
        instance,
        "fmi3SetUInt8",
        LifecycleCall::WriteVariable,
        [&](cppfmu::SlaveInstance3& slave) {
            slave.SetUInt8(vr, nvr, value, nValues);
        });
}

fmi3Status fmi3SetInt16(
    fmi3Instance instance,
    const fmi3ValueReference vr[],
    size_t nvr,
    const fmi3Int16 value[],
    size_t nValues)
{
    return RunLegalCall(
        instance,
        "fmi3SetInt16",
        LifecycleCall::WriteVariable,
        [&](cppfmu::SlaveInstance3& slave) {
            slave.SetInt16(vr, nvr, value, nValues);
        });
}

fmi3Status fmi3SetUInt16(
    fmi3Instance instance,
    const fmi3ValueReference vr[],
    size_t nvr,
    const fmi3UInt16 value[],
    size_t nValues)
{
    return RunLegalCall(
        instance,
        "fmi3SetUInt16",
        LifecycleCall::WriteVariable,
        [&](cppfmu::SlaveInstance3& slave) {
            slave.SetUInt16(vr, nvr, value, nValues);
        });
}

fmi3Status fmi3SetInt32(
    fmi3Instance instance,
    const fmi3ValueReference vr[],
    size_t nvr,
    const fmi3Int32 value[],
    size_t nValues)
{
    return RunLegalCall(
        instance,
        "fmi3SetInt32",
        LifecycleCall::WriteVariable,
        [&](cppfmu::SlaveInstance3& slave) {
            slave.SetInt32(vr, nvr, value, nValues);
        });
}

fmi3Status fmi3SetUInt32(
    fmi3Instance instance,
    const fmi3ValueReference vr[],
    size_t nvr,
    const fmi3UInt32 value[],
    size_t nValues)
{
    return RunLegalCall(
        instance,
        "fmi3SetUInt32",
        LifecycleCall::WriteVariable,
        [&](cppfmu::SlaveInstance3& slave) {
            slave.SetUInt32(vr, nvr, value, nValues);
        });
}

fmi3Status fmi3SetInt64(
    fmi3Instance instance,
    const fmi3ValueReference vr[],
    size_t nvr,
    const fmi3Int64 value[],
    size_t nValues)
{
    return RunLegalCall(
        instance,
        "fmi3SetInt64",
        LifecycleCall::WriteVariable,
        [&](cppfmu::SlaveInstance3& slave) {
            slave.SetInt64(vr, nvr, value, nValues);
        });
}

fmi3Status fmi3SetUInt64(
    fmi3Instance instance,
    const fmi3ValueReference vr[],
    size_t nvr,
    const fmi3UInt64 value[],
    size_t nValues)
{
    return RunLegalCall(
        instance,
        "fmi3SetUInt64",
        LifecycleCall::WriteVariable,
        [&](cppfmu::SlaveInstance3& slave) {
            slave.SetUInt64(vr, nvr, value, nValues);
        });
}

fmi3Status fmi3SetBoolean(
    fmi3Instance instance,
    const fmi3ValueReference vr[],
    size_t nvr,
    const fmi3Boolean value[],
    size_t nValues)
{
    return RunLegalCall(
        instance,
        "fmi3SetBoolean",
        LifecycleCall::WriteVariable,
        [&](cppfmu::SlaveInstance3& slave) {
            slave.SetBoolean(vr, nvr, value, nValues);
        });
}

fmi3Status fmi3SetString(
    fmi3Instance instance,
    const fmi3ValueReference vr[],
    size_t nvr,
    const fmi3String value[],
    size_t nValues)
{
    return RunLegalCall(
        instance,
        "fmi3SetString",
        LifecycleCall::WriteVariable,
        [&](cppfmu::SlaveInstance3& slave) {
            slave.SetString(vr, nvr, value, nValues);
        });
}

fmi3Status fmi3SetBinary(
    fmi3Instance instance,
    const fmi3ValueReference vr[],
    size_t nvr,
    const size_t sizes[],
    const fmi3Binary value[],
    size_t nValues)
{
    return RunLegalCall(
        instance,
        "fmi3SetBinary",
        LifecycleCall::WriteVariable,
        [&](cppfmu::SlaveInstance3& slave) {
            slave.SetBinary(vr, nvr, sizes, value, nValues);
        });
}

fmi3Status fmi3SetClock(
    fmi3Instance instance,
    const fmi3ValueReference vr[],
    size_t nvr,
    const fmi3Clock value[])
{
    return RunLegalCall(
        instance,
        "fmi3SetClock",
        LifecycleCall::WriteVariable,
        [&](cppfmu::SlaveInstance3& slave) { slave.SetClock(vr, nvr, value); });
}

// =============================================================================
// Getting and setting the internal FMU state
// =============================================================================

fmi3Status fmi3GetFMUState(fmi3Instance instance, fmi3FMUState* FMUState)
{
    if (FMUState == nullptr) {
        return ReportInvalidArgument(
            instance,
            "fmi3GetFMUState",
            "was called without anywhere to put the state");
    }
    return RunLegalCall(
        instance,
        "fmi3GetFMUState",
        LifecycleCall::FmuStateRead,
        [&](cppfmu::SlaveInstance3& slave) { slave.GetFMUState(FMUState); });
}

fmi3Status fmi3SetFMUState(fmi3Instance instance, fmi3FMUState FMUState)
{
    if (FMUState == nullptr) {
        return ReportInvalidArgument(
            instance,
            "fmi3SetFMUState",
            "was called without a state");
    }
    const fmi3Status status = RunLegalCall(
        instance,
        "fmi3SetFMUState",
        LifecycleCall::FmuStateRestore,
        [&](cppfmu::SlaveInstance3& slave) { slave.SetFMUState(FMUState); });
    // FMI 3.0's recovery idiom is to restore a state captured in Step Mode and
    // keep stepping, so a SetFMUState that succeeds while Failed is the one way
    // back there other than Reset. RestoreFromSnapshot() performs that move and
    // leaves any other state alone.
    if (status == fmi3OK) {
        AsComponent(instance)->lifecycle.RestoreFromSnapshot();
    }
    return status;
}

fmi3Status fmi3FreeFMUState(fmi3Instance instance, fmi3FMUState* FMUState)
{
    if (FMUState == nullptr) {
        return ReportInvalidArgument(
            instance,
            "fmi3FreeFMUState",
            "was called without a state handle");
    }
    if (*FMUState == nullptr) {
        return fmi3OK;
    }
    const fmi3Status status = RunLegalCall(
        instance,
        "fmi3FreeFMUState",
        LifecycleCall::SnapshotMemory,
        [&](cppfmu::SlaveInstance3& slave) { slave.FreeFMUState(*FMUState); });
    // Only a call that did free the state has earned the null that stops the host
    // freeing it twice.
    if (status == fmi3OK) {
        *FMUState = nullptr;
    }
    return status;
}

fmi3Status fmi3SerializedFMUStateSize(
    fmi3Instance instance,
    fmi3FMUState FMUState,
    size_t* size)
{
    if (FMUState == nullptr || size == nullptr) {
        return ReportInvalidArgument(
            instance,
            "fmi3SerializedFMUStateSize",
            "needs a state and somewhere to put its size");
    }
    return RunLegalCall(
        instance,
        "fmi3SerializedFMUStateSize",
        LifecycleCall::SnapshotMemory,
        [&](cppfmu::SlaveInstance3& slave) {
            *size = slave.SerializedFMUStateSize(FMUState);
        });
}

fmi3Status fmi3SerializeFMUState(
    fmi3Instance instance,
    fmi3FMUState FMUState,
    fmi3Byte serializedState[],
    size_t size)
{
    if (FMUState == nullptr || serializedState == nullptr) {
        return ReportInvalidArgument(
            instance,
            "fmi3SerializeFMUState",
            "needs a state and somewhere to write it");
    }
    return RunLegalCall(
        instance,
        "fmi3SerializeFMUState",
        LifecycleCall::SnapshotMemory,
        [&](cppfmu::SlaveInstance3& slave) {
            slave.SerializeFMUState(FMUState, serializedState, size);
        });
}

fmi3Status fmi3DeserializeFMUState(
    fmi3Instance instance,
    const fmi3Byte serializedState[],
    size_t size,
    fmi3FMUState* FMUState)
{
    if (serializedState == nullptr || FMUState == nullptr) {
        return ReportInvalidArgument(
            instance,
            "fmi3DeserializeFMUState",
            "needs serialized bytes and somewhere to put the state");
    }
    return RunLegalCall(
        instance,
        "fmi3DeserializeFMUState",
        LifecycleCall::SnapshotMemory,
        [&](cppfmu::SlaveInstance3& slave) {
            *FMUState = slave.DeserializeFMUState(serializedState, size);
        });
}

// =============================================================================
// Getting partial derivatives
// =============================================================================

// The quantity FMI asks for here is the partial derivative of the model
// equations with the states held -- not the response of a step. Confined to
// Step Mode: a co-simulation slave is free not to have a model to differentiate
// before ExitInitializationMode or after Terminate.

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
    return RunLegalCall(
        instance,
        "fmi3GetDirectionalDerivative",
        LifecycleCall::StepModeQuery,
        [&](const cppfmu::SlaveInstance3& slave) {
            slave.GetDirectionalDerivative(
                unknowns,
                nUnknowns,
                knowns,
                nKnowns,
                seed,
                nSeed,
                sensitivity,
                nSensitivity);
        });
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
    return RunLegalCall(
        instance,
        "fmi3GetAdjointDerivative",
        LifecycleCall::StepModeQuery,
        [&](const cppfmu::SlaveInstance3& slave) {
            slave.GetAdjointDerivative(
                unknowns,
                nUnknowns,
                knowns,
                nKnowns,
                seed,
                nSeed,
                sensitivity,
                nSensitivity);
        });
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
    return RunLegalCall(
        instance,
        "fmi3GetVariableDependencies",
        LifecycleCall::ReadVariable,
        [&](const cppfmu::SlaveInstance3& slave) {
            slave.GetVariableDependencies(
                dependent,
                elementIndicesOfDependent,
                independents,
                elementIndicesOfIndependents,
                dependencyKinds,
                nDependencies);
        });
}

fmi3Status fmi3GetNumberOfVariableDependencies(
    fmi3Instance instance,
    fmi3ValueReference valueReference,
    size_t* nDependencies)
{
    return RunLegalCall(
        instance,
        "fmi3GetNumberOfVariableDependencies",
        LifecycleCall::ReadVariable,
        [&](const cppfmu::SlaveInstance3& slave) {
            *nDependencies =
                slave.GetNumberOfVariableDependencies(valueReference);
        });
}

fmi3Status fmi3GetOutputDerivatives(
    fmi3Instance instance,
    const fmi3ValueReference vr[],
    size_t nvr,
    const fmi3Int32 orders[],
    fmi3Float64 values[],
    size_t nValues)
{
    return RunLegalCall(
        instance,
        "fmi3GetOutputDerivatives",
        LifecycleCall::StepModeQuery,
        [&](const cppfmu::SlaveInstance3& slave) {
            slave.GetOutputDerivatives(vr, nvr, orders, values, nValues);
        });
}

// =============================================================================
// Entering and exiting the Configuration or Reconfiguration Mode
// =============================================================================

// These exist for structural parameters, which a Co-Simulation slave of this
// shape declares none of.

fmi3Status fmi3EnterConfigurationMode(fmi3Instance instance)
{
    Component* const component = AsComponent(instance);
    if (component == nullptr) return fmi3Error;
    ReportRefusal(
        *component,
        "fmi3EnterConfigurationMode",
        "is not provided by this FMU");
    return fmi3Error;
}

fmi3Status fmi3ExitConfigurationMode(fmi3Instance instance)
{
    Component* const component = AsComponent(instance);
    if (component == nullptr) return fmi3Error;
    ReportRefusal(
        *component,
        "fmi3ExitConfigurationMode",
        "is not provided by this FMU");
    return fmi3Error;
}

// =============================================================================
// Clock and discrete state functions
// =============================================================================

fmi3Status fmi3GetIntervalDecimal(
    fmi3Instance instance,
    const fmi3ValueReference[],
    size_t,
    fmi3Float64[],
    fmi3IntervalQualifier[])
{
    Component* const component = AsComponent(instance);
    if (component == nullptr) return fmi3Error;
    ReportRefusal(
        *component,
        "fmi3GetIntervalDecimal",
        "is not provided by this FMU");
    return fmi3Error;
}

fmi3Status fmi3GetIntervalFraction(
    fmi3Instance instance,
    const fmi3ValueReference[],
    size_t,
    fmi3UInt64[],
    fmi3UInt64[],
    fmi3IntervalQualifier[])
{
    Component* const component = AsComponent(instance);
    if (component == nullptr) return fmi3Error;
    ReportRefusal(
        *component,
        "fmi3GetIntervalFraction",
        "is not provided by this FMU");
    return fmi3Error;
}

fmi3Status fmi3GetShiftDecimal(
    fmi3Instance instance,
    const fmi3ValueReference[],
    size_t,
    fmi3Float64[])
{
    Component* const component = AsComponent(instance);
    if (component == nullptr) return fmi3Error;
    ReportRefusal(
        *component,
        "fmi3GetShiftDecimal",
        "is not provided by this FMU");
    return fmi3Error;
}

fmi3Status fmi3GetShiftFraction(
    fmi3Instance instance,
    const fmi3ValueReference[],
    size_t,
    fmi3UInt64[],
    fmi3UInt64[])
{
    Component* const component = AsComponent(instance);
    if (component == nullptr) return fmi3Error;
    ReportRefusal(
        *component,
        "fmi3GetShiftFraction",
        "is not provided by this FMU");
    return fmi3Error;
}

fmi3Status fmi3SetIntervalDecimal(
    fmi3Instance instance,
    const fmi3ValueReference[],
    size_t,
    const fmi3Float64[])
{
    Component* const component = AsComponent(instance);
    if (component == nullptr) return fmi3Error;
    ReportRefusal(
        *component,
        "fmi3SetIntervalDecimal",
        "is not provided by this FMU");
    return fmi3Error;
}

fmi3Status fmi3SetIntervalFraction(
    fmi3Instance instance,
    const fmi3ValueReference[],
    size_t,
    const fmi3UInt64[],
    const fmi3UInt64[])
{
    Component* const component = AsComponent(instance);
    if (component == nullptr) return fmi3Error;
    ReportRefusal(
        *component,
        "fmi3SetIntervalFraction",
        "is not provided by this FMU");
    return fmi3Error;
}

fmi3Status fmi3SetShiftDecimal(
    fmi3Instance instance,
    const fmi3ValueReference[],
    size_t,
    const fmi3Float64[])
{
    Component* const component = AsComponent(instance);
    if (component == nullptr) return fmi3Error;
    ReportRefusal(
        *component,
        "fmi3SetShiftDecimal",
        "is not provided by this FMU");
    return fmi3Error;
}

fmi3Status fmi3SetShiftFraction(
    fmi3Instance instance,
    const fmi3ValueReference[],
    size_t,
    const fmi3UInt64[],
    const fmi3UInt64[])
{
    Component* const component = AsComponent(instance);
    if (component == nullptr) return fmi3Error;
    ReportRefusal(
        *component,
        "fmi3SetShiftFraction",
        "is not provided by this FMU");
    return fmi3Error;
}

fmi3Status fmi3EvaluateDiscreteStates(fmi3Instance instance)
{
    return RunLegalCall(
        instance,
        "fmi3EvaluateDiscreteStates",
        LifecycleCall::EvaluateDiscreteStates,
        [](cppfmu::SlaveInstance3& slave) { slave.EvaluateDiscreteStates(); });
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
    return RunLegalCall(
        instance,
        "fmi3UpdateDiscreteStates",
        LifecycleCall::UpdateDiscreteStates,
        [&](cppfmu::SlaveInstance3& slave) {
            fmi3Boolean needUpdate = fmi3False;
            fmi3Boolean termSim = fmi3False;
            fmi3Boolean nominalsChanged = fmi3False;
            fmi3Boolean valuesChanged = fmi3False;
            fmi3Boolean timeDefined = fmi3False;
            fmi3Float64 time = 0.0;

            slave.UpdateDiscreteStates(
                needUpdate,
                termSim,
                nominalsChanged,
                valuesChanged,
                timeDefined,
                time);

            if (discreteStatesNeedUpdate)
                *discreteStatesNeedUpdate = needUpdate;
            if (terminateSimulation) *terminateSimulation = termSim;
            if (nominalsOfContinuousStatesChanged)
                *nominalsOfContinuousStatesChanged = nominalsChanged;
            if (valuesOfContinuousStatesChanged)
                *valuesOfContinuousStatesChanged = valuesChanged;
            if (nextEventTimeDefined) *nextEventTimeDefined = timeDefined;
            if (nextEventTime) *nextEventTime = time;
        });
}

// =============================================================================
// Functions for Model Exchange
// =============================================================================

// No instance of a Co-Simulation slave is ever a Model Exchange instance, so
// none of these has anything to act on. They exist because the importers
// resolve the whole fmi3Functions.h set at load time.

fmi3Status fmi3EnterContinuousTimeMode(fmi3Instance instance)
{
    Component* const component = AsComponent(instance);
    if (component == nullptr) return fmi3Error;
    ReportRefusal(
        *component,
        "fmi3EnterContinuousTimeMode",
        "is not provided by this FMU");
    return fmi3Error;
}

fmi3Status fmi3CompletedIntegratorStep(
    fmi3Instance instance,
    fmi3Boolean,
    fmi3Boolean*,
    fmi3Boolean*)
{
    Component* const component = AsComponent(instance);
    if (component == nullptr) return fmi3Error;
    ReportRefusal(
        *component,
        "fmi3CompletedIntegratorStep",
        "is not provided by this FMU");
    return fmi3Error;
}

fmi3Status fmi3SetTime(fmi3Instance instance, fmi3Float64)
{
    Component* const component = AsComponent(instance);
    if (component == nullptr) return fmi3Error;
    ReportRefusal(*component, "fmi3SetTime", "is not provided by this FMU");
    return fmi3Error;
}

fmi3Status fmi3SetContinuousStates(
    fmi3Instance instance,
    const fmi3Float64[],
    size_t)
{
    Component* const component = AsComponent(instance);
    if (component == nullptr) return fmi3Error;
    ReportRefusal(
        *component,
        "fmi3SetContinuousStates",
        "is not provided by this FMU");
    return fmi3Error;
}

fmi3Status fmi3GetContinuousStateDerivatives(
    fmi3Instance instance,
    fmi3Float64[],
    size_t)
{
    Component* const component = AsComponent(instance);
    if (component == nullptr) return fmi3Error;
    ReportRefusal(
        *component,
        "fmi3GetContinuousStateDerivatives",
        "is not provided by this FMU");
    return fmi3Error;
}

fmi3Status fmi3GetEventIndicators(fmi3Instance instance, fmi3Float64[], size_t)
{
    Component* const component = AsComponent(instance);
    if (component == nullptr) return fmi3Error;
    ReportRefusal(
        *component,
        "fmi3GetEventIndicators",
        "is not provided by this FMU");
    return fmi3Error;
}

fmi3Status fmi3GetContinuousStates(fmi3Instance instance, fmi3Float64[], size_t)
{
    Component* const component = AsComponent(instance);
    if (component == nullptr) return fmi3Error;
    ReportRefusal(
        *component,
        "fmi3GetContinuousStates",
        "is not provided by this FMU");
    return fmi3Error;
}

fmi3Status fmi3GetNominalsOfContinuousStates(
    fmi3Instance instance,
    fmi3Float64[],
    size_t)
{
    Component* const component = AsComponent(instance);
    if (component == nullptr) return fmi3Error;
    ReportRefusal(
        *component,
        "fmi3GetNominalsOfContinuousStates",
        "is not provided by this FMU");
    return fmi3Error;
}

fmi3Status fmi3GetNumberOfEventIndicators(
    fmi3Instance instance,
    size_t* nEventIndicators)
{
    return RunLegalCall(
        instance,
        "fmi3GetNumberOfEventIndicators",
        LifecycleCall::ReadVariable,
        [&](const cppfmu::SlaveInstance3& slave) {
            *nEventIndicators = slave.GetNumberOfEventIndicators();
        });
}

fmi3Status fmi3GetNumberOfContinuousStates(
    fmi3Instance instance,
    size_t* nContinuousStates)
{
    return RunLegalCall(
        instance,
        "fmi3GetNumberOfContinuousStates",
        LifecycleCall::ReadVariable,
        [&](const cppfmu::SlaveInstance3& slave) {
            *nContinuousStates = slave.GetNumberOfContinuousStates();
        });
}

// =============================================================================
// Simulating the FMU
// =============================================================================

fmi3Status fmi3EnterStepMode(fmi3Instance instance)
{
    return RunLegalCall(
        instance,
        "fmi3EnterStepMode",
        LifecycleCall::EnterStepMode,
        [](cppfmu::SlaveInstance3& slave) { slave.EnterStepMode(); });
}

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
    Component* const component = AsComponent(instance);
    if (component == nullptr) {
        return fmi3Error;
    }
    if (!component->lifecycle.Allows(LifecycleCall::DoStep)) {
        ReportRefusal(*component, "fmi3DoStep", "was called outside Step Mode");
        return fmi3Error;
    }

    // FMI 3.0 requires a positive step. A zero or negative one would be reported
    // as already reached and a NaN one would reach the slave, so both are refused
    // before anything is asked of it, which leaves the instance in Step Mode.
    if (!(communicationStepSize > 0.0)) {
        if (eventHandlingNeeded) *eventHandlingNeeded = fmi3False;
        if (terminateSimulation) *terminateSimulation = fmi3False;
        if (earlyReturn) *earlyReturn = fmi3False;
        if (lastSuccessfulTime) *lastSuccessfulTime = currentCommunicationPoint;
        return ReportInvalidArgument(
            instance,
            "fmi3DoStep",
            "needs a communicationStepSize greater than zero");
    }

    try {
        fmi3Boolean eventNeeded = fmi3False;
        fmi3Boolean termSim = fmi3False;
        fmi3Boolean earlyRet = fmi3False;
        fmi3Float64 endOfStep = currentCommunicationPoint;

        const bool stepCompleted = component->slave->DoStep(
            static_cast<cppfmu::FMIReal>(currentCommunicationPoint),
            static_cast<cppfmu::FMIReal>(communicationStepSize),
            noSetFMUStatePriorToCurrentPoint,
            eventNeeded,
            termSim,
            earlyRet,
            endOfStep);

        if (eventHandlingNeeded) *eventHandlingNeeded = eventNeeded;
        if (terminateSimulation) *terminateSimulation = termSim;
        if (earlyReturn) *earlyReturn = earlyRet;
        if (lastSuccessfulTime) *lastSuccessfulTime = endOfStep;
        component->lastSuccessfulTime = static_cast<cppfmu::FMIReal>(endOfStep);

        return stepCompleted ? fmi3OK : fmi3Discard;
    } catch (const cppfmu::FatalError& e) {
        ReportError(*component, e.what());
        component->lifecycle.Fail();
        if (lastSuccessfulTime) *lastSuccessfulTime = currentCommunicationPoint;
        return fmi3Fatal;
    } catch (const std::exception& e) {
        ReportError(*component, e.what());
        component->lifecycle.Fail();
        if (lastSuccessfulTime) *lastSuccessfulTime = currentCommunicationPoint;
        return fmi3Error;
    } catch (...) {
        ReportError(
            *component,
            "The FMU failed with an exception carrying no message");
        component->lifecycle.Fail();
        if (lastSuccessfulTime) *lastSuccessfulTime = currentCommunicationPoint;
        return fmi3Error;
    }
}

// =============================================================================
// Functions for Scheduled Execution
// =============================================================================

fmi3Status fmi3ActivateModelPartition(
    fmi3Instance instance,
    fmi3ValueReference,
    fmi3Float64)
{
    Component* const component = AsComponent(instance);
    if (component == nullptr) return fmi3Error;
    ReportRefusal(
        *component,
        "fmi3ActivateModelPartition",
        "is not provided by this FMU");
    return fmi3Error;
}

} // extern "C"
