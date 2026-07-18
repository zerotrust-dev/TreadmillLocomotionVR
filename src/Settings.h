#pragma once

#include "LocomotionAnalysis.h"

#include <string_view>

namespace TLV
{
    class Settings
    {
    public:
        static Settings& GetSingleton();

        void Load();
        [[nodiscard]] bool Save() const;
        [[nodiscard]] bool GetBool(std::string_view name) const;
        [[nodiscard]] float GetFloat(std::string_view name) const;
        [[nodiscard]] bool SetBool(std::string_view name, bool value);
        [[nodiscard]] bool SetFloat(std::string_view name, float value);

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
        [[nodiscard]] std::uint32_t ApiPollMs() const;
        [[nodiscard]] bool EnableOutput() const;
        [[nodiscard]] float ForwardMagnitude() const;
        [[nodiscard]] double CoastMaxSeconds() const;
        [[nodiscard]] std::uint32_t StaleTimeoutMs() const;
        [[nodiscard]] double SprintEnterSeconds() const;
        [[nodiscard]] double SprintExitSeconds() const;
        [[nodiscard]] double SprintCancelSeconds() const;

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
        std::uint32_t apiPollMs_{ 50 };
        bool enableOutput_{ false };
        float forwardMagnitude_{ 1.0F };
        double coastMaxSeconds_{ 0.25 };
        std::uint32_t staleTimeoutMs_{ 450 };
        double sprintEnterSeconds_{ 0.22 };
        double sprintExitSeconds_{ 0.35 };
        double sprintCancelSeconds_{ 0.12 };
    };
}
