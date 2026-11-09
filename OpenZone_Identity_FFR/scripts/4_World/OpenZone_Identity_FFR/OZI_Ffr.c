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
}

class OZI_Ffr
{
    static const string FFR_FILE = "$profile:FFR_Mods\\GroupSpawner\\GroupSpawner.json";

    static string SettingsPath()
    {
        return OZ_Const.PROFILE_DIR + "\\OZ_Identity_FFR.json";
    }

    // Наш файл: прочитати, а якщо його немає -- записати засіяний. Зіпсований
    // файл не перетираємо: адмін побачить ERROR і виправить, а ми працюємо
    // на засіяних значеннях до наступного старту.
    static OZI_FfrSettings LoadSettings()
    {
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
            return s;
        }
        if (!s.GroupFactions)
            s.GroupFactions = new array<ref OZI_FfrMap>();
        if (s.ReloadSeconds < 5)
            s.ReloadSeconds = 5;
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
    protected int m_LoadedAt;
    protected bool m_Loaded;
    protected string m_LastError;

    void OZI_FfrIdentity(OZI_FfrSettings settings)
    {
        m_Set = settings;
        m_GroupToFaction = new map<string, string>();
        m_FactionToGroup = new map<string, string>();
        m_UidToGroup = new map<string, string>();
        m_Unmapped = new array<string>();
        foreach (OZI_FfrMap m : m_Set.GroupFactions)
        {
            if (!m || m.FfrGroup == "" || m.FactionId == "")
                continue;
            m_GroupToFaction.Set(m.FfrGroup, m.FactionId);
            if (!m_FactionToGroup.Contains(m.FactionId))
                m_FactionToGroup.Set(m.FactionId, m.FfrGroup);
        }
        Reload();
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
            return;
        }
        m_LastError = "";

        map<string, string> fresh = new map<string, string>();
        if (file.Groups)
        {
            foreach (OZI_FfrGroup g : file.Groups)
            {
                if (!g || !g.Players)
                    continue;
                foreach (string uid : g.Players)
                {
                    if (uid != "")
                        fresh.Set(uid, g.GroupName);
                }
            }
        }
        m_UidToGroup = fresh;
    }

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
}
