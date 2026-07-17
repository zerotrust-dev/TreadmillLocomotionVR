#include "Settings.h"
#include "Version.h"
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
        spdlog::set_level(spdlog::level::debug);
        spdlog::flush_on(spdlog::level::debug);
    }

    void OnSKSEMessage(SKSE::MessagingInterface::Message* message)
    {
        if (message->type != SKSE::MessagingInterface::kDataLoaded) {
            return;
        }

        auto& settings = TLV::Settings::GetSingleton();
        settings.Load();

        if (!settings.Enabled()) {
            logger::info("Treadmill locomotion telemetry is disabled");
            return;
        }

        if (TLV::XInputTelemetryProbe::GetSingleton().Initialize()) {
            logger::info("Treadmill locomotion telemetry probe initialized");
        } else {
            logger::warn("Treadmill locomotion telemetry probe did not initialize");
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

    logger::info("Plugin loaded successfully");
    return true;
}
