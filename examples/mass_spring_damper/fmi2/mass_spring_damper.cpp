/* Copyright 2026, SINTEF Ocean.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

/* FMI 2.0 mass-spring-damper co-simulation slave.
 *
 * This is a thin adapter over msd::MassSpringDamper (see ../model.hpp). It
 * shows the three things every cppfmu FMI 2.0 model needs:
 *
 *   1. A class deriving from cppfmu::SlaveInstance.
 *   2. Get/Set overrides that map FMI value references to model fields.
 *   3. A CppfmuInstantiateSlave() factory function.
 *
 * The value references below MUST match modelDescription.xml.
 */

#include <cppfmu_cs.hpp>

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

class MassSpringDamperSlave : public cppfmu::SlaveInstance
{
public:
    explicit MassSpringDamperSlave(cppfmu::Logger logger)
        : logger_(logger)
    { }

    void SetReal(
        const cppfmu::FMIValueReference vr[],
        std::size_t nvr,
        const cppfmu::FMIReal value[]) override
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

    void GetReal(
        const cppfmu::FMIValueReference vr[],
        std::size_t nvr,
        cppfmu::FMIReal value[]) const override
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
        cppfmu::FMIReal /*currentCommunicationPoint*/,
        cppfmu::FMIReal communicationStepSize,
        cppfmu::FMIBoolean /*newStep*/,
        cppfmu::FMIReal& /*endOfStep*/) override
    {
        model_.DoStep(communicationStepSize);
        return true;
    }

private:
    msd::MassSpringDamper model_;
    cppfmu::Logger logger_;
};

cppfmu::UniquePtr<cppfmu::SlaveInstance> CppfmuInstantiateSlave(
    cppfmu::FMIString /*instanceName*/,
    cppfmu::FMIString /*fmuGUID*/,
    cppfmu::FMIString /*fmuResourceLocation*/,
    cppfmu::FMIString /*mimeType*/,
    cppfmu::FMIReal /*timeout*/,
    cppfmu::FMIBoolean /*visible*/,
    cppfmu::FMIBoolean /*interactive*/,
    cppfmu::Memory memory,
    cppfmu::Logger logger)
{
    return cppfmu::AllocateUnique<MassSpringDamperSlave>(memory, logger);
}
