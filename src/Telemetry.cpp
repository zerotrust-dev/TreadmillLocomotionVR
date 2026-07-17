#include "Telemetry.h"

namespace TLV
{
    Telemetry& Telemetry::GetSingleton()
    {
        static Telemetry telemetry;
        return telemetry;
    }

    bool Telemetry::EnsureOpen()
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
            logger::warn("Unable to locate SKSE log directory for treadmill telemetry");
            return false;
        }

        *path /= "TreadmillLocomotionVR_Telemetry.csv";
        stream_.open(*path, std::ios::out | std::ios::trunc);
        if (!stream_) {
            openFailed_ = true;
            logger::warn("Unable to open treadmill telemetry CSV: {}", path->string());
            return false;
        }

        stream_
            << "sequence,time_s,tick_ms,user_index,result,packet_number,"
            << "buttons,left_trigger,right_trigger,"
            << "thumb_lx,thumb_ly,thumb_rx,thumb_ry,"
            << "norm_lx,norm_ly,norm_rx,norm_ry,forward_band\n";
        logger::info("Treadmill telemetry CSV opened: {}", path->string());
        return true;
    }

    void Telemetry::WriteSample(
        const XInputTelemetrySample& sample,
        const AnalysisSettings& analysis)
    {
        if (!EnsureOpen()) {
            return;
        }
        if (firstTickMs_ == 0) {
            firstTickMs_ = sample.tickMs;
        }

        const auto normLX = NormalizeStick(sample.thumbLX);
        const auto normLY = NormalizeStick(sample.thumbLY);
        const auto normRX = NormalizeStick(sample.thumbRX);
        const auto normRY = NormalizeStick(sample.thumbRY);
        const auto band = ClassifyForwardSpeed(normLY, analysis);
        const auto timeSeconds =
            static_cast<double>(sample.tickMs - firstTickMs_) / 1000.0;

        stream_
            << sample.sequence << ','
            << timeSeconds << ','
            << sample.tickMs << ','
            << sample.userIndex << ','
            << sample.result << ','
            << sample.packetNumber << ','
            << sample.buttons << ','
            << static_cast<int>(sample.leftTrigger) << ','
            << static_cast<int>(sample.rightTrigger) << ','
            << sample.thumbLX << ','
            << sample.thumbLY << ','
            << sample.thumbRX << ','
            << sample.thumbRY << ','
            << normLX << ','
            << normLY << ','
            << normRX << ','
            << normRY << ','
            << LocomotionBandName(band)
            << '\n';

        if ((sample.sequence % 120) == 0) {
            stream_.flush();
        }
    }
}
