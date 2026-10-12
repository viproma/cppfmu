#include <cppfmu_cs.hpp>

#include <cstring>
#include <stdexcept>
#include <vector>


class TestSlave : public cppfmu::SlaveInstance
{
public:
    explicit TestSlave(cppfmu::Memory memory)
        : memory_(memory)
    { }

    void SetReal(
        const cppfmu::FMIValueReference vr[],
        std::size_t nvr,
        const cppfmu::FMIReal value[]) override
    {
        for (std::size_t i = 0; i < nvr; ++i) {
            if (vr[i] == 0) {
                value_ = value[i];
            } else if (vr[i] == 2) {
                seed_ = value[i];
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
            } else if (vr[i] == 2) {
                value[i] = seed_;
            } else {
                throw std::logic_error("Invalid value reference");
            }
        }
    }

    void SetBoolean(
        const cppfmu::FMIValueReference vr[],
        std::size_t nvr,
        const cppfmu::FMIBoolean value[]) override
    {
        for (std::size_t i = 0; i < nvr; ++i) {
            if (vr[i] == 1) {
                derivativeSupported_ = (value[i] == cppfmu::FMITrue);
            } else {
                throw std::logic_error("Invalid value reference");
            }
        }
    }

    void GetBoolean(
        const cppfmu::FMIValueReference vr[],
        std::size_t nvr,
        cppfmu::FMIBoolean value[]) const override
    {
        for (std::size_t i = 0; i < nvr; ++i) {
            if (vr[i] == 1) {
                value[i] =
                    derivativeSupported_ ? cppfmu::FMITrue : cppfmu::FMIFalse;
            } else {
                throw std::logic_error("Invalid value reference");
            }
        }
    }

    void GetDirectionalDerivative(
        const cppfmu::FMIValueReference vUnknown_ref[],
        std::size_t nUnknown,
        const cppfmu::FMIValueReference vKnown_ref[],
        std::size_t nKnown,
        const cppfmu::FMIReal dvKnown[],
        cppfmu::FMIReal dvUnknown[]) const override
    {
        if (!derivativeSupported_) {
            SlaveInstance::GetDirectionalDerivative(
                vUnknown_ref,
                nUnknown,
                vKnown_ref,
                nKnown,
                dvKnown,
                dvUnknown);
        }
        for (std::size_t i = 0; i < nUnknown; ++i) {
            dvUnknown[i] = 0.0;
            for (std::size_t j = 0; j < nKnown; ++j) {
                dvUnknown[i] += seed_ * dvKnown[j];
            }
        }
    }

    void SetRealInputDerivatives(
        const cppfmu::FMIValueReference vr[],
        std::size_t nvr,
        const cppfmu::FMIInteger order[],
        const cppfmu::FMIReal value[]) override
    {
        if (!derivativeSupported_) {
            SlaveInstance::SetRealInputDerivatives(vr, nvr, order, value);
        }
        inputDerivatives_.assign(value, value + nvr);
        inputOrders_.assign(order, order + nvr);
    }

    void GetRealOutputDerivatives(
        const cppfmu::FMIValueReference vr[],
        std::size_t nvr,
        const cppfmu::FMIInteger order[],
        cppfmu::FMIReal value[]) const override
    {
        if (!derivativeSupported_) {
            SlaveInstance::GetRealOutputDerivatives(vr, nvr, order, value);
        }
        for (std::size_t i = 0; i < nvr; ++i) {
            value[i] = seed_ * static_cast<cppfmu::FMIReal>(order[i]);
        }
    }

    void GetFMUState(cppfmu::FMIFMUState* state) override
    {
        auto s = (*state == nullptr) ? cppfmu::New<cppfmu::FMIReal>(memory_)
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
    bool derivativeSupported_ = false;
    cppfmu::FMIReal seed_ = 1.0;
    std::vector<cppfmu::FMIReal> inputDerivatives_;
    std::vector<cppfmu::FMIInteger> inputOrders_;
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
