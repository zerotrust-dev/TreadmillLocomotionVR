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

    bool GameIntegration::ApplySettings()
    {
        const auto& settings = Settings::GetSingleton();
        spdlog::set_level(
            settings.DebugLogging() ?
                spdlog::level::debug :
                spdlog::level::info);

        auto& output = XInputLocomotionOutput::GetSingleton();
        if (!settings.Enabled() ||
            !settings.DirectApiEnabled() ||
            !settings.EnableOutput()) {
            output.SetLocomotion(0, 0);
            runCancelSecondsRemaining_ = 0.0;
            lastIntentState_ = IntentState::stopped;
            return true;
        }

        if (!output.IsOutputPathReady()) {
            return output.Initialize();
        }
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
            runCancelSecondsRemaining_ = 0.0;
            lastIntentState_ = IntentState::stopped;
            return;
        }

        // Release everything while the game is paused. Without this, walking on
        // the treadmill with a menu open injects left-stick forward and LB into
        // the menu itself: the stick navigates lists and LB/RB switch inventory
        // tabs, so the belt would scroll and re-categorise the player's
        // inventory. Reset the intent so movement is re-derived on resume
        // rather than resuming mid-state.
        if (settings.PauseInMenus()) {
            const auto ui = RE::UI::GetSingleton();
            if (ui && ui->GameIsPaused()) {
                XInputLocomotionOutput::GetSingleton().SetLocomotion(0, 0);
                LocomotionIntent::GetSingleton().Reset("paused");
                runCancelSecondsRemaining_ = 0.0;
                lastIntentState_ = IntentState::stopped;
                return;
            }
        }

        const auto snapshot = RealityRunnerApiClient::GetSingleton().Latest();
        const auto curve = RealityRunnerApiClient::GetSingleton().Curve();
        auto output = LocomotionIntent::GetSingleton().Update(
            snapshot,
            curve,
            deltaSeconds);

        if (lastIntentState_ == IntentState::running &&
            output.state == IntentState::walking &&
            settings.RunCancelSeconds() > 0.0) {
            runCancelSecondsRemaining_ = settings.RunCancelSeconds();
        }
        lastIntentState_ = output.state;

        if (output.state != IntentState::walking) {
            runCancelSecondsRemaining_ = 0.0;
        } else if (runCancelSecondsRemaining_ > 0.0) {
            output.reason = "run-cancel";
            output.intendedLeftY = 0;
            output.intendedButtons = 0;
            runCancelSecondsRemaining_ = (std::max)(
                0.0,
                runCancelSecondsRemaining_ -
                    static_cast<double>((std::max)(0.0F, deltaSeconds)));
        }

        if (settings.Telemetry()) {
            IntentTelemetry::GetSingleton().Write(output);
        }

        if (settings.DebugLogging() &&
            output.sampleSequence != 0 &&
            (output.sampleSequence % 60) == 0) {
            logger::debug(
                "intent seq={} age={} stale={} value={} deviceRunSignal={} state={} "
                "leftY={} buttons=0x{:04x}",
                output.sampleSequence,
                output.frameAgeMs,
                output.stale,
                output.joystickValue,
                output.deviceRunSignal,
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
