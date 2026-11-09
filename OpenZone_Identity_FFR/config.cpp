// OpenZone Identity FFR -- the factions of a server that runs FFR_GroupSpawner (the VPP tab
// "Groups" by Gart): its groups become the factions the OpenZone core answers with, so
// OpenZone Research gives each group its tree and pool.
//
// A separate pbo, like every glue of the series: the core knows nothing about FFR. FFR itself
// is never touched -- this mod only READS the JSON that FFR rewrites after every change of its
// groups, so an FFR update can never break the server's script compile.
//
// Not together with OpenZone Factions: when both are loaded, OpenZone Factions answers and this
// mod steps back with one line in the log (the owner's decision of 2026-11-09).
//
// SERVER-SIDE: load it with -serverMod. Everything it does happens on the server (the core's
// identity service answers there), and in -mod it would make every client download it --
// a client without it is kicked "missing a mod which is on the server" (measured 2026-11-09).

class CfgPatches
{
    class OpenZone_Identity_FFR
    {
        units[] = {};
        weapons[] = {};
        requiredVersion = 0.1;
        requiredAddons[] =
        {
            "DZ_Data",
            "DZ_Scripts",
            "OpenZone_Core"
        };
    };
};

class CfgMods
{
    class OpenZone_Identity_FFR
    {
        dir = "OpenZone_Identity_FFR";
        name = "OpenZone Identity FFR";
        author = "OpenZone";
        version = "0.1.0";
        type = "mod";

        dependencies[] = {"World", "Mission"};

        class defs
        {
            class worldScriptModule
            {
                value = "";
                files[] = {"OpenZone_Identity_FFR/scripts/4_World"};
            };
            class missionScriptModule
            {
                value = "";
                files[] = {"OpenZone_Identity_FFR/scripts/5_Mission"};
            };
        };
    };
};
