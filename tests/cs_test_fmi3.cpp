#include <fmi3Functions.h>

#include <cassert>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>


const double TEST_VALUE = 2.0;
const char* const TEST_INSTANCE_NAME = "MyInstance";


extern "C" void logger(
    fmi3InstanceEnvironment,
    fmi3Status status,
    fmi3String category,
    fmi3String message) noexcept
{
    std::fprintf(stderr, "[%s] %s\n", category, message);
}


int main()
{
    // Instantiation and setup (FMI 3.0)
    const auto instance = fmi3InstantiateCoSimulation(
        TEST_INSTANCE_NAME,
        "04b947f3-c057-4860-b59b-eb0bd6fa52be",
        "",
        fmi3False,
        fmi3True,
        fmi3False,
        fmi3False,
        nullptr,
        0,
        nullptr,
        &logger,
        nullptr);
    assert(instance);

    // Initialization (FMI 3.0)
    {
        const auto rc = fmi3EnterInitializationMode(
            instance, fmi3False, 0.0, 0.0, fmi3False, 0.0);
        assert(rc == fmi3OK);
    }
    {
        const auto rc = fmi3ExitInitializationMode(instance);
        assert(rc == fmi3OK);
    }

    const fmi3ValueReference validVr = 0;
    const fmi3Float64 value1 = 1.0;
    {
        const auto rc = fmi3SetFloat64(instance, &validVr, 1, &value1, 1);
        assert(rc == fmi3OK);
    }
    {
        fmi3Float64 val = 0.0;
        const auto rc = fmi3GetFloat64(instance, &validVr, 1, &val, 1);
        assert(rc == fmi3OK);
        assert(val == value1);
    }

    // Derivative APIs — default "not supported" path (FMI 3.0)
    {
        const fmi3ValueReference vr[] = {0};
        fmi3Float64 dvKnown[] = {1.0};
        fmi3Float64 dvUnknown[] = {0.0};
        auto rc = fmi3GetDirectionalDerivative(instance, vr, 1, vr, 1, dvKnown, 1, dvUnknown, 1);
        assert(rc == fmi3Error);
    }
    {
        const fmi3ValueReference vr[] = {0};
        fmi3Int32 order[] = {1};
        fmi3Float64 value[] = {0.0};
        auto rc = fmi3GetOutputDerivatives(instance, vr, 1, order, value, 1);
        assert(rc == fmi3Error);
    }

    // Enable derivative support (vr=1) and set seed (vr=2) (FMI 3.0)
    {
        const fmi3ValueReference enableVr[] = {1};
        const fmi3Float64 enableVal = 1.0;
        auto rc = fmi3SetFloat64(instance, enableVr, 1, &enableVal, 1);
        assert(rc == fmi3OK);
    }
    {
        const fmi3ValueReference seedVr[] = {2};
        const fmi3Float64 seedVal = 2.0;
        auto rc = fmi3SetFloat64(instance, seedVr, 1, &seedVal, 1);
        assert(rc == fmi3OK);
    }

    // fmi3GetOutputDerivatives — success, value = seed * order = 2.0 * 1 = 2.0 (FMI 3.0)
    {
        const fmi3ValueReference vr[] = {0};
        fmi3Int32 order[] = {1};
        fmi3Float64 value[] = {0.0};
        auto rc = fmi3GetOutputDerivatives(instance, vr, 1, order, value, 1);
        assert(rc == fmi3OK);
        assert(value[0] == 2.0);
    }

    // fmi3GetDirectionalDerivative — success, dvUnknown = seed * dvKnown = 2.0 * 5.0 = 10.0 (FMI 3.0)
    {
        const fmi3ValueReference vUnknown[] = {0};
        const fmi3ValueReference vKnown[] = {0};
        fmi3Float64 dvKnown[] = {5.0};
        fmi3Float64 dvUnknown[] = {0.0};
        auto rc = fmi3GetDirectionalDerivative(instance, vUnknown, 1, vKnown, 1, dvKnown, 1, dvUnknown, 1);
        assert(rc == fmi3OK);
        assert(dvUnknown[0] == 10.0);
    }

    // Save state (FMI 3.0)
    fmi3FMUState state = nullptr;
    {
        const auto rc = fmi3GetFMUState(instance, &state);
        assert(rc == fmi3OK);
        assert(state != nullptr);
    }
    std::size_t stateSize = 0;
    {
        const auto rc = fmi3SerializedFMUStateSize(instance, state, &stateSize);
        assert(rc == fmi3OK);
    }
    assert(stateSize > 0);
    auto serializedState = std::vector<fmi3Byte>(stateSize);
    {
        const auto rc = fmi3SerializeFMUState(
            instance, state, serializedState.data(), serializedState.size());
        assert(rc == fmi3OK);
    }
    {
        const auto rc = fmi3FreeFMUState(instance, &state);
        assert(rc == fmi3OK);
        assert(state == nullptr);
    }

    // Simulation (FMI 3.0)
    {
        fmi3Boolean eventHandlingNeeded = fmi3True;
        fmi3Boolean terminateSimulation = fmi3True;
        fmi3Boolean earlyReturn = fmi3True;
        fmi3Float64 lastSuccessfulTime = 0.0;
        const auto rc = fmi3DoStep(
            instance, 0.0, 0.1, fmi3False,
            &eventHandlingNeeded, &terminateSimulation, &earlyReturn, &lastSuccessfulTime);
        assert(rc == fmi3OK);
        assert(eventHandlingNeeded == fmi3False);
        assert(terminateSimulation == fmi3False);
        assert(earlyReturn == fmi3False);
    }
    const fmi3Float64 value2 = 2.0;
    {
        const auto rc = fmi3SetFloat64(instance, &validVr, 1, &value2, 1);
        assert(rc == fmi3OK);
    }
    {
        const auto rc = fmi3DoStep(
            instance, 0.0, 0.1, fmi3False,
            nullptr, nullptr, nullptr, nullptr);
        assert(rc == fmi3OK);
    }
    {
        fmi3Float64 val = 0.0;
        const auto rc = fmi3GetFloat64(instance, &validVr, 1, &val, 1);
        assert(rc == fmi3OK);
        assert(val == value2);
    }
    fmi3FMUState restoredState = nullptr;
    {
        const auto rc = fmi3DeserializeFMUState(
            instance, serializedState.data(), serializedState.size(), &restoredState);
        assert(rc == fmi3OK);
        assert(restoredState != nullptr);
    }
    {
        const auto rc = fmi3SetFMUState(instance, restoredState);
        assert(rc == fmi3OK);
    }
    {
        const auto rc = fmi3FreeFMUState(instance, &restoredState);
        assert(rc == fmi3OK);
        assert(restoredState == nullptr);
    }
    {
        fmi3Float64 val = 0.0;
        const auto rc = fmi3GetFloat64(instance, &validVr, 1, &val, 1);
        assert(rc == fmi3OK);
        assert(val == value1);
    }
    {
        const fmi3ValueReference invalidVr = 1;
        fmi3Float64 val = -1.0;
        const auto rc = fmi3GetFloat64(instance, &invalidVr, 1, &val, 1);
        assert(rc == fmi3Error);
        std::fprintf(stderr, "(The last error was expected.)\n");
        assert(val == -1.0);
    }

    // Test event/continuous state count queries
    {
        size_t nEvents = 99;
        const auto rc = fmi3GetNumberOfEventIndicators(instance, &nEvents);
        assert(rc == fmi3OK);
        assert(nEvents == 0);
    }
    {
        size_t nStates = 99;
        const auto rc = fmi3GetNumberOfContinuousStates(instance, &nStates);
        assert(rc == fmi3OK);
        assert(nStates == 0);
    }

    // Termination (FMI 3.0)
    const auto terminateResult = fmi3Terminate(instance);
    assert(terminateResult == fmi3OK);

    fmi3FreeInstance(instance);

    return 0;
}
