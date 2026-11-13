// OpenZone VPP tab -- the OPTIONAL bridge between OpenZone and VPP Admin
// Tools. A separate pbo on purpose: requiredAddons below is a HARD
// dependency (a blocking "requires addon" dialog, not a silent skip), and
// the core must keep working on servers that run no VPP at all. The core's
// own VPP awareness is soft (#ifdef AVPPAdminTools) and lives over there.
//
// The exact VPP addon name is DZM_VPPAdminToolsScripts -- the CfgPatches
// class of VPP's scripts pbo. The #ifdef in code guards on AVPPAdminTools
// instead: the engine auto-defines CfgMods class names, not CfgPatches
// (measured 2026-07-31 on 1.29 diag).

class CfgPatches
{
    class OpenZone_VPP
    {
        units[] = {};
        weapons[] = {};
        requiredVersion = 0.1;
        requiredAddons[] =
        {
            "DZ_Data",
            "DZ_Scripts",
            "OpenZone_Core",
            "DZM_VPPAdminToolsScripts"
        };
    };
};

class CfgMods
{
    class OpenZone_VPP
    {
        dir = "OpenZone_VPP";
        name = "OpenZone VPP Tab";
        author = "OpenZone";
        version = "0.1.0";
        type = "mod";

        dependencies[] = {"Mission"};

        class defs
        {
            // The window's small signs (dots, chevrons, the check, the magnifier): Fira Sans,
            // the window's face, has none of them. An imageset no config lists draws nothing.
            class imageSets { files[] = {"OpenZone_VPP/gui/imagesets/ozv_ui.imageset"}; };
            class missionScriptModule { value = ""; files[] = {"OpenZone_VPP/scripts/5_Mission"}; };
        };
    };
};
