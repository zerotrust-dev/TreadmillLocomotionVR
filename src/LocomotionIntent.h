#pragma once

#include "RealityRunnerApiClient.h"

namespace TLV
{
    enum class IntentState
    {
        stopped,
        walking,
        sprinting
    };

    struct IntentOutput
    {
        IntentState state{ IntentState::stopped };
        const char* reason{ "init" };
        std::uint64_t tickMs{ 0 };
        std::uint64_t sampleSequence{ 0 };
        std::uint32_t frameAgeMs{ 0 };
        int joystickValue{ 0 };
        bool deviceSprintActive{ false };
        bool stale{ true };
        std::int16_t intendedLeftY{ 0 };
        std::uint16_t intendedButtons{ 0 };
    };

    [[nodiscard]] const char* IntentStateName(IntentState state);

    class LocomotionIntent
    {
    public:
        static LocomotionIntent& GetSingleton();

        [[nodiscard]] IntentOutput Update(
            const RealityRunnerSnapshot& snapshot,
            const RealityRunnerCurve& curve,
            float deltaSeconds);
        void Reset(const char* reason);

    private:
        LocomotionIntent() = default;

        IntentState state_{ IntentState::stopped };
        double noMovementSeconds_{ 0.0 };
        double sprintPresentSeconds_{ 0.0 };
        double sprintAbsentSeconds_{ 0.0 };
        std::uint64_t lastSequence_{ 0 };
    };
}
