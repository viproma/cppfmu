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


extern "C" void logger(
    fmi2ComponentEnvironment,
    fmi2String instanceName,
    fmi2Status status,
    fmi2String category,
    fmi2String message,
    ...) noexcept
{
    assert(std::string(instanceName) == std::string(TEST_INSTANCE_NAME));
    va_list args;
    va_start(args, message);
    std::vfprintf(stderr, message, args);
    std::fprintf(stderr, "\n");
    va_end(args);
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

    // Termination (FMI 2.0)
    const auto terminateResult = fmi2Terminate(instance);
    assert(terminateResult == fmi2OK);

    fmi2FreeInstance(instance);
#endif
    return 0;
}
