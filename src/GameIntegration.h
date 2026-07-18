#pragma once

namespace TLV
{
    class GameIntegration
    {
    public:
        static GameIntegration& GetSingleton();

        [[nodiscard]] bool Initialize();
        [[nodiscard]] bool ApplySettings();
        void Shutdown();

    private:
        using PlayerUpdate = void(RE::Actor*, float);

        GameIntegration() = default;
        ~GameIntegration();
        GameIntegration(const GameIntegration&) = delete;
        GameIntegration& operator=(const GameIntegration&) = delete;

        static void PlayerUpdateHook(RE::Actor* actor, float deltaSeconds);
        void OnFrame(float deltaSeconds);

        REL::Relocation<PlayerUpdate*> originalPlayerUpdate_;
        bool initialized_{ false };
    };
}
