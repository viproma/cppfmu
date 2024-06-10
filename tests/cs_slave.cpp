#include <cppfmu_cs.hpp>

#include <cstring>
#include <stdexcept>


class TestSlave : public cppfmu::SlaveInstance
{
public:
    explicit TestSlave(cppfmu::Memory memory)
        : memory_(memory)
    {
    }

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

    void GetFMUState(cppfmu::FMIFMUState* state) override
    {
        auto s = (*state == nullptr)
            ? cppfmu::New<cppfmu::FMIReal>(memory_)
            : static_cast<cppfmu::FMIReal*>(*state);
        *s = value_;
        *state = s;
    }

    void SetFMUState(cppfmu::FMIFMUState state) override
    {
        auto s = static_cast<cppfmu::FMIReal*>(state);
        value_ = *s;
    }

    void FreeFMUState(cppfmu::FMIFMUState state) override
    {
        auto s = static_cast<cppfmu::FMIReal*>(state);
        cppfmu::Delete(memory_, s);
    }

    std::size_t SerializedFMUStateSize(cppfmu::FMIFMUState) override
    {
        return sizeof(cppfmu::FMIReal);
    }

    void SerializeFMUState(
        cppfmu::FMIFMUState state,
        cppfmu::FMIByte data[],
        std::size_t size) override
    {
        auto s = static_cast<cppfmu::FMIReal*>(state);
        std::memcpy(data, s, sizeof *s);
    }

    cppfmu::FMIFMUState DeserializeFMUState(
        const cppfmu::FMIByte data[],
        std::size_t size) override
    {
        auto s = cppfmu::New<cppfmu::FMIReal>(memory_);
        std::memcpy(s, data, sizeof *s);
        return s;
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
    cppfmu::Memory memory_;
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
    return cppfmu::AllocateUnique<TestSlave>(memory, memory);
}

