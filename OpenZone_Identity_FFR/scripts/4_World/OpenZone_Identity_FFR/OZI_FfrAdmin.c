// The FFR glue in the OpenZone admin window (VPP rework 2026-11-12, design §4.3, §5.2).
//
// Three things, each registered at the mission start when the glue answers for factions
// (OZI_MissionServer.c):
//  - the admin section "ffr": ffr_groups lists FFR's groups with their players and the
//    faction each maps to, and the settings; ffr_reread reads FFR's file again first;
//  - OZ_Identity_FFR.json as a config of the core's files page, with an applier that
//    checks a new text, keeps a backup, writes it and applies it at once;
//  - a card on Ядро › Огляд.
//
// THE WIRE HAS A TWIN ON THE CLIENT. This glue is a server mod (-serverMod, README): a
// client never loads it, so the window cannot read these classes. The window parses
// the same JSON into its own classes of the same shape (OZ_VppFfrGroups in
// OpenZone_VPP); JSON carries field names, not class names. Change a field here,
// change it there.

class OZI_FfrGroupRow
{
    string Name;
    int    Members;
    // The OpenZone faction the group maps to; empty: none, the base axis only.
    string FactionId;
}

class OZI_FfrGroupsAnswer
{
    // FFR's file: read at the last try, how long ago, where it lives.
    bool   FileRead;
    int    ReadAgo;
    string FfrFile;
    int    Players;
    ref array<ref OZI_FfrGroupRow> Groups;

    // The settings as the service runs them now.
    string BaseFaction;
    bool   AnyMemberSpends;
    int    ReloadSeconds;

    void OZI_FfrGroupsAnswer()
    {
        Groups = new array<ref OZI_FfrGroupRow>();
    }
}

class OZI_FfrSection : OZ_AdminSection
{
    protected ref OZI_FfrIdentity m_Svc;

    void OZI_FfrSection(OZI_FfrIdentity svc)
    {
        m_Svc = svc;
    }

    override string Handle(string op, string json, PlayerIdentity sender, out bool ok, out string error)
    {
        ok = false;
        if (!m_Svc)
        {
            error = "STR_OZ_ERR_INTERNAL";
            return "";
        }

        if (op == OZI_Ffr.OP_GROUPS)
        {
            m_Svc.FfrFresh();
            return FfrGroupsJson(ok, error);
        }
        if (op == OZI_Ffr.OP_REREAD)
        {
            m_Svc.FfrReread();
            return FfrGroupsJson(ok, error);
        }

        error = "STR_OZ_ERR_UNKNOWN_OP";
        return "";
    }

    protected string FfrGroupsJson(out bool ok, out string error)
    {
        OZI_FfrGroupsAnswer a = new OZI_FfrGroupsAnswer();
        a.FileRead = m_Svc.FfrFileRead();
        a.ReadAgo = m_Svc.FfrReadAgo();
        a.FfrFile = OZI_Ffr.FFR_FILE;

        int players = 0;
        for (int i = 0; i < m_Svc.FfrGroupCount(); i++)
        {
            OZI_FfrGroupRow row = new OZI_FfrGroupRow();
            row.Name = m_Svc.FfrGroupName(i);
            row.Members = m_Svc.FfrMembers(row.Name);
            row.FactionId = m_Svc.FfrMapped(row.Name);
            players += row.Members;
            a.Groups.Insert(row);
        }
        a.Players = players;

        OZI_FfrSettings s = m_Svc.CurrentSettings();
        if (s)
        {
            a.BaseFaction = s.BaseFaction;
            a.AnyMemberSpends = s.AnyMemberSpends;
            a.ReloadSeconds = s.ReloadSeconds;
        }

        string outJson;
        string err;
        if (!JsonFileLoader<OZI_FfrGroupsAnswer>.MakeData(a, outJson, err, false))
        {
            error = "STR_OZ_ERR_INTERNAL";
            return "";
        }
        ok = true;
        return outJson;
    }
}

// OZ_Identity_FFR.json from the files page or the factions module's page: parsed into
// an object made by script, put in order (OZI_FfrSettings.Normalise), the old file kept
// as a backup, the new one written and used at once. A text that does not parse never
// reaches the disk.
class OZI_FfrCfgApplier : OZ_AdminCfgApplier
{
    protected ref OZI_FfrIdentity m_Svc;

    void OZI_FfrCfgApplier(OZI_FfrIdentity svc)
    {
        m_Svc = svc;
    }

    override bool Apply(string json)
    {
        OZI_FfrSettings s = new OZI_FfrSettings();
        string err;
        if (!JsonFileLoader<OZI_FfrSettings>.LoadData(json, s, err) || !s)
        {
            OZ_Log.Warn("identity FFR: OZ_Identity_FFR.json rejected: " + err);
            return false;
        }

        int fixes = s.Normalise();
        string path = OZI_Ffr.SettingsPath();
        if (FileExist(path))
            OZ_Json.Backup(path, OZI_Ffr.TAG);

        string werr;
        if (!JsonFileLoader<OZI_FfrSettings>.SaveFile(path, s, werr))
        {
            OZ_Log.Error("identity FFR: cannot write " + path + ": " + werr);
            return false;
        }

        OZI_Ffr.s_Problems = fixes;
        if (m_Svc)
            m_Svc.UseSettings(s);

        string line = "identity FFR: settings applied, " + s.GroupFactions.Count().ToString();
        line += " group names, base " + s.BaseFaction;
        line += ", fixed " + fixes.ToString();
        OZ_Log.Info(line);
        return true;
    }
}

// The glue's card on Ядро › Огляд: FFR's groups and players, the base faction and who
// spends the pool; problems are FFR's groups no row maps, a broken or fixed settings
// file, and FFR's file unread.
class OZI_FfrFacts : OZ_AdminFactsProvider
{
    protected ref OZI_FfrIdentity m_Svc;

    void OZI_FfrFacts(OZI_FfrIdentity svc)
    {
        m_Svc = svc;
    }

    override void Fill(OZ_AdminModuleCard card)
    {
        card.Id = OZI_Ffr.TAG;
        card.Name = "#STR_OZ_FFR_CARD";
        card.Pages = 1;
        if (!m_Svc)
            return;

        int problems = m_Svc.FfrUnmapped() + OZI_Ffr.s_Problems;
        if (!m_Svc.FfrFileRead())
            problems++;
        card.Problems = problems;

        int players = 0;
        for (int i = 0; i < m_Svc.FfrGroupCount(); i++)
            players += m_Svc.FfrMembers(m_Svc.FfrGroupName(i));

        card.Add("#STR_OZ_FFR_FACT_GROUPS", m_Svc.FfrGroupCount().ToString());
        card.Add("#STR_OZ_FFR_FACT_PLAYERS", players.ToString());

        OZI_FfrSettings s = m_Svc.CurrentSettings();
        if (!s)
            return;
        card.Add("#STR_OZ_FFR_FACT_BASE", s.BaseFaction);
        if (s.AnyMemberSpends)
            card.Add("#STR_OZ_FFR_FACT_SPENDS", "#STR_OZ_FFR_SPENDS_ANY");
        else
            card.Add("#STR_OZ_FFR_FACT_SPENDS", "#STR_OZ_FFR_SPENDS_NOBODY");
    }

    // Our file read from disk again and used; FFR's file read again with it.
    override bool Reload(array<string> said)
    {
        if (!m_Svc)
            return false;
        OZI_FfrSettings s = OZI_Ffr.LoadSettings();
        m_Svc.UseSettings(s);
        said.Insert("#STR_OZ_FFR_RELOADED");
        return true;
    }
}
