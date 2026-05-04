#include <cppfmu_cs_fmi3.hpp>

#include <cstring>
#include <stdexcept>
#include <vector>


class TestSlave3 : public cppfmu::SlaveInstance3
{
public:
    void SetFloat64(
        const cppfmu::FMIValueReference vr[],
        std::size_t nvr,
        const cppfmu::FMIReal value[],
        std::size_t /*nValues*/) override
    {
        for (std::size_t i = 0; i < nvr; ++i) {
            if (vr[i] == 0) {
                value_ = value[i];
            } else if (vr[i] == 1) {
                derivativeSupported_ = (value[i] != 0.0);
            } else if (vr[i] == 2) {
                seed_ = value[i];
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
            if (vr[i] == 0) {
                value[i] = value_;
            } else if (vr[i] == 1) {
                value[i] = derivativeSupported_ ? 1.0 : 0.0;
            } else if (vr[i] == 2) {
                value[i] = seed_;
            } else {
                throw std::logic_error("Invalid value reference");
            }
        }
    }

    void GetDirectionalDerivative(
        const cppfmu::FMIValueReference /*vUnknown_ref*/[],
        std::size_t nUnknown,
        const cppfmu::FMIValueReference /*vKnown_ref*/[],
        std::size_t nKnown,
        const cppfmu::FMIReal dvKnown[],
        std::size_t /*nSeed*/,
        cppfmu::FMIReal dvUnknown[],
        std::size_t /*nSensitivity*/) const override
    {
        if (!derivativeSupported_) {
            SlaveInstance3::GetDirectionalDerivative(
                nullptr, 0, nullptr, 0, nullptr, 0, nullptr, 0);
        }
        for (std::size_t i = 0; i < nUnknown; ++i) {
            dvUnknown[i] = 0.0;
            for (std::size_t j = 0; j < nKnown; ++j) {
                dvUnknown[i] += seed_ * dvKnown[j];
            }
        }
    }

    void GetOutputDerivatives(
        const cppfmu::FMIValueReference vr[],
        std::size_t nvr,
        const cppfmu::FMIInteger order[],
        cppfmu::FMIReal value[],
        std::size_t /*nValues*/) const override
    {
        if (!derivativeSupported_) {
            SlaveInstance3::GetOutputDerivatives(vr, nvr, order, value, 0);
        }
        for (std::size_t i = 0; i < nvr; ++i) {
            value[i] = seed_ * static_cast<cppfmu::FMIReal>(order[i]);
        }
    }

    void GetFMUState(cppfmu::FMIFMUState* state) override
    {
        auto s = (*state == nullptr)
            ? new cppfmu::FMIReal
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
        delete s;
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
        auto s = new cppfmu::FMIReal;
        std::memcpy(s, data, sizeof *s);
        return s;
    }

    bool DoStep(
        cppfmu::FMIReal /*currentCommunicationPoint*/,
        cppfmu::FMIReal /*communicationStepSize*/,
        cppfmu::FMIBoolean /*noSetFMUStatePriorToCurrentPoint*/,
        cppfmu::FMIBoolean& eventHandlingNeeded,
        cppfmu::FMIBoolean& terminateSimulation,
        cppfmu::FMIBoolean& earlyReturn,
        cppfmu::FMIReal& lastSuccessfulTime) override
    {
        eventHandlingNeeded = cppfmu::FMIFalse;
        terminateSimulation = cppfmu::FMIFalse;
        earlyReturn = cppfmu::FMIFalse;
        return true;
    }

    void SetBinary(
        const cppfmu::FMIValueReference vr[],
        std::size_t nvr,
        const std::size_t sizes[],
        const cppfmu::FMIBinary value[],
        std::size_t /*nValues*/) override
    {
        for (std::size_t i = 0; i < nvr; ++i) {
            if (vr[i] == 3) {
                binaryData_.assign(value[i], value[i] + sizes[i]);
            } else {
                throw std::logic_error("Invalid value reference");
            }
        }
    }

    void GetBinary(
        const cppfmu::FMIValueReference vr[],
        std::size_t nvr,
        std::size_t sizes[],
        cppfmu::FMIBinary value[],
        std::size_t /*nValues*/) const override
    {
        for (std::size_t i = 0; i < nvr; ++i) {
            if (vr[i] == 3) {
                sizes[i] = binaryData_.size();
                if (value != nullptr && value[i] != nullptr) {
                    std::memcpy(const_cast<cppfmu::FMIByte*>(value[i]), binaryData_.data(), binaryData_.size());
                }
            } else {
                throw std::logic_error("Invalid value reference");
            }
        }
    }

private:
    cppfmu::FMIReal value_ = 0.0;
    bool derivativeSupported_ = false;
    cppfmu::FMIReal seed_ = 1.0;
    std::vector<cppfmu::FMIByte> binaryData_;
};


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
    std::function<void(cppfmu::FMIStatus, cppfmu::FMIString, cppfmu::FMIString)> /*logger*/)
{
    return cppfmu::AllocateUnique3<TestSlave3>();
}
