#include "LocomotionIntent.h"

#include "Settings.h"

namespace
{
    constexpr std::uint16_t xinputGamepadLeftShoulder = 0x0100;

    [[nodiscard]] std::int16_t ToStick(float normalized)
    {
        const auto scaled = std::lround(std::clamp(normalized, -1.0F, 1.0F) * 32767.0F);
        return static_cast<std::int16_t>(std::clamp<long>(scaled, -32768L, 32767L));
    }

    [[nodiscard]] bool IsMoving(
        const TLV::RealityRunnerSnapshot& snapshot,
        const TLV::RealityRunnerCurve& curve)
    {
        const auto magnitude = std::abs(snapshot.joystickValue);
        const auto deadzone = (std::max)(curve.deadzone, 1);
        return snapshot.connected && magnitude >= deadzone;
    }

    [[nodiscard]] bool IsSprinting(
        const TLV::RealityRunnerSnapshot& snapshot,
        const TLV::RealityRunnerCurve& curve)
    {
        if (!snapshot.connected) {
            return false;
        }
        const auto magnitude = std::abs(snapshot.joystickValue);
        const auto threshold = (std::max)(curve.sprintThreshold, 1);
        return snapshot.sprintActive || magnitude >= threshold;
    }
}

namespace TLV
{
    const char* IntentStateName(IntentState state)
    {
        switch (state) {
        case IntentState::stopped:
            return "stopped";
        case IntentState::walking:
            return "walking";
        case IntentState::sprinting:
            return "sprinting";
        default:
            return "unknown";
        }
    }

    LocomotionIntent& LocomotionIntent::GetSingleton()
    {
        static LocomotionIntent singleton;
        return singleton;
    }

    void LocomotionIntent::Reset(const char*)
    {
        state_ = IntentState::stopped;
        noMovementSeconds_ = 0.0;
        sprintPresentSeconds_ = 0.0;
        sprintAbsentSeconds_ = 0.0;
        lastSequence_ = 0;
    }

    IntentOutput LocomotionIntent::Update(
        const RealityRunnerSnapshot& snapshot,
        const RealityRunnerCurve& curve,
        float deltaSeconds)
    {
        const auto& settings = Settings::GetSingleton();
        const auto now = GetTickCount64();
        const auto hasFreshFrame = snapshot.connected &&
            snapshot.tickMs != 0 &&
            now >= snapshot.tickMs &&
            now - snapshot.tickMs <= settings.StaleTimeoutMs();
        const auto frameAge = snapshot.tickMs != 0 && now >= snapshot.tickMs ?
            static_cast<std::uint32_t>(
                (std::min<std::uint64_t>)(now - snapshot.tickMs, 999999)) :
            999999U;
        const auto dt = std::clamp(
            static_cast<double>(deltaSeconds),
            0.0,
            0.1);

        const char* reason = "hold";
        if (!hasFreshFrame) {
            if (state_ != IntentState::stopped) {
                reason = "stale";
            }
            state_ = IntentState::stopped;
            noMovementSeconds_ = 0.0;
            sprintPresentSeconds_ = 0.0;
            sprintAbsentSeconds_ = 0.0;
        } else {
            const auto moving = IsMoving(snapshot, curve);
            const auto sprinting = IsSprinting(snapshot, curve);
            if (moving) {
                noMovementSeconds_ = 0.0;
            } else {
                noMovementSeconds_ += dt;
            }

            if (sprinting) {
                sprintPresentSeconds_ += dt;
                sprintAbsentSeconds_ = 0.0;
            } else {
                sprintAbsentSeconds_ += dt;
                sprintPresentSeconds_ = 0.0;
            }

            switch (state_) {
            case IntentState::stopped:
                if (moving) {
                    state_ = IntentState::walking;
                    reason = "movement-start";
                }
                break;
            case IntentState::walking:
                if (!moving && noMovementSeconds_ >= settings.CoastMaxSeconds()) {
                    state_ = IntentState::stopped;
                    reason = "coast-expired";
                } else if (
                    sprintPresentSeconds_ >= settings.SprintEnterSeconds()) {
                    state_ = IntentState::sprinting;
                    reason = "sprint-enter";
                }
                break;
            case IntentState::sprinting:
                if (!moving && noMovementSeconds_ >= settings.CoastMaxSeconds()) {
                    state_ = IntentState::stopped;
                    reason = "coast-expired";
                } else if (
                    sprintAbsentSeconds_ >= settings.SprintExitSeconds()) {
                    state_ = IntentState::walking;
                    reason = "sprint-exit";
                }
                break;
            }
        }

        const auto forward = ToStick(settings.ForwardMagnitude());
        IntentOutput output{};
        output.state = state_;
        output.reason = reason;
        output.tickMs = now;
        output.sampleSequence = snapshot.sequence;
        output.frameAgeMs = frameAge;
        output.joystickValue = snapshot.joystickValue;
        output.deviceSprintActive = snapshot.sprintActive;
        output.stale = !hasFreshFrame;
        output.intendedLeftY =
            state_ == IntentState::stopped ? 0 : forward;
        output.intendedButtons =
            state_ == IntentState::sprinting ? xinputGamepadLeftShoulder : 0;
        lastSequence_ = snapshot.sequence;
        return output;
    }
}
