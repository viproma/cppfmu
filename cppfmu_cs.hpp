/* Copyright 2016-2017, SINTEF Ocean.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
#ifndef CPPFMU_CS_HPP
#define CPPFMU_CS_HPP

#include <vector>
#include "cppfmu_common.hpp"

namespace cppfmu
{

/* ============================================================================
 * CO-SIMULATION INTERFACE
 * ============================================================================
 */

/* A base class for co-simulation slave instances.
 *
 * To implement a co-simulation slave, create a class which publicly derives
 * from this one and override its virtual methods as required.  DoStep() is
 * the only function which it is mandatory to override.
 *
 * The methods map directly to the C functions defined by FMI 1.0, so the
 * documentation here is intentionally sparse.  We refer to the FMI 1.0
 * specification for detailed information.
 */
class SlaveInstance
{
public:
    // Called from fmiInitializeSlave(). Does nothing by default.
    virtual void Initialize(
        fmiReal tStart,
        fmiBoolean stopTimeDefined,
        fmiReal tStop);

    // Called from fmiTerminateSlave(). Does nothing by default.
    virtual void Terminate();

    // Called from fmiResetSlave(). Does nothing by default.
    virtual void Reset();

    // Called from fmiSetXxx(). Throw std::logic_error by default.
    virtual void SetReal(
        const fmiValueReference vr[],
        std::size_t nvr,
        const fmiReal value[]);
    virtual void SetInteger(
        const fmiValueReference vr[],
        std::size_t nvr,
        const fmiInteger value[]);
    virtual void SetBoolean(
        const fmiValueReference vr[],
        std::size_t nvr,
        const fmiBoolean value[]);
    virtual void SetString(
        const fmiValueReference vr[],
        std::size_t nvr,
        const fmiString value[]);

    // Called from fmiGetXxx(). Throw std::logic_error by default.
    virtual void GetReal(
        const fmiValueReference vr[],
        std::size_t nvr,
        fmiReal value[]) const;
    virtual void GetInteger(
        const fmiValueReference vr[],
        std::size_t nvr,
        fmiInteger value[]) const;
    virtual void GetBoolean(
        const fmiValueReference vr[],
        std::size_t nvr,
        fmiBoolean value[]) const;
    virtual void GetString(
        const fmiValueReference vr[],
        std::size_t nvr,
        fmiString value[]) const;

    // Called from fmiDoStep(). Must be implemented in model code.
    virtual bool DoStep(
        fmiReal currentCommunicationPoint,
        fmiReal communicationStepSize,
        fmiBoolean newStep,
        fmiReal& endOfStep) = 0;

    // The instance is destroyed in fmiFreeSlaveInstance().
    virtual ~SlaveInstance() CPPFMU_NOEXCEPT;
};

} // namespace cppfmu


/* A function which must be defined by model code, and which should create
 * and return a new slave instance.
 *
 * The returned instance must be managed by a std::unique_ptr with a deleter
 * of type std::function<void(void*)> that takes care of freeing the memory.
 * The simplest way to set this up is to use cppfmu::AllocateUnique() to
 * create the slave instance.
 *
 * Most of its parameters correspond to those of fmiInstantiateSlave(),
 * except that 'functions' and 'loggingOn' have been replaced with more
 * convenient types:
 *
 *     memory = An object which the model code can use for memory management,
 *              typically in conjunction with cppfmu::Allocator,
 *              cppfmu::AllocateUnique(), etc.  Allocation and deallocation
 *              requests get forwarded to the simulation environment.
 *
 *     logger = An object which the model code can use to log messages (e.g.
 *              warnings or debug info).  The messages are forwarded to the
 *              simulation environment's logging facilities.
 *
 * Note that this function is declared in the global namespace.
 */
cppfmu::UniquePtr<cppfmu::SlaveInstance> CppfmuInstantiateSlave(
    fmiString  instanceName,
    fmiString  fmuGUID,
    fmiString  fmuLocation,
    fmiString  mimeType,
    fmiReal    timeout,
    fmiBoolean visible,
    fmiBoolean interactive,
    cppfmu::Memory memory,
    cppfmu::Logger logger);


#endif // header guard
