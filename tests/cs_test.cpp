#include <fmi2Functions.h>

#include <cassert>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>


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

    const auto setupResult = fmi2SetupExperiment(
        instance,
        fmi2False,
        0.0,
        0.0,
        fmi2False,
        0.0);
    assert(setupResult == fmi2OK);

    // Initialization
    const auto initResult = fmi2EnterInitializationMode(instance);
    assert(initResult == fmi2OK);

    const fmi2ValueReference validVr = 0;
    const auto setResult = fmi2SetReal(instance, &validVr, 1, &TEST_VALUE);
    assert(setResult == fmi2OK);

    fmi2Real value = 0.0;
    const auto validGetResult = fmi2GetReal(instance, &validVr, 1, &value);
    assert(validGetResult == fmi2OK);
    assert(value == TEST_VALUE);

    const auto endInitResult = fmi2ExitInitializationMode(instance);
    assert(endInitResult == fmi2OK);

    // Simulation
    const auto stepResult = fmi2DoStep(instance, 0.0,0.1, fmi2False);
    assert(stepResult == fmi2OK);

    const fmi2ValueReference invalidVr = 1;
    value = -1.0;
    const auto invalidGetResult = fmi2GetReal(instance, &invalidVr, 1, &value);
    assert(invalidGetResult == fmi2Error);
    assert(value == -1.0);

    // Termination
    const auto terminateResult = fmi2Terminate(instance);
    assert(terminateResult == fmi2OK);

    fmi2FreeInstance(instance);
    return 0;
}
