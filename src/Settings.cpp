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
            (std::max)(0L, ini.GetLongValue(section, key, fallback)));
    }

    [[nodiscard]] double ReadDouble(
        CSimpleIniA& ini,
        const char* section,
        const char* key,
        double fallback)
    {
        return ini.GetDoubleValue(section, key, fallback);
    }

    void WriteBool(CSimpleIniA& ini, const char* section, const char* key, bool value)
    {
        ini.SetBoolValue(section, key, value);
    }

    void WriteFloat(CSimpleIniA& ini, const char* section, const char* key, float value)
    {
        ini.SetDoubleValue(section, key, value);
    }

    void WriteDouble(CSimpleIniA& ini, const char* section, const char* key, double value)
    {
        ini.SetDoubleValue(section, key, value);
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
        userIndex_ = (std::min)(
            3U,
            ReadUInt(ini, "Probe", "UserIndex", userIndex_));
        directApiEnabled_ = ReadBool(
            ini,
            "RealityRunner",
            "DirectApiEnabled",
            directApiEnabled_);
        comPort_ = ini.GetValue("RealityRunner", "ComPort", comPort_.c_str());
        apiPollMs_ = std::clamp<std::uint32_t>(
            ReadUInt(ini, "RealityRunner", "ApiPollMs", apiPollMs_),
            10,
            1000);
        enableOutput_ = ReadBool(ini, "Output", "EnableOutput", enableOutput_);
        forwardMagnitude_ = std::clamp(
            ReadFloat(ini, "Output", "ForwardMagnitude", forwardMagnitude_),
            0.0F,
            1.0F);
        coastMaxSeconds_ = std::clamp(
            ReadDouble(ini, "Intent", "CoastMaxSeconds", coastMaxSeconds_),
            0.0,
            2.0);
        staleTimeoutMs_ = std::clamp<std::uint32_t>(
            ReadUInt(ini, "Intent", "StaleTimeoutMs", staleTimeoutMs_),
            100,
            5000);
        runEnterSeconds_ = std::clamp(
            ReadDouble(
                ini,
                "Intent",
                "RunEnterSeconds",
                ReadDouble(ini, "Intent", "SprintEnterSeconds", runEnterSeconds_)),
            0.0,
            2.0);
        runExitSeconds_ = std::clamp(
            ReadDouble(
                ini,
                "Intent",
                "RunExitSeconds",
                ReadDouble(ini, "Intent", "SprintExitSeconds", runExitSeconds_)),
            0.0,
            2.0);
        runCancelSeconds_ = std::clamp(
            ReadDouble(
                ini,
                "Intent",
                "RunCancelSeconds",
                ReadDouble(ini, "Intent", "SprintCancelSeconds", runCancelSeconds_)),
            0.0,
            0.50);
        if (coastMaxSeconds_ * 1000.0 >= static_cast<double>(staleTimeoutMs_)) {
            coastMaxSeconds_ =
                (std::max)(0.0, (static_cast<double>(staleTimeoutMs_) - 50.0) / 1000.0);
            logger::warn(
                "CoastMaxSeconds must be below StaleTimeoutMs; clamped to {:.3f}",
                coastMaxSeconds_);
        }

        analysis_.deadzone = std::clamp(
            ReadFloat(ini, "Analysis", "Deadzone", analysis_.deadzone),
            0.0F,
            0.50F);
        analysis_.runThreshold = std::clamp(
            ReadFloat(
                ini,
                "Analysis",
                "RunThreshold",
                ReadFloat(ini, "Analysis", "SprintThreshold", analysis_.runThreshold)),
            analysis_.deadzone,
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
        logger::info(
            "directApi={} comPort={} apiPollMs={} enableOutput={} forwardMagnitude={:.3f} "
            "coast={:.3f}s stale={}ms runEnter={:.3f}s runExit={:.3f}s runCancel={:.3f}s",
            directApiEnabled_,
            comPort_,
            apiPollMs_,
            enableOutput_,
            forwardMagnitude_,
            coastMaxSeconds_,
            staleTimeoutMs_,
            runEnterSeconds_,
            runExitSeconds_,
            runCancelSeconds_);
    }

    bool Settings::Save() const
    {
        CSimpleIniA ini;
        ini.SetUnicode();

        WriteBool(ini, "General", "Enabled", enabled_);
        WriteBool(ini, "General", "Telemetry", telemetry_);
        WriteBool(ini, "General", "DebugLogging", debugLogging_);

        WriteBool(ini, "RealityRunner", "DirectApiEnabled", directApiEnabled_);
        ini.SetValue("RealityRunner", "ComPort", comPort_.c_str());
        ini.SetLongValue("RealityRunner", "ApiPollMs", apiPollMs_);

        WriteBool(ini, "Output", "EnableOutput", enableOutput_);
        WriteFloat(ini, "Output", "ForwardMagnitude", forwardMagnitude_);

        WriteDouble(ini, "Intent", "CoastMaxSeconds", coastMaxSeconds_);
        ini.SetLongValue("Intent", "StaleTimeoutMs", staleTimeoutMs_);
        WriteDouble(ini, "Intent", "RunEnterSeconds", runEnterSeconds_);
        WriteDouble(ini, "Intent", "RunExitSeconds", runExitSeconds_);
        WriteDouble(ini, "Intent", "RunCancelSeconds", runCancelSeconds_);

        WriteFloat(ini, "Analysis", "Deadzone", analysis_.deadzone);
        WriteFloat(ini, "Analysis", "RunThreshold", analysis_.runThreshold);

        if (ini.SaveFile(settingsPath) < 0) {
            logger::error("Could not save {}", "TreadmillLocomotionVR.ini");
            return false;
        }

        logger::info("Saved {}", "TreadmillLocomotionVR.ini");
        return true;
    }

    bool Settings::GetBool(std::string_view name) const
    {
        if (name == "Enabled") {
            return enabled_;
        }
        if (name == "Telemetry") {
            return telemetry_;
        }
        if (name == "DebugLogging") {
            return debugLogging_;
        }
        if (name == "DirectApiEnabled") {
            return directApiEnabled_;
        }
        if (name == "EnableOutput") {
            return enableOutput_;
        }
        return false;
    }

    float Settings::GetFloat(std::string_view name) const
    {
        if (name == "Deadzone") {
            return analysis_.deadzone;
        }
        if (name == "RunThreshold" || name == "SprintThreshold") {
            return analysis_.runThreshold;
        }
        if (name == "ForwardMagnitude") {
            return forwardMagnitude_;
        }
        if (name == "CoastMaxSeconds") {
            return static_cast<float>(coastMaxSeconds_);
        }
        if (name == "StaleTimeoutMs") {
            return static_cast<float>(staleTimeoutMs_);
        }
        if (name == "RunEnterSeconds" || name == "SprintEnterSeconds") {
            return static_cast<float>(runEnterSeconds_);
        }
        if (name == "RunExitSeconds" || name == "SprintExitSeconds") {
            return static_cast<float>(runExitSeconds_);
        }
        if (name == "RunCancelSeconds" || name == "SprintCancelSeconds") {
            return static_cast<float>(runCancelSeconds_);
        }
        return 0.0F;
    }

    bool Settings::SetBool(std::string_view name, bool value)
    {
        if (name == "Enabled") {
            enabled_ = value;
            return true;
        }
        if (name == "Telemetry") {
            telemetry_ = value;
            return true;
        }
        if (name == "DebugLogging") {
            debugLogging_ = value;
            return true;
        }
        if (name == "DirectApiEnabled") {
            directApiEnabled_ = value;
            return true;
        }
        if (name == "EnableOutput") {
            enableOutput_ = value;
            return true;
        }
        return false;
    }

    bool Settings::SetFloat(std::string_view name, float value)
    {
        if (name == "Deadzone") {
            analysis_.deadzone = std::clamp(value, 0.0F, 0.50F);
            analysis_.runThreshold = (std::max)(analysis_.runThreshold, analysis_.deadzone);
            return true;
        }
        if (name == "RunThreshold" || name == "SprintThreshold") {
            analysis_.runThreshold = std::clamp(value, analysis_.deadzone, 1.0F);
            return true;
        }
        if (name == "ForwardMagnitude") {
            forwardMagnitude_ = std::clamp(value, 0.0F, 1.0F);
            return true;
        }
        if (name == "CoastMaxSeconds") {
            coastMaxSeconds_ = std::clamp<double>(value, 0.0, 2.0);
            return true;
        }
        if (name == "StaleTimeoutMs") {
            staleTimeoutMs_ = std::clamp<std::uint32_t>(
                static_cast<std::uint32_t>((std::max)(0.0F, value)),
                100,
                5000);
            return true;
        }
        if (name == "RunEnterSeconds" || name == "SprintEnterSeconds") {
            runEnterSeconds_ = std::clamp<double>(value, 0.0, 2.0);
            return true;
        }
        if (name == "RunExitSeconds" || name == "SprintExitSeconds") {
            runExitSeconds_ = std::clamp<double>(value, 0.0, 2.0);
            return true;
        }
        if (name == "RunCancelSeconds" || name == "SprintCancelSeconds") {
            runCancelSeconds_ = std::clamp<double>(value, 0.0, 0.50);
            return true;
        }
        return false;
    }

    bool Settings::Enabled() const { return enabled_; }
    bool Settings::Telemetry() const { return telemetry_; }
    bool Settings::DebugLogging() const { return debugLogging_; }
    bool Settings::PatchXInput() const { return patchXInput_; }
    bool Settings::LogOnly() const { return logOnly_; }
    bool Settings::LogAllUsers() const { return logAllUsers_; }
    std::uint32_t Settings::UserIndex() const { return userIndex_; }
    const AnalysisSettings& Settings::Analysis() const { return analysis_; }
    bool Settings::DirectApiEnabled() const { return directApiEnabled_; }
    const std::string& Settings::ComPort() const { return comPort_; }
    std::uint32_t Settings::ApiPollMs() const { return apiPollMs_; }
    bool Settings::EnableOutput() const { return enableOutput_; }
    float Settings::ForwardMagnitude() const { return forwardMagnitude_; }
    double Settings::CoastMaxSeconds() const { return coastMaxSeconds_; }
    std::uint32_t Settings::StaleTimeoutMs() const { return staleTimeoutMs_; }
    double Settings::RunEnterSeconds() const { return runEnterSeconds_; }
    double Settings::RunExitSeconds() const { return runExitSeconds_; }
    double Settings::RunCancelSeconds() const { return runCancelSeconds_; }
}
