#pragma once

#include <cstdint>

namespace TLV
{
    class XInputLocomotionOutput
    {
    public:
        static XInputLocomotionOutput& GetSingleton();

        [[nodiscard]] bool Initialize();
        [[nodiscard]] bool IsInstalled() const;
        [[nodiscard]] bool HasStateHook() const;
        // True when this mod's output can reach the game: either it owns the
        // XInput hook directly, or it has joined the shared mux and the hook
        // owner will apply its contribution.
        [[nodiscard]] bool IsOutputPathReady() const;
        void Shutdown();
        void SetLocomotion(std::int16_t leftY, std::uint16_t buttons);
        [[nodiscard]] float LastAppliedLeftY() const;
        [[nodiscard]] std::uint16_t LastAppliedButtons() const;

    private:
        XInputLocomotionOutput() = default;
        ~XInputLocomotionOutput();

        XInputLocomotionOutput(const XInputLocomotionOutput&) = delete;
        XInputLocomotionOutput& operator=(const XInputLocomotionOutput&) = delete;

        bool installed_{ false };
    };
}
