#include "IntentTelemetry.h"

namespace TLV
{
    IntentTelemetry& IntentTelemetry::GetSingleton()
    {
        static IntentTelemetry singleton;
        return singleton;
    }

    bool IntentTelemetry::EnsureOpen()
    {
        if (stream_.is_open()) {
            return true;
        }
        if (openFailed_) {
            return false;
        }

        auto path = SKSE::log::log_directory();
        if (!path) {
            openFailed_ = true;
            logger::warn("Unable to locate SKSE log directory for intent telemetry");
            return false;
        }

        *path /= "TreadmillLocomotionVR_Intent.csv";
        stream_.open(*path, std::ios::out | std::ios::trunc);
        if (!stream_) {
            openFailed_ = true;
            logger::warn("Unable to open intent telemetry CSV: {}", path->string());
            return false;
        }

        stream_
            << "time_s,tick_ms,sample_sequence,frame_age_ms,stale,"
            << "joystick_value,device_run_signal,state,reason,"
            << "intended_left_y,intended_buttons\n";
        logger::info("Intent telemetry CSV opened: {}", path->string());
        return true;
    }

    void IntentTelemetry::Write(const IntentOutput& output)
    {
        if (!EnsureOpen()) {
            return;
        }
        if (firstTickMs_ == 0) {
            firstTickMs_ = output.tickMs;
        }
        const auto timeSeconds =
            static_cast<double>(output.tickMs - firstTickMs_) / 1000.0;

        stream_
            << timeSeconds << ','
            << output.tickMs << ','
            << output.sampleSequence << ','
            << output.frameAgeMs << ','
            << (output.stale ? 1 : 0) << ','
            << output.joystickValue << ','
            << (output.deviceRunSignal ? 1 : 0) << ','
            << IntentStateName(output.state) << ','
            << output.reason << ','
            << output.intendedLeftY << ','
            << output.intendedButtons << '\n';

        ++lineCount_;
        if ((lineCount_ % 120) == 0) {
            stream_.flush();
        }
    }
}
