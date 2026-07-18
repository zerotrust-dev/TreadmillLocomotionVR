using Mutagen.Bethesda.Plugins;
using Mutagen.Bethesda.Plugins.Binary.Parameters;
using Mutagen.Bethesda.Skyrim;
using Noggog;

if (args.Length != 1)
{
    Console.Error.WriteLine("Usage: McmPluginBuilder <output-esp-path>");
    Environment.Exit(2);
}

var outputPath = Path.GetFullPath(args[0]);
Directory.CreateDirectory(Path.GetDirectoryName(outputPath)!);

var modKey = ModKey.FromNameAndExtension(Path.GetFileName(outputPath));
var mod = new SkyrimMod(modKey, SkyrimRelease.SkyrimSE)
{
    ModHeader =
    {
        Author = "zerotrust-dev",
        Description = "Treadmill Locomotion VR in-game MCM menu."
    }
};

var quest = new Quest(mod.GetNextFormKey(), SkyrimRelease.SkyrimSE)
{
    EditorID = "TLV_MCMQuest",
    Name = "Treadmill Locomotion VR MCM",
    Flags = Quest.Flag.StartGameEnabled,
    Priority = 0,
    QuestFormVersion = 65,
    VirtualMachineAdapter = new QuestAdapter
    {
        ExtraBindDataVersion = 1,
        FileName = "TreadmillLocomotionVR_MCM"
    }
};

quest.VirtualMachineAdapter.Scripts.Add(new ScriptEntry
{
    Name = "TreadmillLocomotionVR_MCM"
});

mod.Quests.Add(quest);
mod.SyncRecordCount();
mod.WriteToBinary(new FilePath(outputPath), BinaryWriteParameters.Default);

Console.WriteLine($"Wrote {outputPath}");
