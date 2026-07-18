#pragma once

#include "LocomotionAnalysis.h"

namespace TLV
{
    class Settings
    {
    public:
        static Settings& GetSingleton();

        void Load();

        [[nodiscard]] bool Enabled() const;
        [[nodiscard]] bool Telemetry() const;
        [[nodiscard]] bool DebugLogging() const;
        [[nodiscard]] bool PatchXInput() const;
        [[nodiscard]] bool LogOnly() const;
        [[nodiscard]] bool LogAllUsers() const;
        [[nodiscard]] std::uint32_t UserIndex() const;
        [[nodiscard]] const AnalysisSettings& Analysis() const;
        [[nodiscard]] bool DirectApiEnabled() const;
        [[nodiscard]] const std::string& ComPort() const;
        [[nodiscard]] bool EnableOutput() const;
        [[nodiscard]] float ForwardMagnitude() const;
        [[nodiscard]] double CoastMaxSeconds() const;
        [[nodiscard]] std::uint32_t StaleTimeoutMs() const;
        [[nodiscard]] double SprintEnterSeconds() const;
        [[nodiscard]] double SprintExitSeconds() const;

    private:
        bool enabled_{ false };
        bool telemetry_{ true };
        bool debugLogging_{ false };
        bool patchXInput_{ true };
        bool logOnly_{ true };
        bool logAllUsers_{ true };
        std::uint32_t userIndex_{ 0 };
        AnalysisSettings analysis_{};
        bool directApiEnabled_{ false };
        std::string comPort_{ "COM4" };
        bool enableOutput_{ false };
        float forwardMagnitude_{ 1.0F };
        double coastMaxSeconds_{ 0.25 };
        std::uint32_t staleTimeoutMs_{ 450 };
        double sprintEnterSeconds_{ 0.22 };
        double sprintExitSeconds_{ 0.35 };
    };
}
