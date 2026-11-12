// Підставити групи FFR у ядро -- або відступити.
//
// OpenZone Factions має пріоритет (рішення власника 2026-11-09): два
// постачальники однієї служби означали б, що ядро слухає того, хто
// підключився останнім, і різні моди бачили б різні фракції. Ім'я класу
// CfgMods рушій дефайнить сам, тож перевірка -- на етапі компіляції.
//
// Без файла FFR на старті склейка теж мовчить: тоді FFR на сервері немає, і
// порожня служба замість заглушки ядра лише ввімкнула б фракційні вкладки,
// яким нічого показати.
//
// The admin window (VPP rework 2026-11-12): only when the glue answers for factions does
// it bring its admin section, its config file and its card (OZI_FfrAdmin.c). The classes
// live in 4_World with the service, so the core's registries -- 3_Game and 4_World
// statics -- never hold an object of a script module unloaded before them.

modded class MissionServer
{
    override void OnMissionStart()
    {
        super.OnMissionStart();

#ifdef OpenZone_Factions
        OZ_Log.Warn("identity FFR: OpenZone Factions is loaded and answers for factions - OpenZone Identity FFR stays off; remove one of the two");
#else
        if (!FileExist(OZI_Ffr.FFR_FILE))
        {
            // Info, а не Warn: сервер без FFR -- це стан, а не помилка, і
            // вердикт стенду з max_warnings = 0 не мусить червоніти від нього.
            OZ_Log.Info("identity FFR: no " + OZI_Ffr.FFR_FILE + " - FFR_GroupSpawner is not on this server, OpenZone Identity FFR stays off");
            return;
        }
        OZI_FfrSettings s = OZI_Ffr.LoadSettings();
        OZI_FfrIdentity svc = new OZI_FfrIdentity(s);
        OZ_Identity.Provide(svc);

        OZ_AdminRegistry.Register(OZI_Ffr.SECTION, new OZI_FfrSection(svc));
        OZ_AdminCfg.Register(OZI_Ffr.CFG_NAME, OZI_Ffr.SettingsPath(), new OZI_FfrCfgApplier(svc), OZI_Ffr.CFG_OWNER);
        OZ_AdminFacts.Register(new OZI_FfrFacts(svc));

        string spends = "leaders only";
        if (s.AnyMemberSpends)
            spends = "any member";
        string line = "identity FFR: groups of FFR_GroupSpawner are the factions now: " + svc.MemberCount().ToString() + " players in groups, ";
        line += s.GroupFactions.Count().ToString() + " group names mapped, base " + s.BaseFaction + ", the pool is spent by " + spends;
        OZ_Log.Info(line);
#endif
    }
}
