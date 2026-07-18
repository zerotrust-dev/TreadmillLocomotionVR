#include "PapyrusApi.h"

#include "GameIntegration.h"
#include "RealityRunnerApiClient.h"
#include "Settings.h"
#include "Version.h"

namespace
{
    constexpr auto scriptName = "TreadmillLocomotionVR";
    constexpr std::int32_t nativeApiVersion = 1;

    [[nodiscard]] std::string ToString(RE::BSFixedString value)
    {
        return value.c_str() ? value.c_str() : "";
    }

    std::int32_t GetNativeApiVersion(RE::StaticFunctionTag*)
    {
        return nativeApiVersion;
    }

    RE::BSFixedString GetPluginVersion(RE::StaticFunctionTag*)
    {
        return RE::BSFixedString(TLV::Version::semantic);
    }

    bool GetBoolSetting(RE::StaticFunctionTag*, RE::BSFixedString name)
    {
        return TLV::Settings::GetSingleton().GetBool(ToString(name));
    }

    float GetFloatSetting(RE::StaticFunctionTag*, RE::BSFixedString name)
    {
        return TLV::Settings::GetSingleton().GetFloat(ToString(name));
    }

    bool SetBoolSetting(
        RE::StaticFunctionTag*,
        RE::BSFixedString name,
        bool value)
    {
        return TLV::Settings::GetSingleton().SetBool(ToString(name), value);
    }

    bool SetFloatSetting(
        RE::StaticFunctionTag*,
        RE::BSFixedString name,
        float value)
    {
        return TLV::Settings::GetSingleton().SetFloat(ToString(name), value);
    }

    bool SaveSettings(RE::StaticFunctionTag*)
    {
        return TLV::Settings::GetSingleton().Save();
    }

    bool ReloadSettings(RE::StaticFunctionTag*)
    {
        TLV::Settings::GetSingleton().Load();
        logger::info("Settings reloaded through Papyrus API");
        return true;
    }

    bool ApplySettings(RE::StaticFunctionTag*)
    {
        auto& settings = TLV::Settings::GetSingleton();
        if (settings.Enabled() && settings.DirectApiEnabled()) {
            TLV::RealityRunnerApiClient::GetSingleton().Start();
            TLV::GameIntegration::GetSingleton().Initialize();
        } else {
            TLV::RealityRunnerApiClient::GetSingleton().Stop();
        }
        const auto applied = TLV::GameIntegration::GetSingleton().ApplySettings();
        logger::info("Settings applied through Papyrus API; success={}", applied);
        return applied;
    }
}

namespace TLV::PapyrusApi
{
    bool Register(RE::BSScript::IVirtualMachine* vm)
    {
        vm->RegisterFunction("GetNativeApiVersion", scriptName, GetNativeApiVersion);
        vm->RegisterFunction("GetPluginVersion", scriptName, GetPluginVersion);
        vm->RegisterFunction("GetBoolSetting", scriptName, GetBoolSetting);
        vm->RegisterFunction("GetFloatSetting", scriptName, GetFloatSetting);
        vm->RegisterFunction("SetBoolSetting", scriptName, SetBoolSetting);
        vm->RegisterFunction("SetFloatSetting", scriptName, SetFloatSetting);
        vm->RegisterFunction("SaveSettings", scriptName, SaveSettings);
        vm->RegisterFunction("ReloadSettings", scriptName, ReloadSettings);
        vm->RegisterFunction("ApplySettings", scriptName, ApplySettings);

        logger::info(
            "Registered Papyrus API {} version {}",
            scriptName,
            nativeApiVersion);
        return true;
    }
}
