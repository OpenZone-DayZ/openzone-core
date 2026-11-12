// Ядро › Огляд (VPP rework 2026-11-12, design §4.3): a card per module of the
// series -- whether it has pages here, how many problems its files had at the
// last load, a few facts -- and "read every module's files again".
//
// The core names no other mod: each module registers a provider of its own at
// its mission start (OZ_AdminFacts.Register), as it registers its admin
// sections and its configs. A module that registers nothing has no card.

// One fact of a card: a stringtable key and its value (a number, words or a key).
class OZ_AdminFact
{
    string Label;
    string Value;

    OZ_AdminFact Copy()
    {
        OZ_AdminFact c = new OZ_AdminFact();
        c.Label = Label;
        c.Value = Value;
        return c;
    }
}

// A module's card.
class OZ_AdminModuleCard
{
    // "core", "factions", ...; empty: no card.
    string Id;
    // A stringtable key: the module's name, as in the rail.
    string Name;
    // 1: the module has pages in this window; 0: it is run from the game alone.
    int Pages;
    // Problems its files had at the last load; -1: the module does not count them.
    int Problems;
    ref array<ref OZ_AdminFact> Facts;

    void OZ_AdminModuleCard()
    {
        Pages = 1;
        Problems = -1;
        Facts = new array<ref OZ_AdminFact>();
    }

    void Add(string label, string value)
    {
        OZ_AdminFact f = new OZ_AdminFact();
        f.Label = label;
        f.Value = value;
        Facts.Insert(f);
    }

    OZ_AdminModuleCard Copy()
    {
        OZ_AdminModuleCard c = new OZ_AdminModuleCard();
        c.Id = Id;
        c.Name = Name;
        c.Pages = Pages;
        c.Problems = Problems;
        if (!Facts)
            return c;
        for (int i = 0; i < Facts.Count(); i++)
        {
            if (Facts[i])
                c.Facts.Insert(Facts[i].Copy());
        }
        return c;
    }
}

// The bridge as the overview names it.
class OZ_AdminBridge
{
    static const int OFF  = 0;
    static const int UP   = 1;
    static const int DOWN = 2;
}

// The answer of `overview`.
class OZ_AdminOverview
{
    int Players;
    // serverDZ.cfg maxPlayers; 0 when the server does not say.
    int MaxPlayers;
    float Fps;
    // OZ_AdminBridge.
    int Bridge;
    // The sum of the cards that count their problems.
    int Problems;
    ref array<ref OZ_AdminModuleCard> Modules;

    void OZ_AdminOverview()
    {
        Modules = new array<ref OZ_AdminModuleCard>();
    }

    OZ_AdminOverview Copy()
    {
        OZ_AdminOverview c = new OZ_AdminOverview();
        c.Players = Players;
        c.MaxPlayers = MaxPlayers;
        c.Fps = Fps;
        c.Bridge = Bridge;
        c.Problems = Problems;
        if (!Modules)
            return c;
        for (int i = 0; i < Modules.Count(); i++)
        {
            if (Modules[i])
                c.Modules.Insert(Modules[i].Copy());
        }
        return c;
    }
}

// One module's line of `reload_all`.
class OZ_AdminReloadLine
{
    string Id;
    string Name;
    // 1: its files were read again; 0: the module has no reload of its own (a
    // restart applies its files).
    int Done;
    int Problems;
    // What the module said: stringtable keys or words.
    ref array<string> Said;

    void OZ_AdminReloadLine()
    {
        Said = new array<string>();
    }

    OZ_AdminReloadLine Copy()
    {
        OZ_AdminReloadLine c = new OZ_AdminReloadLine();
        c.Id = Id;
        c.Name = Name;
        c.Done = Done;
        c.Problems = Problems;
        if (!Said)
            return c;
        for (int i = 0; i < Said.Count(); i++)
            c.Said.Insert(Said[i]);
        return c;
    }
}

// The answer of `reload_all`.
class OZ_AdminReloadReport
{
    int Done;
    int Problems;
    ref array<ref OZ_AdminReloadLine> Lines;

    void OZ_AdminReloadReport()
    {
        Lines = new array<ref OZ_AdminReloadLine>();
    }

    OZ_AdminReloadReport Copy()
    {
        OZ_AdminReloadReport c = new OZ_AdminReloadReport();
        c.Done = Done;
        c.Problems = Problems;
        if (!Lines)
            return c;
        for (int i = 0; i < Lines.Count(); i++)
        {
            if (Lines[i])
                c.Lines.Insert(Lines[i].Copy());
        }
        return c;
    }
}

// What a module tells the overview. Server only.
class OZ_AdminFactsProvider
{
    // Fill the module's card: Id, Name, Pages, Problems (left at -1 when the module
    // does not count them) and its facts (card.Add).
    void Fill(OZ_AdminModuleCard card)
    {
    }

    // Read the module's files from disk again, as at the start. Lines for the admin
    // go to `said` (stringtable keys or words). False: nothing is read again here,
    // a restart applies the module's files.
    bool Reload(array<string> said)
    {
        return false;
    }
}

class OZ_AdminFacts
{
    private static ref array<ref OZ_AdminFactsProvider> s_All = new array<ref OZ_AdminFactsProvider>();

    static void Register(OZ_AdminFactsProvider p)
    {
        if (!p || s_All.Find(p) != -1)
            return;
        s_All.Insert(p);
    }

    static int Count()
    {
        return s_All.Count();
    }

    static OZ_AdminFactsProvider At(int i)
    {
        if (i < 0 || i >= s_All.Count())
            return null;
        return s_All[i];
    }

    // The registry lives one mission, as the core's other registries do
    // (OZ_AdminRegistry.Clear): the next mission registers its cards again.
    static void Clear()
    {
        s_All.Clear();
    }
}

// The core's own card: its zones, the config files the window edits, the kinds
// the bridge mirrors. Its settings file is read at the start only (the bridge
// and the gates run on it), so a reload reads the spawn zones and says so.
class OZ_CoreFacts : OZ_AdminFactsProvider
{
    override void Fill(OZ_AdminModuleCard card)
    {
        card.Id = "core";
        card.Name = "#STR_OZ_VPP_MOD_CORE";
        card.Pages = 1;
        int found = OZ_ConfigProblems.Of(OZ_Const.SETTINGS_TAG);
        found += OZ_ConfigProblems.Of("spawns");
        card.Problems = found;
        card.Add("#STR_OZ_FACT_ZONES", OZ_Spawns.Count().ToString());
        card.Add("#STR_OZ_FACT_CONFIGS", OZ_AdminCfg.Count().ToString());
        card.Add("#STR_OZ_FACT_MIRRORS", OZ_BridgeClient.MirrorCount().ToString());
    }

    override bool Reload(array<string> said)
    {
        OZ_Spawns.Reload(false);
        said.Insert("#STR_OZ_FACT_SETTINGS_RESTART");
        return true;
    }
}
