#pragma once

#include "LocomotionIntent.h"

namespace TLV
{
    class IntentTelemetry
    {
    public:
        static IntentTelemetry& GetSingleton();

        void Write(const IntentOutput& output);

    private:
        bool EnsureOpen();

        std::ofstream stream_;
        bool openFailed_{ false };
        std::uint64_t firstTickMs_{ 0 };
        std::uint64_t lineCount_{ 0 };
    };
}
