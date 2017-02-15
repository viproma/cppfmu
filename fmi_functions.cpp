/* Copyright 2016-2017, SINTEF Ocean.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
#include <exception>
#include <limits>

#include "cppfmu_cs.hpp"


namespace
{
    // A struct that holds all the data for one model instance.
    struct Component
    {
        Component(
            fmiString instanceName,
            fmiCallbackFunctions callbackFunctions,
            fmiBoolean loggingOn)
            : memory{callbackFunctions}
            , debugLoggingEnabled{std::make_shared<bool>(loggingOn == fmiTrue)}
            , logger{this, cppfmu::CopyString(memory, instanceName), callbackFunctions, debugLoggingEnabled}
            , lastSuccessfulTime{std::numeric_limits<fmiReal>::quiet_NaN()}
        {
        }

        // General
        cppfmu::Memory memory;
        std::shared_ptr<bool> debugLoggingEnabled;
        cppfmu::Logger logger;

        // Co-simulation
        cppfmu::UniquePtr<cppfmu::SlaveInstance> slave;
        fmiReal lastSuccessfulTime;
    };
}


// FMI functions
extern "C"
{


DllExport const char* fmiGetTypesPlatform() { return fmiPlatform; }


DllExport const char* fmiGetVersion() { return fmiVersion; }


DllExport fmiComponent fmiInstantiateSlave(
    fmiString  instanceName,
    fmiString  fmuGUID,
    fmiString  fmuLocation,
    fmiString  mimeType,
    fmiReal    timeout,
    fmiBoolean visible,
    fmiBoolean interactive,
    fmiCallbackFunctions functions,
    fmiBoolean loggingOn)
{
    try {
        auto component = cppfmu::AllocateUnique<Component>(cppfmu::Memory{functions},
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


DllExport void fmiFreeSlaveInstance(fmiComponent c)
{
    const auto component = reinterpret_cast<Component*>(c);
    // The Component object was allocated using cppfmu::AllocateUnique(),
    // which uses cppfmu::New() internally, so we use cppfmu::Delete() to
    // release it again.
    cppfmu::Delete(component->memory, component);
}


DllExport fmiStatus fmiInitializeSlave(
    fmiComponent c,
    fmiReal      tStart,
    fmiBoolean   stopTimeDefined,
    fmiReal      tStop)
{
    const auto component = reinterpret_cast<Component*>(c);
    try {
        component->slave->Initialize(tStart, stopTimeDefined, tStop);
        return fmiOK;
    } catch (const cppfmu::FatalError& e) {
        component->logger.Log(fmiFatal, "", e.what());
        return fmiFatal;
    } catch (const std::exception& e) {
        component->logger.Log(fmiError, "", e.what());
        return fmiError;
    }
}


DllExport fmiStatus fmiResetSlave(fmiComponent c)
{
    const auto component = reinterpret_cast<Component*>(c);
    try {
        component->slave->Reset();
        return fmiOK;
    } catch (const cppfmu::FatalError& e) {
        component->logger.Log(fmiFatal, "", e.what());
        return fmiFatal;
    } catch (const std::exception& e) {
        component->logger.Log(fmiError, "", e.what());
        return fmiError;
    }
}


DllExport fmiStatus fmiTerminateSlave(fmiComponent c)
{
    const auto component = reinterpret_cast<Component*>(c);
    try {
        component->slave->Terminate();
        return fmiOK;
    } catch (const cppfmu::FatalError& e) {
        component->logger.Log(fmiFatal, "", e.what());
        return fmiFatal;
    } catch (const std::exception& e) {
        component->logger.Log(fmiError, "", e.what());
        return fmiError;
    }
}


DllExport fmiStatus fmiSetDebugLogging(
    fmiComponent c,
    fmiBoolean loggingOn)
{
    *(reinterpret_cast<Component*>(c)->debugLoggingEnabled) = (loggingOn == fmiTrue);
    return fmiOK;
}


DllExport fmiStatus fmiGetReal(
    fmiComponent c,
    const fmiValueReference vr[],
    size_t nvr,
    fmiReal value[])
{
    const auto component = reinterpret_cast<Component*>(c);
    try {
        component->slave->GetReal(vr, nvr, value);
        return fmiOK;
    } catch (const cppfmu::FatalError& e) {
        component->logger.Log(fmiFatal, "", e.what());
        return fmiFatal;
    } catch (const std::exception& e) {
        component->logger.Log(fmiError, "", e.what());
        return fmiError;
    }
}

DllExport fmiStatus fmiGetInteger(
    fmiComponent c,
    const fmiValueReference vr[],
    size_t nvr,
    fmiInteger value[])
{
    const auto component = reinterpret_cast<Component*>(c);
    try {
        component->slave->GetInteger(vr, nvr, value);
        return fmiOK;
    } catch (const cppfmu::FatalError& e) {
        component->logger.Log(fmiFatal, "", e.what());
        return fmiFatal;
    } catch (const std::exception& e) {
        component->logger.Log(fmiError, "", e.what());
        return fmiError;
    }
}

DllExport fmiStatus fmiGetBoolean(
    fmiComponent c,
    const fmiValueReference vr[],
    size_t nvr,
    fmiBoolean value[])
{
    const auto component = reinterpret_cast<Component*>(c);
    try {
        component->slave->GetBoolean(vr, nvr, value);
        return fmiOK;
    } catch (const cppfmu::FatalError& e) {
        component->logger.Log(fmiFatal, "", e.what());
        return fmiFatal;
    } catch (const std::exception& e) {
        component->logger.Log(fmiError, "", e.what());
        return fmiError;
    }
}

DllExport fmiStatus fmiGetString(
    fmiComponent c,
    const fmiValueReference vr[],
    size_t nvr,
    fmiString value[])
{
    const auto component = reinterpret_cast<Component*>(c);
    try {
        component->slave->GetString(vr, nvr, value);
        return fmiOK;
    } catch (const cppfmu::FatalError& e) {
        component->logger.Log(fmiFatal, "", e.what());
        return fmiFatal;
    } catch (const std::exception& e) {
        component->logger.Log(fmiError, "", e.what());
        return fmiError;
    }
}


DllExport fmiStatus fmiSetReal(
    fmiComponent c,
    const fmiValueReference vr[],
    size_t nvr,
    const fmiReal value[])
{
    const auto component = reinterpret_cast<Component*>(c);
    try {
        component->slave->SetReal(vr, nvr, value);
        return fmiOK;
    } catch (const cppfmu::FatalError& e) {
        component->logger.Log(fmiFatal, "", e.what());
        return fmiFatal;
    } catch (const std::exception& e) {
        component->logger.Log(fmiError, "", e.what());
        return fmiError;
    }
}

DllExport fmiStatus fmiSetInteger(
    fmiComponent c,
    const fmiValueReference vr[],
    size_t nvr,
    const fmiInteger value[])
{
    const auto component = reinterpret_cast<Component*>(c);
    try {
        component->slave->SetInteger(vr, nvr, value);
        return fmiOK;
    } catch (const cppfmu::FatalError& e) {
        component->logger.Log(fmiFatal, "", e.what());
        return fmiFatal;
    } catch (const std::exception& e) {
        component->logger.Log(fmiError, "", e.what());
        return fmiError;
    }
}

DllExport fmiStatus fmiSetBoolean (fmiComponent c, const fmiValueReference vr[], size_t nvr, const fmiBoolean value[])
{
    const auto component = reinterpret_cast<Component*>(c);
    try {
        component->slave->SetBoolean(vr, nvr, value);
        return fmiOK;
    } catch (const cppfmu::FatalError& e) {
        component->logger.Log(fmiFatal, "", e.what());
        return fmiFatal;
    } catch (const std::exception& e) {
        component->logger.Log(fmiError, "", e.what());
        return fmiError;
    }
}

DllExport fmiStatus fmiSetString  (fmiComponent c, const fmiValueReference vr[], size_t nvr, const fmiString  value[])
{
    const auto component = reinterpret_cast<Component*>(c);
    try {
        component->slave->SetString(vr, nvr, value);
        return fmiOK;
    } catch (const cppfmu::FatalError& e) {
        component->logger.Log(fmiFatal, "", e.what());
        return fmiFatal;
    } catch (const std::exception& e) {
        component->logger.Log(fmiError, "", e.what());
        return fmiError;
    }
}


DllExport fmiStatus fmiSetRealInputDerivatives(
    fmiComponent c,
    const  fmiValueReference /*vr*/[],
    size_t /*nvr*/,
    const  fmiInteger /*order*/[],
    const  fmiReal /*value*/[])
{
    reinterpret_cast<Component*>(c)->logger.Log(
        fmiError,
        "cppfmu",
        "FMI function not supported: fmiSetRealInputDerivatives");
    return fmiError;
}


DllExport fmiStatus fmiGetRealOutputDerivatives(
    fmiComponent c,
    const   fmiValueReference /*vr*/[],
    size_t  /*nvr*/,
    const   fmiInteger /*order*/[],
    fmiReal /*value*/[])
{
    reinterpret_cast<Component*>(c)->logger.Log(
        fmiError,
        "cppfmu",
        "FMI function not supported: fmiGetRealInputDerivatives");
    return fmiError;
}



DllExport fmiStatus fmiCancelStep(fmiComponent c)
{
    reinterpret_cast<Component*>(c)->logger.Log(
        fmiError,
        "cppfmu",
        "FMI function not supported: fmiCancelStep");
    return fmiError;
}


DllExport fmiStatus fmiDoStep(
    fmiComponent c,
    fmiReal      currentCommunicationPoint,
    fmiReal      communicationStepSize,
    fmiBoolean   newStep)
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


DllExport fmiStatus fmiGetStatus(
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



DllExport fmiStatus fmiGetRealStatus(
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


DllExport fmiStatus fmiGetIntegerStatus(
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



DllExport fmiStatus fmiGetBooleanStatus(
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


DllExport fmiStatus fmiGetStringStatus(
    fmiComponent c,
    const fmiStatusKind /*s*/,
    fmiString*  /*value*/)
{
    reinterpret_cast<Component*>(c)->logger.Log(
        fmiError,
        "cppfmu",
        "FMI function not supported: fmiGetStringStatus");
    return fmiError;
}


}
