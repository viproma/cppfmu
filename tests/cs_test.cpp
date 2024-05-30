#include <fmi2Functions.h>

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

extern "C" void* alloc(std::size_t nobj, std::size_t size) noexcept
{
    return std::malloc(nobj*size);
}


int main()
{
    // Instantiation and setup
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
        const auto rc = fmi2SetupExperiment(
            instance,
            fmi2False,
            0.0,
            0.0,
            fmi2False,
            0.0);
        assert(rc == fmi2OK);
    }

    // Initialization
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

    // Save state
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
            instance, state, serializedState.data(), serializedState.size());
        assert(rc == fmi2OK);
    }
    {
        const auto rc = fmi2FreeFMUstate(instance, &state);
        assert(rc == fmi2OK);
        assert(state == nullptr);
    }

    // Simulation
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
            instance, serializedState.data(), serializedState.size(), &restoredState);
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

    // Termination
    const auto terminateResult = fmi2Terminate(instance);
    assert(terminateResult == fmi2OK);

    fmi2FreeInstance(instance);
    return 0;
}
