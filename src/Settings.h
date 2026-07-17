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

    private:
        bool enabled_{ false };
        bool telemetry_{ true };
        bool debugLogging_{ false };
        bool patchXInput_{ true };
        bool logOnly_{ true };
        bool logAllUsers_{ true };
        std::uint32_t userIndex_{ 0 };
        AnalysisSettings analysis_{};
    };
}
