#include "GameIntegration.h"

#include "IntentTelemetry.h"
#include "LocomotionIntent.h"
#include "RealityRunnerApiClient.h"
#include "Settings.h"
#include "XInputLocomotionOutput.h"

namespace TLV
{
    GameIntegration& GameIntegration::GetSingleton()
    {
        static GameIntegration singleton;
        return singleton;
    }

    GameIntegration::~GameIntegration()
    {
        Shutdown();
    }

    bool GameIntegration::Initialize()
    {
        if (initialized_) {
            return true;
        }
        if (!REL::Module::IsVR()) {
            logger::critical("TreadmillLocomotionVR requires Skyrim VR");
            return false;
        }

        constexpr std::size_t playerUpdateIndex = 0xAF;
        REL::Relocation<std::uintptr_t> playerVtable{ RE::PlayerCharacter::VTABLE[0] };
        originalPlayerUpdate_ = playerVtable.write_vfunc(
            playerUpdateIndex,
            PlayerUpdateHook);
        if (originalPlayerUpdate_.address() == 0) {
            logger::critical("Unable to install treadmill player update hook");
            return false;
        }

        if (Settings::GetSingleton().EnableOutput()) {
            const auto outputReady =
                XInputLocomotionOutput::GetSingleton().Initialize();
            if (!outputReady) {
                logger::warn(
                    "Treadmill XInput output requested but no output path is ready");
            }
        }

        initialized_ = true;
        logger::info("Treadmill player update hook installed");
        return true;
    }

    void GameIntegration::Shutdown()
    {
        XInputLocomotionOutput::GetSingleton().SetLocomotion(0, 0);
        XInputLocomotionOutput::GetSingleton().Shutdown();

        if (initialized_) {
            // Vtable hook restoration is intentionally left to process shutdown,
            // matching the sibling SKSE plugins' pattern.
            initialized_ = false;
        }
    }

    void GameIntegration::PlayerUpdateHook(RE::Actor* actor, float deltaSeconds)
    {
        auto& integration = GetSingleton();
        integration.OnFrame(deltaSeconds);
        integration.originalPlayerUpdate_(actor, deltaSeconds);
    }

    void GameIntegration::OnFrame(float deltaSeconds)
    {
        const auto& settings = Settings::GetSingleton();
        if (!settings.Enabled() || !settings.DirectApiEnabled()) {
            XInputLocomotionOutput::GetSingleton().SetLocomotion(0, 0);
            return;
        }

        const auto snapshot = RealityRunnerApiClient::GetSingleton().Latest();
        const auto curve = RealityRunnerApiClient::GetSingleton().Curve();
        const auto output = LocomotionIntent::GetSingleton().Update(
            snapshot,
            curve,
            deltaSeconds);

        if (settings.Telemetry()) {
            IntentTelemetry::GetSingleton().Write(output);
        }

        if (settings.DebugLogging() &&
            output.sampleSequence != 0 &&
            (output.sampleSequence % 60) == 0) {
            logger::debug(
                "intent seq={} age={} stale={} value={} deviceSprint={} state={} "
                "leftY={} buttons=0x{:04x}",
                output.sampleSequence,
                output.frameAgeMs,
                output.stale,
                output.joystickValue,
                output.deviceSprintActive,
                IntentStateName(output.state),
                output.intendedLeftY,
                output.intendedButtons);
        }

        if (settings.EnableOutput()) {
            auto& outputPath = XInputLocomotionOutput::GetSingleton();
            if (outputPath.IsOutputPathReady()) {
                outputPath.SetLocomotion(
                    output.intendedLeftY,
                    output.intendedButtons);
            } else {
                outputPath.SetLocomotion(0, 0);
                static std::atomic<bool> warned{ false };
                if (!warned.exchange(true, std::memory_order_relaxed)) {
                    logger::warn(
                        "EnableOutput=true requested, but no XInput output path is ready");
                }
            }
        } else {
            XInputLocomotionOutput::GetSingleton().SetLocomotion(0, 0);
        }
    }
}
