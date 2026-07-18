#pragma once

#include <cstdint>

namespace TLV
{
    enum class LocomotionBand
    {
        idle,
        walk,
        sprint
    };

    struct AnalysisSettings
    {
        float deadzone{ 0.08F };
        float sprintThreshold{ 0.75F };
    };

    [[nodiscard]] float NormalizeStick(std::int16_t value);
    [[nodiscard]] LocomotionBand ClassifyForwardSpeed(
        float normalizedY,
        const AnalysisSettings& settings);
    [[nodiscard]] const char* LocomotionBandName(LocomotionBand band);
}
