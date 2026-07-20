#include "Settings.h"
#include "Version.h"
#include "GameIntegration.h"
#include "PapyrusApi.h"
#include "RealityRunnerApiClient.h"
#include "XInputTelemetryProbe.h"

namespace
{
    void InitializeLogging()
    {
        auto path = SKSE::log::log_directory();
        if (!path) {
            SKSE::stl::report_and_fail("Unable to locate SKSE log directory");
        }

        *path /= "TreadmillLocomotionVR.log";
        auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(
            path->string(),
            true);
        auto log = std::make_shared<spdlog::logger>("global", std::move(sink));

        spdlog::set_default_logger(std::move(log));
        spdlog::set_level(spdlog::level::info);
        spdlog::flush_on(spdlog::level::warn);
    }

    void OnSKSEMessage(SKSE::MessagingInterface::Message* message)
    {
        if (message->type != SKSE::MessagingInterface::kDataLoaded) {
            return;
        }

        auto& settings = TLV::Settings::GetSingleton();
        settings.Load();
        spdlog::set_level(
            settings.DebugLogging() ?
                spdlog::level::debug :
                spdlog::level::info);

        if (!settings.Enabled()) {
            logger::info("Treadmill locomotion telemetry is disabled");
            return;
        }

        if (settings.DirectApiEnabled()) {
            if (TLV::RealityRunnerApiClient::GetSingleton().Start()) {
                logger::info("RealityRunner direct API reader started");
            } else {
                logger::warn("RealityRunner direct API reader did not start");
            }

            if (TLV::GameIntegration::GetSingleton().Initialize()) {
                logger::info("Treadmill locomotion intent integration initialized");
            } else {
                logger::warn("Treadmill locomotion intent integration did not initialize");
            }
            return;
        }

        if (settings.PatchXInput()) {
            if (TLV::XInputTelemetryProbe::GetSingleton().Initialize()) {
                logger::info("Treadmill locomotion telemetry probe initialized");
            } else {
                logger::warn("Treadmill locomotion telemetry probe did not initialize");
            }
        }
    }
}

SKSEPluginLoad(const SKSE::LoadInterface* skse)
{
    InitializeLogging();
    logger::info("{} {} loading", TLV::Version::name, TLV::Version::semantic);

    SKSE::Init(skse);

    const auto messaging = SKSE::GetMessagingInterface();
    if (!messaging || !messaging->RegisterListener(OnSKSEMessage)) {
        logger::critical("Unable to register SKSE message listener");
        return false;
    }

    const auto papyrus = SKSE::GetPapyrusInterface();
    if (!papyrus || !papyrus->Register(TLV::PapyrusApi::Register)) {
        logger::critical("Unable to register Papyrus API");
        return false;
    }

    logger::info("Plugin loaded successfully");
    return true;
}
