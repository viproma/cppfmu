/* Copyright 2016-2026, SINTEF Ocean.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

// The checks are assert()s: keep them in Release builds, which the conan
// recipe tests in.
#undef NDEBUG

/* FMI 3.0 Intermediate Update Mode through the C entry points.
 *
 * The slave opens an intermediate update in the middle of each step; the
 * importer's callback checks, from inside it, what FMI 3.0 allows there: get
 * the intermediate output and its derivative, then set the input; nothing
 * else, and no get after a set. The slave uses the input it was given and
 * returns early when asked to.
 */
#include <cppfmu_cs_fmi3.hpp>
#include <fmi3Functions.h>

#include <cassert>
#include <cmath>
#include <cstdio>
#include <stdexcept>

namespace
{

// Value references.
const fmi3ValueReference U = 0;         // input, intermediateUpdate
const fmi3ValueReference Y = 1;         // output, intermediateUpdate
const fmi3ValueReference MODE = 2;      // parameter: what DoStep does
const fmi3ValueReference AVAILABLE = 3; // output: 1 if updates are available

// MODE values.
const double NORMAL = 0.0;
const double OUTSIDE_STEP = 1.0; // calls the update at a time outside the step
const double GETS_ONLY = 2.0;    // opens the update for gets only

class Slave : public cppfmu::SlaveInstance3
{
public:
    void SetFloat64(
        const cppfmu::FMIValueReference vr[],
        std::size_t nvr,
        const cppfmu::FMIReal value[],
        std::size_t /*nValues*/) override
    {
        for (std::size_t i = 0; i < nvr; ++i) {
            if (vr[i] == U) {
                if (std::isnan(value[i])) {
                    throw std::runtime_error("the input cannot be NaN");
                }
                u_ = value[i];
            } else if (vr[i] == MODE) {
                mode_ = value[i];
            } else {
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
            if (vr[i] == U) {
                value[i] = u_;
            } else if (vr[i] == Y) {
                value[i] = y_;
            } else if (vr[i] == MODE) {
                value[i] = mode_;
            } else if (vr[i] == AVAILABLE) {
                value[i] = IntermediateUpdateAvailable() ? 1.0 : 0.0;
            } else {
                throw std::logic_error("Invalid value reference");
            }
        }
    }

    void GetOutputDerivatives(
        const cppfmu::FMIValueReference /*vr*/[],
        std::size_t nvr,
        const cppfmu::FMIInteger /*orders*/[],
        cppfmu::FMIReal values[],
        std::size_t /*nValues*/) const override
    {
        for (std::size_t i = 0; i < nvr; ++i) values[i] = -1.0;
    }

    bool DoStep(
        cppfmu::FMIReal t,
        cppfmu::FMIReal h,
        cppfmu::FMIBoolean /*noSetFMUStatePriorToCurrentPoint*/,
        cppfmu::FMIBoolean& eventHandlingNeeded,
        cppfmu::FMIBoolean& terminateSimulation,
        cppfmu::FMIBoolean& earlyReturn,
        cppfmu::FMIReal& lastSuccessfulTime) override
    {
        eventHandlingNeeded = cppfmu::FMIFalse;
        terminateSimulation = cppfmu::FMIFalse;
        earlyReturn = cppfmu::FMIFalse;
        lastSuccessfulTime = t + h;
        if (!IntermediateUpdateAvailable()) {
            y_ = u_;
            return true;
        }
        // The intermediate output the importer reads in the middle.
        y_ = 10.0 * u_;
        const auto time = mode_ == OUTSIDE_STEP ? t + 2 * h : t + h / 2;
        const auto result =
            IntermediateUpdate(time, mode_ != GETS_ONLY, true, true, true);
        y_ = u_; // the input the importer set in the middle
        if (result.earlyReturnRequested) {
            earlyReturn = cppfmu::FMITrue;
            lastSuccessfulTime = result.earlyReturnTime;
            return false;
        }
        return true;
    }

private:
    double u_ = 1.0;
    double y_ = 0.0;
    double mode_ = NORMAL;
};

// What the importer's callback does and saw.
struct Importer
{
    fmi3Instance instance = nullptr;
    bool requestEarlyReturn = false;
    double setValue = 3.0;
    int calls = 0;
    double time = 0.0;
    bool setRequested = false;
    double intermediateY = 0.0;
    double intermediateDerivative = 0.0;
};

extern "C" void LogMessage(
    fmi3InstanceEnvironment,
    fmi3Status,
    fmi3String category,
    fmi3String message) noexcept
{
    std::fprintf(stderr, "[%s] %s\n", category, message);
}

extern "C" void OnIntermediateUpdate(
    fmi3InstanceEnvironment environment,
    fmi3Float64 intermediateUpdateTime,
    fmi3Boolean intermediateVariableSetRequested,
    fmi3Boolean intermediateVariableGetAllowed,
    fmi3Boolean intermediateStepFinished,
    fmi3Boolean canReturnEarly,
    fmi3Boolean* earlyReturnRequested,
    fmi3Float64* earlyReturnTime) noexcept
{
    auto& importer = *static_cast<Importer*>(environment);
    const auto instance = importer.instance;
    ++importer.calls;
    importer.time = intermediateUpdateTime;
    importer.setRequested = intermediateVariableSetRequested == fmi3True;
    assert(intermediateVariableGetAllowed == fmi3True);
    assert(intermediateStepFinished == fmi3True);
    assert(canReturnEarly == fmi3True);

    // Gets first: the intermediate output and its derivative.
    assert(fmi3GetFloat64(instance, &Y, 1, &importer.intermediateY, 1) == fmi3OK);
    const fmi3Int32 order = 1;
    assert(
        fmi3GetOutputDerivatives(
            instance,
            &Y,
            1,
            &order,
            &importer.intermediateDerivative,
            1) == fmi3OK);

    // Nothing else of Step Mode: no stepping, no Jacobians, no FMU state, no
    // mode changes. Each is refused and leaves the instance as it was.
    fmi3Boolean flag = fmi3False;
    fmi3Float64 time = 0.0;
    assert(
        fmi3DoStep(instance, 0.0, 0.1, fmi3True, &flag, &flag, &flag, &time) ==
        fmi3Error);
    fmi3Float64 seed = 1.0, sensitivity = 0.0;
    assert(
        fmi3GetDirectionalDerivative(
            instance,
            &Y,
            1,
            &U,
            1,
            &seed,
            1,
            &sensitivity,
            1) == fmi3Error);
    fmi3FMUState state = nullptr;
    assert(fmi3GetFMUState(instance, &state) == fmi3Error);
    assert(fmi3EnterEventMode(instance) == fmi3Error);
    assert(fmi3Terminate(instance) == fmi3Error);

    // Then the sets, if the FMU requested them; after one, no more gets.
    const auto rc = fmi3SetFloat64(instance, &U, 1, &importer.setValue, 1);
    if (!importer.setRequested) {
        assert(rc == fmi3Error);
        return;
    }
    if (std::isnan(importer.setValue)) {
        assert(rc == fmi3Error); // the slave failed: so does the step
        return;
    }
    assert(rc == fmi3OK);
    double y = 0.0;
    assert(fmi3GetFloat64(instance, &Y, 1, &y, 1) == fmi3Error);

    if (importer.requestEarlyReturn) {
        *earlyReturnRequested = fmi3True;
        *earlyReturnTime = intermediateUpdateTime;
    }
}

fmi3Instance Instantiate(Importer* importer, bool withCallback)
{
    const auto instance = fmi3InstantiateCoSimulation(
        "intermediate",
        "token",
        "",
        fmi3False,
        fmi3False,
        fmi3False,
        fmi3True,
        withCallback ? &U : nullptr,
        withCallback ? 1 : 0,
        importer,
        &LogMessage,
        withCallback ? &OnIntermediateUpdate : nullptr);
    assert(instance);
    if (importer) importer->instance = instance;
    assert(
        fmi3EnterInitializationMode(instance, fmi3False, 0.0, 0.0, fmi3False, 0.0) ==
        fmi3OK);
    assert(fmi3ExitInitializationMode(instance) == fmi3OK);
    return instance;
}

double Get(fmi3Instance instance, fmi3ValueReference vr)
{
    double v = NAN;
    assert(fmi3GetFloat64(instance, &vr, 1, &v, 1) == fmi3OK);
    return v;
}

void Set(fmi3Instance instance, fmi3ValueReference vr, double v)
{
    assert(fmi3SetFloat64(instance, &vr, 1, &v, 1) == fmi3OK);
}

fmi3Status Step(fmi3Instance instance, double t, double h, fmi3Boolean* early, double* last)
{
    fmi3Boolean eventNeeded = fmi3False, terminate = fmi3False;
    return fmi3DoStep(instance, t, h, fmi3True, &eventNeeded, &terminate, early, last);
}

} // namespace

cppfmu::UniquePtr<cppfmu::SlaveInstance3> CppfmuInstantiateSlave(
    cppfmu::FMIString /*instanceName*/,
    cppfmu::FMIString /*instantiationToken*/,
    cppfmu::FMIString /*resourceLocation*/,
    cppfmu::FMIBoolean /*visible*/,
    cppfmu::FMIBoolean /*loggingOn*/,
    cppfmu::FMIBoolean /*eventModeUsed*/,
    cppfmu::FMIBoolean /*earlyReturnAllowed*/,
    const cppfmu::FMIValueReference[] /*requiredIntermediateVariables*/,
    std::size_t /*nRequiredIntermediateVariables*/,
    cppfmu::FMIComponentEnvironment /*instanceEnvironment*/,
    std::function<void(cppfmu::FMIStatus, cppfmu::FMIString, cppfmu::FMIString)>
    /*logger*/)
{
    return cppfmu::AllocateUnique3<Slave>();
}

int main()
{
    fmi3Boolean early = fmi3False;
    double last = 0.0;

    // Without a callback the slave sees no intermediate updates.
    {
        const auto instance = Instantiate(nullptr, false);
        assert(Get(instance, AVAILABLE) == 0.0);
        assert(Step(instance, 0.0, 0.5, &early, &last) == fmi3OK);
        assert(Get(instance, Y) == 1.0);
        fmi3FreeInstance(instance);
    }

    // An update in the middle of the step: the importer reads the
    // intermediate output, then sets the input, which the step uses.
    {
        Importer importer;
        const auto instance = Instantiate(&importer, true);
        assert(Get(instance, AVAILABLE) == 1.0);
        assert(Step(instance, 1.0, 0.5, &early, &last) == fmi3OK);
        assert(importer.calls == 1);
        assert(importer.time == 1.25);
        assert(importer.setRequested);
        assert(importer.intermediateY == 10.0);
        assert(importer.intermediateDerivative == -1.0);
        assert(early == fmi3False && last == 1.5);
        assert(Get(instance, Y) == 3.0);

        // Back in Step Mode, the calls refused in the update are legal again
        // (this slave answers a set after a get).
        double y = 0.0;
        assert(fmi3GetFloat64(instance, &Y, 1, &y, 1) == fmi3OK);
        Set(instance, U, 1.0);
        assert(fmi3GetFloat64(instance, &Y, 1, &y, 1) == fmi3OK);

        // An early return the importer asks for.
        importer.requestEarlyReturn = true;
        importer.setValue = 4.0;
        assert(Step(instance, 1.5, 0.5, &early, &last) == fmi3Discard);
        assert(early == fmi3True && last == 1.75);
        assert(Get(instance, Y) == 4.0);
        importer.requestEarlyReturn = false;

        // An update opened for gets only refuses the set.
        Set(instance, MODE, GETS_ONLY);
        assert(Step(instance, 1.75, 0.25, &early, &last) == fmi3OK);
        assert(!importer.setRequested);
        assert(Get(instance, U) == 4.0);
        Set(instance, MODE, NORMAL);

        // A set that fails the slave fails the instance, and so the step.
        importer.setValue = NAN;
        assert(Step(instance, 2.0, 0.5, &early, &last) == fmi3Error);
        assert(fmi3GetFloat64(instance, &Y, 1, &last, 1) == fmi3Error);
        fmi3FreeInstance(instance);
    }

    // An update at a time outside the step is the slave's error.
    {
        Importer importer;
        const auto instance = Instantiate(&importer, true);
        Set(instance, MODE, OUTSIDE_STEP);
        assert(Step(instance, 0.0, 0.5, &early, &last) == fmi3Error);
        assert(importer.calls == 0);
        fmi3FreeInstance(instance);
    }

    return 0;
}
