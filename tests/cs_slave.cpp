#include <cppfmu_cs.hpp>

#include <stdexcept>


class TestSlave : public cppfmu::SlaveInstance
{
public:
    void SetReal(
        const cppfmu::FMIValueReference vr[],
        std::size_t nvr,
        const cppfmu::FMIReal value[]) override
    {
        for (std::size_t i = 0; i < nvr; ++i) {
            if (vr[i] == 0) {
                value_ = value[i];
            } else {
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
            if (vr[i] == 0) {
                value[i] = value_;
            } else {
                throw std::logic_error("Invalid value reference");
            }
        }
    }

    bool DoStep(
        cppfmu::FMIReal currentCommunicationPoint,
        cppfmu::FMIReal communicationStepSize,
        cppfmu::FMIBoolean newStep,
        cppfmu::FMIReal& endOfStep) override
    {
        return true;
    }

private:
    cppfmu::FMIReal value_ = 0.0;
};


cppfmu::UniquePtr<cppfmu::SlaveInstance> CppfmuInstantiateSlave(
    cppfmu::FMIString instanceName,
    cppfmu::FMIString fmuGUID,
    cppfmu::FMIString fmuResourceLocation,
    cppfmu::FMIString mimeType,
    cppfmu::FMIReal timeout,
    cppfmu::FMIBoolean visible,
    cppfmu::FMIBoolean interactive,
    cppfmu::Memory memory,
    cppfmu::Logger logger)
{
    return cppfmu::AllocateUnique<TestSlave>(memory);
}

