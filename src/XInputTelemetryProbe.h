#pragma once

#include "Telemetry.h"

namespace TLV
{
    class XInputTelemetryProbe
    {
    public:
        static XInputTelemetryProbe& GetSingleton();

        [[nodiscard]] bool Initialize();
        void Shutdown();

        [[nodiscard]] bool IsInstalled() const;

    private:
        XInputTelemetryProbe() = default;
        ~XInputTelemetryProbe();
        XInputTelemetryProbe(const XInputTelemetryProbe&) = delete;
        XInputTelemetryProbe& operator=(const XInputTelemetryProbe&) = delete;

        bool installed_{ false };
    };
}
