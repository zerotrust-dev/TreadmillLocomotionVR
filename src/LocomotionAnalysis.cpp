#include "LocomotionAnalysis.h"

#include <algorithm>
#include <limits>

namespace TLV
{
    float NormalizeStick(std::int16_t value)
    {
        if (value == std::numeric_limits<std::int16_t>::min()) {
            return -1.0F;
        }
        return static_cast<float>(value) / 32767.0F;
    }

    LocomotionBand ClassifyForwardSpeed(
        float normalizedY,
        const AnalysisSettings& settings)
    {
        const auto forward = std::max(0.0F, normalizedY);
        if (forward < settings.deadzone) {
            return LocomotionBand::idle;
        }
        if (forward >= settings.sprintThreshold) {
            return LocomotionBand::sprint;
        }
        if (forward >= settings.runThreshold) {
            return LocomotionBand::run;
        }
        if (forward >= settings.walkThreshold) {
            return LocomotionBand::walk;
        }
        return LocomotionBand::idle;
    }

    const char* LocomotionBandName(LocomotionBand band)
    {
        switch (band) {
        case LocomotionBand::idle:
            return "idle";
        case LocomotionBand::walk:
            return "walk";
        case LocomotionBand::run:
            return "run";
        case LocomotionBand::sprint:
            return "sprint";
        default:
            return "unknown";
        }
    }
}
