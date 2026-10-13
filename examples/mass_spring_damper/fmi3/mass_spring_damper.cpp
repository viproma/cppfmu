/* Copyright 2026, SINTEF Ocean.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

/* FMI 3.0 mass-spring-damper co-simulation slave.
 *
 * Same model as ../fmi2/mass_spring_damper.cpp, adapted to FMI 3.0. Compare
 * the two files side by side to see what changes between the versions:
 *
 *   * Base class cppfmu::SlaveInstance3 instead of cppfmu::SlaveInstance.
 *   * GetReal/SetReal become GetFloat64/SetFloat64, and every Get/Set takes an
 *     extra nValues argument for array-valued variables (all our variables are
 *     scalars, so nValues == nvr and we can ignore it).
 *   * DoStep gains four output parameters (event handling, termination, early
 *     return, last successful time).
 *   * No cppfmu::Memory: FMI 3.0 dropped the allocator callbacks, so we use
 *     cppfmu::AllocateUnique3 and plain new/delete.
 *   * The logger is a std::function rather than a cppfmu::Logger.
 *
 * The value references below MUST match modelDescription.xml.
 */

#include <cppfmu_cs_fmi3.hpp>

#include <stdexcept>

#include "../model.hpp"

// Value references -- keep in sync with modelDescription.xml.
namespace
{
enum ValueReference : cppfmu::FMIValueReference
{
    VR_MASS = 0,      // parameter
    VR_STIFFNESS = 1, // parameter
    VR_DAMPING = 2,   // parameter
    VR_FORCE = 3,     // input
    VR_POSITION = 4,  // output
    VR_VELOCITY = 5,  // output
};
}

class MassSpringDamperSlave : public cppfmu::SlaveInstance3
{
public:
    using LoggerFunction = std::function<
        void(cppfmu::FMIStatus, cppfmu::FMIString, cppfmu::FMIString)>;

    explicit MassSpringDamperSlave(LoggerFunction logger)
        : logger_(std::move(logger))
    { }

    void SetFloat64(
        const cppfmu::FMIValueReference vr[],
        std::size_t nvr,
        const cppfmu::FMIReal value[],
        std::size_t /*nValues*/) override
    {
        for (std::size_t i = 0; i < nvr; ++i) {
            switch (vr[i]) {
            case VR_MASS:
                model_.mass = value[i];
                break;
            case VR_STIFFNESS:
                model_.stiffness = value[i];
                break;
            case VR_DAMPING:
                model_.damping = value[i];
                break;
            case VR_FORCE:
                model_.force = value[i];
                break;
            case VR_POSITION:
                model_.position = value[i];
                break;
            case VR_VELOCITY:
                model_.velocity = value[i];
                break;
            default:
                throw std::logic_error("Invalid value reference");
            }
        }
    }

    void GetFloat64(
        const cppfmu::FMIValueReference vr[],
        std::size_t nvr,
        cppfmu::FMIReal value[],
        std::size_t /*nValues*/) const override
    {
        for (std::size_t i = 0; i < nvr; ++i) {
            switch (vr[i]) {
            case VR_MASS:
                value[i] = model_.mass;
                break;
            case VR_STIFFNESS:
                value[i] = model_.stiffness;
                break;
            case VR_DAMPING:
                value[i] = model_.damping;
                break;
            case VR_FORCE:
                value[i] = model_.force;
                break;
            case VR_POSITION:
                value[i] = model_.position;
                break;
            case VR_VELOCITY:
                value[i] = model_.velocity;
                break;
            default:
                throw std::logic_error("Invalid value reference");
            }
        }
    }

    void ExitInitializationMode() override
    {
        if (model_.mass <= 0.0) {
            throw std::logic_error("mass must be positive");
        }
    }

    bool DoStep(
        cppfmu::FMIReal currentCommunicationPoint,
        cppfmu::FMIReal communicationStepSize,
        cppfmu::FMIBoolean /*noSetFMUStatePriorToCurrentPoint*/,
        cppfmu::FMIBoolean& eventHandlingNeeded,
        cppfmu::FMIBoolean& terminateSimulation,
        cppfmu::FMIBoolean& earlyReturn,
        cppfmu::FMIReal& lastSuccessfulTime) override
    {
        model_.DoStep(communicationStepSize);

        // This model has no events and never returns early, so it always completes
        // the whole step it was asked for.
        eventHandlingNeeded = cppfmu::FMIFalse;
        terminateSimulation = cppfmu::FMIFalse;
        earlyReturn = cppfmu::FMIFalse;
        lastSuccessfulTime = currentCommunicationPoint + communicationStepSize;
        return true;
    }

private:
    msd::MassSpringDamper model_;
    LoggerFunction logger_;
};

cppfmu::UniquePtr<cppfmu::SlaveInstance3> CppfmuInstantiateSlave(
    cppfmu::FMIString /*instanceName*/,
    cppfmu::FMIString /*instantiationToken*/,
    cppfmu::FMIString /*resourceLocation*/,
    cppfmu::FMIBoolean /*visible*/,
    cppfmu::FMIBoolean /*loggingOn*/,
    cppfmu::FMIBoolean /*eventModeUsed*/,
    cppfmu::FMIBoolean /*earlyReturnAllowed*/,
    const cppfmu::FMIValueReference /*requiredIntermediateVariables*/[],
    std::size_t /*nRequiredIntermediateVariables*/,
    cppfmu::FMIComponentEnvironment /*instanceEnvironment*/,
    std::function<void(cppfmu::FMIStatus, cppfmu::FMIString, cppfmu::FMIString)>
        logger)
{
    return cppfmu::AllocateUnique3<MassSpringDamperSlave>(std::move(logger));
}
