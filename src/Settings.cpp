#include "Settings.h"

#include "Version.h"

namespace
{
    constexpr auto settingsPath = L"Data/SKSE/Plugins/TreadmillLocomotionVR.ini";

    [[nodiscard]] bool ReadBool(
        CSimpleIniA& ini,
        const char* section,
        const char* key,
        bool fallback)
    {
        return ini.GetBoolValue(section, key, fallback);
    }

    [[nodiscard]] float ReadFloat(
        CSimpleIniA& ini,
        const char* section,
        const char* key,
        float fallback)
    {
        return static_cast<float>(ini.GetDoubleValue(section, key, fallback));
    }

    [[nodiscard]] std::uint32_t ReadUInt(
        CSimpleIniA& ini,
        const char* section,
        const char* key,
        std::uint32_t fallback)
    {
        return static_cast<std::uint32_t>(
            (std::max)<long>(0, ini.GetLongValue(section, key, fallback)));
    }
}

namespace TLV
{
    Settings& Settings::GetSingleton()
    {
        static Settings settings;
        return settings;
    }

    void Settings::Load()
    {
        CSimpleIniA ini;
        ini.SetUnicode();
        if (ini.LoadFile(settingsPath) < 0) {
            logger::warn("Could not read {}; using defaults", "TreadmillLocomotionVR.ini");
            return;
        }

        enabled_ = ReadBool(ini, "General", "Enabled", enabled_);
        telemetry_ = ReadBool(ini, "General", "Telemetry", telemetry_);
        debugLogging_ = ReadBool(ini, "General", "DebugLogging", debugLogging_);
        patchXInput_ = ReadBool(ini, "Probe", "PatchXInput", patchXInput_);
        logOnly_ = ReadBool(ini, "Probe", "LogOnly", logOnly_);
        logAllUsers_ = ReadBool(ini, "Probe", "LogAllUsers", logAllUsers_);
        userIndex_ = (std::min)<std::uint32_t>(
            3,
            ReadUInt(ini, "Probe", "UserIndex", userIndex_));

        analysis_.deadzone = std::clamp(
            ReadFloat(ini, "Analysis", "Deadzone", analysis_.deadzone),
            0.0F,
            0.50F);
        analysis_.walkThreshold = std::clamp(
            ReadFloat(ini, "Analysis", "WalkThreshold", analysis_.walkThreshold),
            analysis_.deadzone,
            1.0F);
        analysis_.runThreshold = std::clamp(
            ReadFloat(ini, "Analysis", "RunThreshold", analysis_.runThreshold),
            analysis_.walkThreshold,
            1.0F);
        analysis_.sprintThreshold = std::clamp(
            ReadFloat(ini, "Analysis", "SprintThreshold", analysis_.sprintThreshold),
            analysis_.runThreshold,
            1.0F);

        logger::info(
            "{} settings loaded: enabled={} telemetry={} patchXInput={} logOnly={} logAllUsers={} userIndex={}",
            Version::name,
            enabled_,
            telemetry_,
            patchXInput_,
            logOnly_,
            logAllUsers_,
            userIndex_);
    }

    bool Settings::Enabled() const { return enabled_; }
    bool Settings::Telemetry() const { return telemetry_; }
    bool Settings::DebugLogging() const { return debugLogging_; }
    bool Settings::PatchXInput() const { return patchXInput_; }
    bool Settings::LogOnly() const { return logOnly_; }
    bool Settings::LogAllUsers() const { return logAllUsers_; }
    std::uint32_t Settings::UserIndex() const { return userIndex_; }
    const AnalysisSettings& Settings::Analysis() const { return analysis_; }
}
