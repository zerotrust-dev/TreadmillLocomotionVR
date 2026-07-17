#pragma once

#include "LocomotionAnalysis.h"

namespace TLV
{
    struct XInputTelemetrySample
    {
        std::uint64_t sequence{ 0 };
        std::uint64_t tickMs{ 0 };
        std::uint32_t userIndex{ 0 };
        std::uint32_t result{ 0 };
        std::uint32_t packetNumber{ 0 };
        std::uint16_t buttons{ 0 };
        std::uint8_t leftTrigger{ 0 };
        std::uint8_t rightTrigger{ 0 };
        std::int16_t thumbLX{ 0 };
        std::int16_t thumbLY{ 0 };
        std::int16_t thumbRX{ 0 };
        std::int16_t thumbRY{ 0 };
    };

    class Telemetry
    {
    public:
        static Telemetry& GetSingleton();

        void WriteSample(
            const XInputTelemetrySample& sample,
            const AnalysisSettings& analysis);

    private:
        bool EnsureOpen();

        std::ofstream stream_;
        bool openFailed_{ false };
        std::uint64_t firstTickMs_{ 0 };
    };
}
