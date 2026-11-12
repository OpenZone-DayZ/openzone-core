// Групи FFR_GroupSpawner як фракції ядра OpenZone.
//
// ЧИТАЄМО ФАЙЛ, А НЕ КЛАСИ FFR. Менеджер FFR живе в 5_Mission, його пошук
// гравця protected, а сам мод оновлюється без нас: виклик у його клас -- це
// помилка компіляції ВСЬОГО сервера після першого ж перейменування в нього.
// Свій JSON він переписує сам після кожної зміни груп (SaveConfig на кожному
// призначенні й вилученні), тож файл і є правда, лише з запізненням до
// ReloadSeconds. Невідомі поля (точка спавну, спорядження) серіалізатор
// пропускає: нам потрібні тільки GroupName і Players.
//
// ДВІ ОСІ ЯДРА. Угруповання -- група FFR, переведена через GroupFactions у id фракції
// OpenZone (власника дерева в research: duty, freedom, ...). Базова --
// одна на всіх, BaseFaction. Група без рядка в GroupFactions не дає угруповання: її
// члени лишаються на базовій осі, а в лог іде одне попередження на групу.
//
// ХТО ТРАТИТЬ ПУЛ. Лідерів у FFR немає. Рішення власника 2026-11-09: тратить
// будь-який член групи. Research питає саме IsLeader, тож з AnyMemberSpends
// «лідер» тут означає «член групи»; без нього лідерів немає зовсім.
//
// THE ADMIN WINDOW (VPP rework 2026-11-12, design §4.3 and §5.2): OZ_Identity_FFR.json
// is a registered config of the core's files page, and the factions module of the
// window shows the groups, their mapping and these settings (OZI_FfrAdmin.c). A new
// text applies at once: UseSettings rebuilds the maps and reads FFR's file again.

class OZI_FfrGroup
{
    string GroupName;
    ref array<string> Players;
}

class OZI_FfrFile
{
    ref array<ref OZI_FfrGroup> Groups;
}

class OZI_FfrMap
{
    string FfrGroup;   // GroupName у FFR, точний збіг
    string FactionId;  // id фракції OpenZone

    void OZI_FfrMap(string group = "", string faction = "")
    {
        FfrGroup = group;
        FactionId = faction;
    }

    // A copy made by script (the core's OZ_ConfigBase header): the service reads its
    // rows long after the file was parsed (the admin window lists them).
    OZI_FfrMap Copy()
    {
        return new OZI_FfrMap(FfrGroup, FactionId);
    }
}

class OZI_FfrSettings
{
    int Version = 1;
    ref array<ref OZI_FfrMap> GroupFactions;
    string BaseFaction = "neutral";
    bool AnyMemberSpends = true;
    int ReloadSeconds = 30;

    // Перший старт: найчастіші назви угруповань у RP-серверах сталкера --
    // українською, і «Долг» (єдиний виняток серії з правила мов). Точні
    // назви груп конкретного сервера, зокрема іншою мовою, адмін дописує сам.
    void Seed()
    {
        GroupFactions = new array<ref OZI_FfrMap>();
        GroupFactions.Insert(new OZI_FfrMap("Долг", "duty"));
        GroupFactions.Insert(new OZI_FfrMap("Свобода", "freedom"));
        GroupFactions.Insert(new OZI_FfrMap("Воля", "freedom"));
        GroupFactions.Insert(new OZI_FfrMap("Вчені", "ecolog"));
        GroupFactions.Insert(new OZI_FfrMap("Екологи", "ecolog"));
        GroupFactions.Insert(new OZI_FfrMap("Чисте небо", "clearsky"));
        GroupFactions.Insert(new OZI_FfrMap("СОП", "sop"));
        GroupFactions.Insert(new OZI_FfrMap("Бандити", "bandit"));
        GroupFactions.Insert(new OZI_FfrMap("Одинаки", "neutral"));
    }

    // Right after a parse, while the reads are honest: every row into one made by
    // script, rows with an empty name or id dropped, the period kept at 5 s or more.
    // Returns how many things it fixed (the admin window counts them as problems).
    int Normalise()
    {
        int fixes = 0;
        array<ref OZI_FfrMap> rows = new array<ref OZI_FfrMap>();
        if (GroupFactions)
        {
            for (int i = 0; i < GroupFactions.Count(); i++)
            {
                OZI_FfrMap m = GroupFactions[i];
                if (!m)
                {
                    fixes++;
                    continue;
                }
                OZI_FfrMap c = m.Copy();
                c.FfrGroup = c.FfrGroup.Trim();
                c.FactionId = c.FactionId.Trim();
                if (c.FfrGroup == "" || c.FactionId == "")
                {
                    fixes++;
                    continue;
                }
                rows.Insert(c);
            }
        }
        GroupFactions = rows;

        BaseFaction = BaseFaction.Trim();
        if (ReloadSeconds < 5)
        {
            ReloadSeconds = 5;
            fixes++;
        }
        return fixes;
    }
}

class OZI_Ffr
{
    static const string FFR_FILE = "$profile:FFR_Mods\\GroupSpawner\\GroupSpawner.json";

    // The admin section, its ops and the config's name in the core's files page. The
    // admin window has its own copy of these strings (OZ_VppMenu.c, FFR_*): the glue
    // is a server mod, so the client never sees this class.
    static const string SECTION   = "ffr";
    static const string OP_GROUPS = "ffr_groups";
    static const string OP_REREAD = "ffr_reread";
    static const string CFG_NAME  = "IdentityFFR";
    static const string CFG_OWNER = "factions";
    static const string TAG       = "identity_ffr";

    // What the last read of our own file did: 1 when it was broken and the built-in
    // names stand in for it, plus what Normalise fixed (the admin window's problems).
    static int s_Problems = 0;

    static string SettingsPath()
    {
        return OZ_Const.PROFILE_DIR + "\\OZ_Identity_FFR.json";
    }

    // Наш файл: прочитати, а якщо його немає -- записати засіяний. Зіпсований
    // файл не перетираємо: адмін побачить ERROR і виправить, а ми працюємо
    // на засіяних значеннях до наступного старту.
    static OZI_FfrSettings LoadSettings()
    {
        s_Problems = 0;
        OZI_FfrSettings s = new OZI_FfrSettings();
        string err;
        string path = SettingsPath();
        if (!FileExist(path))
        {
            s.Seed();
            if (!JsonFileLoader<OZI_FfrSettings>.SaveFile(path, s, err))
                OZ_Log.Error("identity FFR: cannot write " + path + ": " + err);
            else
                OZ_Log.Info("identity FFR: wrote " + path + " with " + s.GroupFactions.Count().ToString() + " group names - put the exact names of this server's groups there");
            return s;
        }
        if (!JsonFileLoader<OZI_FfrSettings>.LoadFile(path, s, err))
        {
            OZ_Log.Error("identity FFR: " + path + " is broken, using the built-in names until it is fixed: " + err);
            s = new OZI_FfrSettings();
            s.Seed();
            s_Problems = 1;
            return s;
        }
        s_Problems = s.Normalise();
        return s;
    }
}

class OZI_FfrIdentity : OZ_IdentityService
{
    protected ref OZI_FfrSettings m_Set;
    protected ref map<string, string> m_GroupToFaction;
    protected ref map<string, string> m_FactionToGroup;
    protected ref map<string, string> m_UidToGroup;
    protected ref array<string> m_Unmapped;

    // The groups of FFR's file as the last read found them, in its order, and how many
    // players each holds (a group without players is still a group).
    protected ref array<string> m_GroupNames;
    protected ref map<string, int> m_GroupCounts;
    protected int m_LoadedAt;
    protected bool m_Loaded;
    protected bool m_FileRead;
    protected string m_LastError;

    void OZI_FfrIdentity(OZI_FfrSettings settings)
    {
        m_Set = settings;
        m_GroupToFaction = new map<string, string>();
        m_FactionToGroup = new map<string, string>();
        m_UidToGroup = new map<string, string>();
        m_Unmapped = new array<string>();
        m_GroupNames = new array<string>();
        m_GroupCounts = new map<string, int>();
        BuildMaps();
        Reload();
    }

    protected void BuildMaps()
    {
        m_GroupToFaction.Clear();
        m_FactionToGroup.Clear();
        m_Unmapped.Clear();
        if (!m_Set || !m_Set.GroupFactions)
            return;
        foreach (OZI_FfrMap m : m_Set.GroupFactions)
        {
            if (!m || m.FfrGroup == "" || m.FactionId == "")
                continue;
            m_GroupToFaction.Set(m.FfrGroup, m.FactionId);
            if (!m_FactionToGroup.Contains(m.FactionId))
                m_FactionToGroup.Set(m.FactionId, m.FfrGroup);
        }
    }

    // A new text of OZ_Identity_FFR.json, checked and saved by the admin window's
    // applier: the maps follow it at once and FFR's file is read again.
    void UseSettings(OZI_FfrSettings settings)
    {
        if (!settings)
            return;
        m_Set = settings;
        BuildMaps();
        Reload();
    }

    OZI_FfrSettings CurrentSettings()
    {
        return m_Set;
    }

    // Скільки гравців знайдено в групах -- для рядка в лозі при старті.
    int MemberCount()
    {
        return m_UidToGroup.Count();
    }

    protected void Fresh()
    {
        if (!m_Loaded || GetGame().GetTime() - m_LoadedAt >= m_Set.ReloadSeconds * 1000)
            Reload();
    }

    protected void Reload()
    {
        m_Loaded = true;
        m_LoadedAt = GetGame().GetTime();

        OZI_FfrFile file = new OZI_FfrFile();
        string err;
        if (!JsonFileLoader<OZI_FfrFile>.LoadFile(OZI_Ffr.FFR_FILE, file, err))
        {
            // Одне повідомлення на одну й ту саму причину: файл перечитується
            // щопівхвилини, і повтор засипав би лог.
            if (err != m_LastError)
                OZ_Log.Warn("identity FFR: cannot read " + OZI_Ffr.FFR_FILE + ", keeping the groups read before: " + err);
            m_LastError = err;
            m_FileRead = false;
            return;
        }
        m_LastError = "";
        m_FileRead = true;

        map<string, string> fresh = new map<string, string>();
        array<string> names = new array<string>();
        map<string, int> counts = new map<string, int>();
        if (file.Groups)
        {
            foreach (OZI_FfrGroup g : file.Groups)
            {
                if (!g)
                    continue;
                string gname = g.GroupName;
                if (names.Find(gname) == -1)
                {
                    names.Insert(gname);
                    counts.Set(gname, 0);
                }
                if (!g.Players)
                    continue;
                foreach (string uid : g.Players)
                {
                    if (uid == "")
                        continue;
                    fresh.Set(uid, gname);
                    counts.Set(gname, counts.Get(gname) + 1);
                }
            }
        }
        m_UidToGroup = fresh;
        m_GroupNames = names;
        m_GroupCounts = counts;
    }

    // ------------------------------------------------- for the admin window

    void FfrFresh()
    {
        Fresh();
    }

    void FfrReread()
    {
        Reload();
    }

    bool FfrFileRead()
    {
        return m_FileRead;
    }

    int FfrReadAgo()
    {
        int ms = GetGame().GetTime() - m_LoadedAt;
        return ms / 1000;
    }

    int FfrGroupCount()
    {
        return m_GroupNames.Count();
    }

    string FfrGroupName(int i)
    {
        if (i < 0 || i >= m_GroupNames.Count())
            return "";
        return m_GroupNames[i];
    }

    int FfrMembers(string group)
    {
        int n;
        if (m_GroupCounts.Find(group, n))
            return n;
        return 0;
    }

    // The faction a group maps to, without the warning FactionOfGroup writes.
    string FfrMapped(string group)
    {
        string f;
        if (m_GroupToFaction.Find(group, f))
            return f;
        return "";
    }

    // Groups of FFR's file with no row in GroupFactions: their members count as the
    // base faction only.
    int FfrUnmapped()
    {
        int n = 0;
        for (int i = 0; i < m_GroupNames.Count(); i++)
        {
            if (!m_GroupToFaction.Contains(m_GroupNames[i]))
                n++;
        }
        return n;
    }

    // ------------------------------------------------- the core's contract

    string GroupOf(string uid)
    {
        if (uid == "")
            return "";
        Fresh();
        string g;
        if (m_UidToGroup.Find(uid, g))
            return g;
        return "";
    }

    protected string FactionOfGroup(string group)
    {
        if (group == "")
            return "";
        string f;
        if (m_GroupToFaction.Find(group, f))
            return f;
        if (m_Unmapped.Find(group) < 0)
        {
            m_Unmapped.Insert(group);
            OZ_Log.Warn("identity FFR: group '" + group + "' has no faction in " + OZI_Ffr.SettingsPath() + " - its members count as " + m_Set.BaseFaction);
        }
        return "";
    }

    override string OrgOf(string uid)
    {
        return FactionOfGroup(GroupOf(uid));
    }

    // FFR пише Steam64, а коли його немає -- GUID ідентичності. Живий гравець
    // дає спитати обидва.
    override string OrgOfPlayer(PlayerBase player, string uid)
    {
        string f = OrgOf(uid);
        if (f != "" || !player || !player.GetIdentity())
            return f;
        return FactionOfGroup(GroupOf(player.GetIdentity().GetId()));
    }

    override string BaseOf(string uid)
    {
        return m_Set.BaseFaction;
    }

    override string FactionName(string id)
    {
        string g;
        if (m_FactionToGroup.Find(id, g))
            return g;
        return id;
    }

    override void FactionIds(out array<string> outIds)
    {
        outIds = new array<string>();
        for (int i = 0; i < m_GroupToFaction.Count(); i++)
        {
            string f = m_GroupToFaction.GetElement(i);
            if (outIds.Find(f) < 0)
                outIds.Insert(f);
        }
        if (m_Set.BaseFaction != "" && outIds.Find(m_Set.BaseFaction) < 0)
            outIds.Insert(m_Set.BaseFaction);
    }

    override bool IsLeader(string uid)
    {
        return m_Set.AnyMemberSpends && GroupOf(uid) != "";
    }

    // What this source can say (the core's contract, VPP rework 2026-11-12):
    // FFR has no leaders and no posts; with AnyMemberSpends every member of a
    // group answers as its leader.
    override string Source()
    {
        return "OpenZone Identity FFR";
    }

    override bool HasLeaders()
    {
        return m_Set.AnyMemberSpends;
    }

    override bool HasPosts()
    {
        return false;
    }

    override string LeaderMeans()
    {
        if (m_Set.AnyMemberSpends)
            return "#STR_OZ_FFR_LEADER_ANY_MEMBER";
        return "";
    }
}
