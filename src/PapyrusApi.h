#pragma once

namespace RE::BSScript
{
    class IVirtualMachine;
}

namespace TLV::PapyrusApi
{
    bool Register(RE::BSScript::IVirtualMachine* vm);
}
