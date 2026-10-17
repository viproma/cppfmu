// The checks are assert()s: keep them in Release builds, which the conan
// recipe tests in.
#undef NDEBUG

#ifdef CPPFMU_USE_FMI_1_0
#   define MODEL_IDENTIFIER
extern "C"
{
#   include <fmiFunctions.h>
}
#else
#   include <fmi2Functions.h>
#endif

#include <cassert>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>


// The category the wrapper logs with, as fmi_functions.cpp is compiled.
#ifndef CPPFMU_LOG_CATEGORY
#   define CPPFMU_LOG_CATEGORY "cppfmu"
#endif
const char* const wrapperCategory = CPPFMU_LOG_CATEGORY;


#ifdef CPPFMU_USE_FMI_1_0
const double TEST_VALUE = 2.0;
const char* const TEST_INSTANCE_NAME = "MyInstance";


extern "C" void logger(
    fmiComponent c,
    fmiString instanceName,
    fmiStatus status,
    fmiString category,
    fmiString message,
    ...) noexcept
{
    assert(std::string(instanceName) == std::string(TEST_INSTANCE_NAME));
    va_list args;
    va_start(args, message);
    std::vfprintf(stderr, message, args);
    std::fprintf(stderr, "\n");
    va_end(args);
}
#else
const double TEST_VALUE = 2.0;
const char* const TEST_INSTANCE_NAME = "MyInstance";

// The last message and category the FMU logged, formatted, and the format
// as the FMU passed it (what a logger that does not format shows).
std::string lastLogMessage;
std::string lastLogFormat;
std::string lastLogCategory;


extern "C" void logger(
    fmi2ComponentEnvironment,
    fmi2String instanceName,
    fmi2Status status,
    fmi2String category,
    fmi2String message,
    ...) noexcept
{
    assert(std::string(instanceName) == std::string(TEST_INSTANCE_NAME));
    char buffer[1024];
    va_list args;
    va_start(args, message);
    std::vsnprintf(buffer, sizeof buffer, message, args);
    va_end(args);
    lastLogMessage = buffer;
    lastLogFormat = message;
    lastLogCategory = category ? category : "(null)";
    std::fprintf(stderr, "%s\n", buffer);
}
#endif

extern "C" void* alloc(std::size_t nobj, std::size_t size) noexcept
{
    return std::malloc(nobj * size);
}


int main()
{
#ifdef CPPFMU_USE_FMI_1_0
    // Instantiation and setup (FMI 1.0)
    fmiCallbackFunctions callbacks = {
        &logger,
        &alloc,
        &std::free,
    };
    const auto instance = fmiInstantiateSlave(
        TEST_INSTANCE_NAME,
        "04b947f3-c057-4860-b59b-eb0bd6fa52be",
        "",
        "application/x-fmu-sharedlibrary",
        0.0,
        fmiFalse,
        fmiFalse,
        callbacks,
        fmiTrue);
    assert(instance);

    // Initialization (FMI 1.0)
    {
        const auto rc = fmiInitializeSlave(instance, 0.0, fmiFalse, 0.0);
        assert(rc == fmiOK);
    }

    const fmiValueReference validVr = 0;
    const fmiReal value1 = 1.0;
    {
        const auto rc = fmiSetReal(instance, &validVr, 1, &value1);
        assert(rc == fmiOK);
    }
    {
        fmiReal val = 0.0;
        const auto rc = fmiGetReal(instance, &validVr, 1, &val);
        assert(rc == fmiOK);
        assert(val == value1);
    }

    // Derivative APIs — default "not supported" path (FMI 1.0)
    // Note: fmiGetDirectionalDerivative is not part of FMI 1.0 spec
    {
        const fmiValueReference vr[] = {0};
        fmiInteger order[] = {1};
        fmiReal value[] = {1.0};
        auto rc = fmiSetRealInputDerivatives(instance, vr, 1, order, value);
        assert(rc == fmiError);
    }
    {
        const fmiValueReference vr[] = {0};
        fmiInteger order[] = {1};
        fmiReal value[] = {0.0};
        auto rc = fmiGetRealOutputDerivatives(instance, vr, 1, order, value);
        assert(rc == fmiError);
    }

    // Enable derivative support (vr=1) and set seed (vr=2) (FMI 1.0)
    {
        const fmiValueReference enableVr[] = {1};
        const fmiBoolean enableVal = fmiTrue;
        auto rc = fmiSetBoolean(instance, enableVr, 1, &enableVal);
        assert(rc == fmiOK);
    }
    {
        const fmiValueReference seedVr[] = {2};
        const fmiReal seedVal = 2.0;
        auto rc = fmiSetReal(instance, seedVr, 1, &seedVal);
        assert(rc == fmiOK);
    }

    // Verify derivative support flag via GetBoolean
    {
        const fmiValueReference enableVr[] = {1};
        fmiBoolean val = fmiFalse;
        auto rc = fmiGetBoolean(instance, enableVr, 1, &val);
        assert(rc == fmiOK);
        assert(val == fmiTrue);
    }

    // fmiSetRealInputDerivatives — success (FMI 1.0)
    {
        const fmiValueReference vr[] = {0};
        fmiInteger order[] = {1};
        fmiReal value[] = {3.14};
        auto rc = fmiSetRealInputDerivatives(instance, vr, 1, order, value);
        assert(rc == fmiOK);
    }

    // fmiGetRealOutputDerivatives — success, value = seed * order = 2.0 * 1 = 2.0 (FMI 1.0)
    {
        const fmiValueReference vr[] = {0};
        fmiInteger order[] = {1};
        fmiReal value[] = {0.0};
        auto rc = fmiGetRealOutputDerivatives(instance, vr, 1, order, value);
        assert(rc == fmiOK);
        assert(value[0] == 2.0);
    }

    // Simulation (FMI 1.0)
    {
        const auto rc = fmiDoStep(instance, 0.0, 0.1, fmiTrue);
        assert(rc == fmiOK);
    }
    const fmiReal value2 = 2.0;
    {
        const auto rc = fmiSetReal(instance, &validVr, 1, &value2);
        assert(rc == fmiOK);
    }
    {
        const auto rc = fmiDoStep(instance, 0.0, 0.1, fmiTrue);
        assert(rc == fmiOK);
    }
    {
        fmiReal val = 0.0;
        const auto rc = fmiGetReal(instance, &validVr, 1, &val);
        assert(rc == fmiOK);
        assert(val == value2);
    }
    {
        const fmiValueReference invalidVr = 1;
        fmiReal val = -1.0;
        const auto rc = fmiGetReal(instance, &invalidVr, 1, &val);
        assert(rc == fmiError);
        std::fprintf(stderr, "(The last error was expected.)\n");
        assert(val == -1.0);
    }

    // Termination (FMI 1.0)
    const auto terminateResult = fmiTerminateSlave(instance);
    assert(terminateResult == fmiOK);

    fmiFreeSlaveInstance(instance);
#else
    // Instantiation and setup (FMI 2.0)
    const auto callbacks = fmi2CallbackFunctions{
        &logger,
        &alloc,
        &std::free,
        nullptr,
        nullptr,
    };
    const auto instance = fmi2Instantiate(
        TEST_INSTANCE_NAME,
        fmi2CoSimulation,
        "04b947f3-c057-4860-b59b-eb0bd6fa52be",
        nullptr,
        &callbacks,
        fmi2False,
        fmi2True);
    assert(instance);

    {
        const auto rc =
            fmi2SetupExperiment(instance, fmi2False, 0.0, 0.0, fmi2False, 0.0);
        assert(rc == fmi2OK);
    }

    // Initialization (FMI 2.0)
    {
        const auto rc = fmi2EnterInitializationMode(instance);
        assert(rc == fmi2OK);
    }

    const fmi2ValueReference validVr = 0;
    const fmi2Real value1 = 1.0;
    {
        const auto rc = fmi2SetReal(instance, &validVr, 1, &value1);
        assert(rc == fmi2OK);
    }
    {
        fmi2Real val = 0.0;
        const auto rc = fmi2GetReal(instance, &validVr, 1, &val);
        assert(rc == fmi2OK);
        assert(val == value1);
    }

    {
        const auto rc = fmi2ExitInitializationMode(instance);
        assert(rc == fmi2OK);
    }

    // Derivative APIs — default "not supported" path (FMI 2.0)
    {
        const fmi2ValueReference vr[] = {0};
        fmi2Real dvKnown[] = {1.0};
        fmi2Real dvUnknown[] = {0.0};
        auto rc = fmi2GetDirectionalDerivative(
            instance,
            vr,
            1,
            vr,
            1,
            dvKnown,
            dvUnknown);
        assert(rc == fmi2Error);
    }
    {
        const fmi2ValueReference vr[] = {0};
        fmi2Integer order[] = {1};
        fmi2Real value[] = {1.0};
        auto rc = fmi2SetRealInputDerivatives(instance, vr, 1, order, value);
        assert(rc == fmi2Error);
    }
    {
        const fmi2ValueReference vr[] = {0};
        fmi2Integer order[] = {1};
        fmi2Real value[] = {0.0};
        auto rc = fmi2GetRealOutputDerivatives(instance, vr, 1, order, value);
        assert(rc == fmi2Error);
    }

    // Enable derivative support (vr=1) and set seed (vr=2) (FMI 2.0)
    {
        const fmi2ValueReference enableVr[] = {1};
        const fmi2Boolean enableVal = fmi2True;
        auto rc = fmi2SetBoolean(instance, enableVr, 1, &enableVal);
        assert(rc == fmi2OK);
    }
    {
        const fmi2ValueReference seedVr[] = {2};
        const fmi2Real seedVal = 2.0;
        auto rc = fmi2SetReal(instance, seedVr, 1, &seedVal);
        assert(rc == fmi2OK);
    }

    // Verify derivative support flag via GetBoolean
    {
        const fmi2ValueReference enableVr[] = {1};
        fmi2Boolean val = fmi2False;
        auto rc = fmi2GetBoolean(instance, enableVr, 1, &val);
        assert(rc == fmi2OK);
        assert(val == fmi2True);
    }

    // fmi2SetRealInputDerivatives — success (FMI 2.0)
    {
        const fmi2ValueReference vr[] = {0};
        fmi2Integer order[] = {1};
        fmi2Real value[] = {3.14};
        auto rc = fmi2SetRealInputDerivatives(instance, vr, 1, order, value);
        assert(rc == fmi2OK);
    }

    // fmi2GetRealOutputDerivatives — success, value = seed * order = 2.0 * 1 = 2.0 (FMI 2.0)
    {
        const fmi2ValueReference vr[] = {0};
        fmi2Integer order[] = {1};
        fmi2Real value[] = {0.0};
        auto rc = fmi2GetRealOutputDerivatives(instance, vr, 1, order, value);
        assert(rc == fmi2OK);
        assert(value[0] == 2.0);
    }

    // fmi2GetDirectionalDerivative — success, dvUnknown = seed * dvKnown = 2.0 * 5.0 = 10.0 (FMI 2.0)
    {
        const fmi2ValueReference vUnknown[] = {0};
        const fmi2ValueReference vKnown[] = {0};
        fmi2Real dvKnown[] = {5.0};
        fmi2Real dvUnknown[] = {0.0};
        auto rc = fmi2GetDirectionalDerivative(
            instance,
            vUnknown,
            1,
            vKnown,
            1,
            dvKnown,
            dvUnknown);
        assert(rc == fmi2OK);
        assert(dvUnknown[0] == 10.0);
    }

    // Save state (FMI 2.0)
    fmi2FMUstate state = nullptr;
    {
        const auto rc = fmi2GetFMUstate(instance, &state);
        assert(rc == fmi2OK);
        assert(state != nullptr);
    }
    std::size_t stateSize = 0;
    {
        const auto rc = fmi2SerializedFMUstateSize(instance, state, &stateSize);
        assert(rc == fmi2OK);
    }
    assert(stateSize > 0);
    auto serializedState = std::vector<fmi2Byte>(stateSize);
    {
        const auto rc = fmi2SerializeFMUstate(
            instance,
            state,
            serializedState.data(),
            serializedState.size());
        assert(rc == fmi2OK);
    }
    {
        const auto rc = fmi2FreeFMUstate(instance, &state);
        assert(rc == fmi2OK);
        assert(state == nullptr);
    }

    // Simulation (FMI 2.0)
    {
        const auto rc = fmi2DoStep(instance, 0.0, 0.1, fmi2False);
        assert(rc == fmi2OK);
    }
    const fmi2Real value2 = 2.0;
    {
        const auto rc = fmi2SetReal(instance, &validVr, 1, &value2);
        assert(rc == fmi2OK);
    }
    {
        const auto rc = fmi2DoStep(instance, 0.0, 0.1, fmi2False);
        assert(rc == fmi2OK);
    }
    {
        fmi2Real val = 0.0;
        const auto rc = fmi2GetReal(instance, &validVr, 1, &val);
        assert(rc == fmi2OK);
        assert(val == value2);
    }
    fmi2FMUstate restoredState = nullptr;
    {
        const auto rc = fmi2DeSerializeFMUstate(
            instance,
            serializedState.data(),
            serializedState.size(),
            &restoredState);
        assert(rc == fmi2OK);
        assert(restoredState != nullptr);
    }
    {
        const auto rc = fmi2SetFMUstate(instance, restoredState);
        assert(rc == fmi2OK);
    }
    {
        const auto rc = fmi2FreeFMUstate(instance, &restoredState);
        assert(rc == fmi2OK);
        assert(restoredState == nullptr);
    }
    {
        fmi2Real val = 0.0;
        const auto rc = fmi2GetReal(instance, &validVr, 1, &val);
        assert(rc == fmi2OK);
        assert(val == value1);
    }
    {
        const fmi2ValueReference invalidVr = 1;
        fmi2Real val = -1.0;
        const auto rc = fmi2GetReal(instance, &invalidVr, 1, &val);
        assert(rc == fmi2Error);
        std::fprintf(stderr, "(The last error was expected.)\n");
        assert(val == -1.0);
    }

    // An exception message is logged as text, not used as a printf format.
    {
        const fmi2ValueReference throwingVr = 3;
        const fmi2Real val = 0.0;
        const auto rc = fmi2SetReal(instance, &throwingVr, 1, &val);
        assert(rc == fmi2Error);
        assert(lastLogMessage == "fmi2SetReal: %s%s%n");
        // The text is the format, with '%' doubled, so that a logger that
        // does not format shows the text too.
        assert(lastLogFormat == "fmi2SetReal: %%s%%s%%n");
        assert(
            lastLogCategory ==
            (wrapperCategory ? wrapperCategory : std::string("(null)")));
    }

    // Messages, errors included, reach the importer only while logging is on
    // for their category.
    {
        const fmi2ValueReference throwingVr = 3;
        const fmi2Real val = 0.0;
        const fmi2String wrapperCategories[] = {wrapperCategory};
        const fmi2String otherCategory[] = {"other"};

        assert(fmi2SetDebugLogging(instance, fmi2False, 0, nullptr) == fmi2OK);
        lastLogMessage.clear();
        assert(fmi2SetReal(instance, &throwingVr, 1, &val) == fmi2Error);
        assert(lastLogMessage.empty());

        assert(
            fmi2SetDebugLogging(instance, fmi2True, 1, otherCategory) ==
            fmi2OK);
        assert(fmi2SetReal(instance, &throwingVr, 1, &val) == fmi2Error);
        assert(lastLogMessage.empty());

        // A NULL category cannot be picked out; the step above showed that it
        // is off while other categories are.
        if (wrapperCategory) {
            assert(
                fmi2SetDebugLogging(instance, fmi2True, 1, wrapperCategories) ==
                fmi2OK);
            assert(fmi2SetReal(instance, &throwingVr, 1, &val) == fmi2Error);
            assert(lastLogMessage == "fmi2SetReal: %s%s%n");
        }

        assert(fmi2SetDebugLogging(instance, fmi2True, 0, nullptr) == fmi2OK);
    }

    // A step size that is not positive is refused.
    {
        const auto rc = fmi2DoStep(instance, 0.1, 0.0, fmi2False);
        assert(rc == fmi2Error);
        std::fprintf(stderr, "(The last error was expected.)\n");
    }
    {
        const auto rc = fmi2DoStep(instance, 0.1, -0.1, fmi2False);
        assert(rc == fmi2Error);
        std::fprintf(stderr, "(The last error was expected.)\n");
    }

    // fmi2Terminated is answered, as importers ask for it after a discard.
    {
        fmi2Boolean terminated = fmi2True;
        const auto rc =
            fmi2GetBooleanStatus(instance, fmi2Terminated, &terminated);
        assert(rc == fmi2OK);
        assert(terminated == fmi2False);
    }
    {
        fmi2Boolean value = fmi2False;
        const auto rc =
            fmi2GetBooleanStatus(instance, fmi2DoStepStatus, &value);
        assert(rc == fmi2Error);
        std::fprintf(stderr, "(The last error was expected.)\n");
    }

    // Freeing a NULL FMU state is ignored.
    {
        fmi2FMUstate nullState = nullptr;
        const auto rc = fmi2FreeFMUstate(instance, &nullState);
        assert(rc == fmi2OK);
    }

    // Termination (FMI 2.0)
    const auto terminateResult = fmi2Terminate(instance);
    assert(terminateResult == fmi2OK);

    fmi2FreeInstance(instance);

    // Freeing a NULL instance is ignored.
    fmi2FreeInstance(nullptr);

    // The FMI 2.0 Co-Simulation state machine (4.2.4): a call made in a
    // state that does not allow it is refused, and changes nothing.
    {
        const auto inst = fmi2Instantiate(
            TEST_INSTANCE_NAME,
            fmi2CoSimulation,
            "04b947f3-c057-4860-b59b-eb0bd6fa52be",
            nullptr,
            &callbacks,
            fmi2False,
            fmi2True);
        assert(inst);
        fmi2Real val = 0.0;

        // Instantiated
        assert(fmi2DoStep(inst, 0.0, 0.1, fmi2False) == fmi2Error); // S001
        assert(
            lastLogMessage ==
            "fmi2DoStep: was called in a state that does not allow it");
        assert(fmi2ExitInitializationMode(inst) == fmi2Error); // S002
        assert(fmi2GetReal(inst, &validVr, 1, &val) == fmi2Error);
        assert(fmi2SetReal(inst, &validVr, 1, &value1) == fmi2OK);
        assert(
            fmi2SetupExperiment(inst, fmi2False, 0.0, 0.0, fmi2False, 0.0) ==
            fmi2OK);
        assert(fmi2EnterInitializationMode(inst) == fmi2OK);

        // Initialization Mode
        assert(fmi2DoStep(inst, 0.0, 0.1, fmi2False) == fmi2Error); // S003
        assert(fmi2EnterInitializationMode(inst) == fmi2Error);     // S004
        assert(
            fmi2SetupExperiment(inst, fmi2False, 0.0, 0.0, fmi2False, 0.0) ==
            fmi2Error);
        assert(fmi2GetReal(inst, &validVr, 1, &val) == fmi2OK);
        assert(val == value1);
        assert(fmi2ExitInitializationMode(inst) == fmi2OK);

        // stepComplete
        assert(fmi2EnterInitializationMode(inst) == fmi2Error); // S005
        assert(fmi2ExitInitializationMode(inst) == fmi2Error);  // S006
        assert(fmi2DoStep(inst, 0.0, 0.1, fmi2False) == fmi2OK);

        // A discarded step still allows the next one; FMI 2.0 is not clear
        // about what may follow a discard, so nothing is refused after one.
        assert(fmi2DoStep(inst, 0.1, 2.0, fmi2False) == fmi2Discard);
        fmi2Boolean terminated = fmi2True;
        assert(
            fmi2GetBooleanStatus(inst, fmi2Terminated, &terminated) == fmi2OK);
        assert(terminated == fmi2False);
        fmi2Real lastTime = 0.0;
        assert(
            fmi2GetRealStatus(inst, fmi2LastSuccessfulTime, &lastTime) ==
            fmi2OK);
        assert(lastTime == 1.1);
        assert(fmi2DoStep(inst, 1.1, 0.1, fmi2False) == fmi2OK);

        // An error does not change the state either.
        const fmi2ValueReference throwingVr = 3;
        assert(fmi2SetReal(inst, &throwingVr, 1, &value1) == fmi2Error);
        assert(fmi2DoStep(inst, 1.2, 0.1, fmi2False) == fmi2OK);

        fmi2FMUstate stepState = nullptr;
        assert(fmi2GetFMUstate(inst, &stepState) == fmi2OK);
        assert(fmi2Terminate(inst) == fmi2OK);

        // Terminated
        assert(fmi2DoStep(inst, 1.3, 0.1, fmi2False) == fmi2Error); // S008
        assert(fmi2SetReal(inst, &validVr, 1, &value2) == fmi2Error);
        assert(fmi2Terminate(inst) == fmi2Error);
        assert(fmi2GetReal(inst, &validVr, 1, &val) == fmi2OK);
        assert(val == value1);

        // Reset leads back to Instantiated, and a new run works.
        assert(fmi2Reset(inst) == fmi2OK);
        assert(fmi2DoStep(inst, 0.0, 0.1, fmi2False) == fmi2Error);
        assert(fmi2EnterInitializationMode(inst) == fmi2OK);
        assert(fmi2ExitInitializationMode(inst) == fmi2OK);
        assert(fmi2DoStep(inst, 0.0, 0.1, fmi2False) == fmi2OK);

        // A state set into a freshly reset instance, as fmusim
        // --initial-fmu-state-file does, can be stepped on at once: the
        // state does not record its mode, so every call is allowed until
        // the instance reaches a known one.
        assert(fmi2Reset(inst) == fmi2OK);
        assert(fmi2SetFMUstate(inst, stepState) == fmi2OK);
        assert(fmi2GetReal(inst, &validVr, 1, &val) == fmi2OK);
        assert(fmi2DoStep(inst, 1.3, 0.1, fmi2False) == fmi2OK);
        // ... after which it is in stepComplete again.
        assert(fmi2EnterInitializationMode(inst) == fmi2Error);
        assert(fmi2FreeFMUstate(inst, &stepState) == fmi2OK);

        std::fprintf(stderr, "(The errors above were expected.)\n");
        fmi2FreeInstance(inst);
    }

    // NULL memory callbacks are allowed when the FMU declares
    // canNotUseMemoryManagementFunctions (fmusim always passes NULL).
    {
        const auto nullMemoryCallbacks = fmi2CallbackFunctions{
            &logger,
            nullptr,
            nullptr,
            nullptr,
            nullptr,
        };
        const auto instance2 = fmi2Instantiate(
            TEST_INSTANCE_NAME,
            fmi2CoSimulation,
            "04b947f3-c057-4860-b59b-eb0bd6fa52be",
            nullptr,
            &nullMemoryCallbacks,
            fmi2False,
            fmi2False);
        assert(instance2);
        assert(fmi2SetReal(instance2, &validVr, 1, &value2) == fmi2OK);

        // Instantiated with loggingOn = fmi2False, so the error is not logged.
        const fmi2ValueReference throwingVr = 3;
        lastLogMessage.clear();
        assert(fmi2SetReal(instance2, &throwingVr, 1, &value2) == fmi2Error);
        assert(lastLogMessage.empty());

        fmi2FMUstate state2 = nullptr;
        assert(fmi2GetFMUstate(instance2, &state2) == fmi2OK);
        assert(fmi2FreeFMUstate(instance2, &state2) == fmi2OK);
        fmi2FreeInstance(instance2);
    }
#endif
    return 0;
}
