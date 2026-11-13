// The classes of the game for a search typed into a field: the class fields of the OpenZone
// window in VPP (design docs/specs/2026-11-13-openzone-vpp-class-search-design.md). Built on
// the client from its own config -- it has the server's mods -- a slice at a time, once a game
// session, and kept: a search then costs no server work and no traffic.
//
// Three stages. The classes: CfgVehicles, CfgWeapons and CfgMagazines, every class of scope 1
// or more, its kind, its name as the game shows it (ConfigGetText: the game's language) and its
// name's key (ConfigGetTextRaw). Then the string tables (OZ_StringTables in steps: their
// discovery took 2.1 s on the stand's server, 2026-11-13). Then each name's two columns, the
// series' Ukrainian and the English, joined to what a search looks through. A search works
// from the first class on and finds more as the stages go.
//
// Kinds: a class of CfgVehicles is a vehicle when Transport is among its parents, an item when
// Inventory_Base is, else an object of the world (a building, a character, a prop). Classes of
// CfgWeapons and CfgMagazines are items. Items and vehicles need scope 2, what can exist in the
// game; objects take scope 1 too (the map's buildings).

class OZ_ClassIndex
{
    static const int ITEM = 1;
    static const int OBJECT = 2;
    static const int VEHICLE = 4;
    static const int ANY = 7;

    protected static const int STAGE_NONE = 0;
    protected static const int STAGE_CLASSES = 1;
    protected static const int STAGE_TABLES = 2;
    protected static const int STAGE_NAMES = 3;
    protected static const int STAGE_READY = 4;

    // Classes looked at, and names joined, between two looks at the clock.
    protected static const int CLASSES_PER_LOOK = 16;
    protected static const int NAMES_PER_LOOK = 64;

    protected static int s_Stage = 0;
    protected static ref array<string> s_Roots;
    protected static int s_Root = 0;
    protected static int s_At = 0;
    protected static int s_Of = 0;
    protected static int s_Walked = 0;
    protected static int s_ToWalk = 0;
    protected static int s_Named = 0;

    // A class each: its name, its name as the game shows it, that name's key, its kind, and
    // what a search looks through (all of it small, one line each: the class first).
    protected static ref array<string> s_Class;
    protected static ref array<string> s_Shown;
    protected static ref array<string> s_Key;
    protected static ref array<int> s_Kind;
    protected static ref array<string> s_Hay;

    // Moves on when a search could find something it did not before.
    protected static int s_Rev = 0;
    protected static int s_Ticks = 0;
    protected static int s_Frames = 0;

    // The last search: its text, its kinds, and every class it found.
    protected static string s_LastQ = "";
    protected static int s_LastKinds = 0;
    protected static int s_LastRev = -1;
    protected static ref array<int> s_LastAll;

    // Work until the budget (TickCount ticks) is spent; nothing once the index is whole.
    static void Advance(int budget)
    {
        if (s_Stage == STAGE_READY)
            return;
        if (s_Stage == STAGE_NONE)
            BeginWalk();
        int t0 = TickCount(0);
        s_Frames++;
        while (s_Stage != STAGE_READY)
        {
            if (s_Stage == STAGE_CLASSES)
            {
                WalkSome();
            }
            else if (s_Stage == STAGE_TABLES)
            {
                int left = budget - TickCount(t0);
                if (left < 1)
                    left = 1;
                if (OZ_StringTables.Advance(left))
                    s_Stage = STAGE_NAMES;
            }
            else
            {
                NameSome();
            }
            if (TickCount(t0) >= budget)
                break;
        }
        s_Ticks += TickCount(t0);
    }

    // True once every class is in: the names of the tables may still be joining.
    static bool Walked()
    {
        return s_Stage > STAGE_CLASSES;
    }

    static bool Ready()
    {
        return s_Stage == STAGE_READY;
    }

    // How much of the classes is in, 0..100.
    static int Percent()
    {
        if (s_Stage > STAGE_CLASSES)
            return 100;
        if (s_ToWalk <= 0)
            return 0;
        return s_Walked * 100 / s_ToWalk;
    }

    static int Revision()
    {
        return s_Rev;
    }

    static int Count()
    {
        if (!s_Class)
            return 0;
        return s_Class.Count();
    }

    static string ClassAt(int i)
    {
        return s_Class[i];
    }

    // The name to show: the game's, else the class's own. A name the tables lack in the game's
    // language comes with the engine's $UNT$ before it.
    static string NameAt(int i)
    {
        string s = s_Shown[i];
        if (s.IndexOf("$UNT$") == 0)
            s = s.Substring(5, s.Length() - 5);
        if (s == "")
            s = s_Class[i];
        return s;
    }

    static int KindAt(int i)
    {
        return s_Kind[i];
    }

    // Up to `limit` classes of the kinds (a sum of ITEM, OBJECT, VEHICLE) whose class or names
    // hold the text, best first: the class begins with it, then a name begins with it, then a
    // word begins with it, then the rest, each alphabetical by class. `total` is how many hold
    // it in all. Fewer than two letters find nothing. A text that only grew since the last
    // search looks only through what that search found.
    static void Find(string text, int kinds, int limit, out array<int> found, out int total)
    {
        found = new array<int>();
        total = 0;
        if (!s_Hay)
            return;
        string q = text.Trim();
        if (q.LengthUtf8() < 2)
            return;
        q = OZ_Fold.Lower(q);

        array<int> pool = null;
        if (s_LastAll && s_LastRev == s_Rev && s_LastKinds == kinds)
        {
            if (s_LastQ != "" && q.IndexOf(s_LastQ) == 0)
                pool = s_LastAll;
        }

        array<int> hits = new array<int>();
        array<string> first = new array<string>();
        array<string> named = new array<string>();
        array<string> word = new array<string>();
        array<string> rest = new array<string>();
        string nameStart = "\n" + q;
        int n = s_Hay.Count();
        if (pool)
            n = pool.Count();
        for (int k = 0; k < n; k++)
        {
            int i = k;
            if (pool)
                i = pool[k];
            if ((s_Kind[i] & kinds) == 0)
                continue;
            string hay = s_Hay[i];
            int at = hay.IndexOf(q);
            if (at < 0)
                continue;
            hits.Insert(i);
            string order = SortKey(hay, i);
            if (at == 0)
                first.Insert(order);
            else if (hay.IndexOf(nameStart) >= 0)
                named.Insert(order);
            else if (AtWordStart(hay, q))
                word.Insert(order);
            else
                rest.Insert(order);
        }

        total = hits.Count();
        s_LastQ = q;
        s_LastKinds = kinds;
        s_LastRev = s_Rev;
        s_LastAll = hits;

        first.Sort();
        named.Sort();
        word.Sort();
        rest.Sort();
        CollectKeys(first, limit, found);
        CollectKeys(named, limit, found);
        CollectKeys(word, limit, found);
        CollectKeys(rest, limit, found);
    }

    // ---------- building ----------

    protected static void BeginWalk()
    {
        s_Roots = {"CfgVehicles", "CfgWeapons", "CfgMagazines"};
        s_Class = new array<string>();
        s_Shown = new array<string>();
        s_Key = new array<string>();
        s_Kind = new array<int>();
        s_Hay = new array<string>();
        s_ToWalk = 0;
        for (int r = 0; r < s_Roots.Count(); r++)
            s_ToWalk += GetGame().ConfigGetChildrenCount(s_Roots[r]);
        s_Root = 0;
        s_At = 0;
        s_Of = GetGame().ConfigGetChildrenCount(s_Roots[0]);
        s_Walked = 0;
        s_Named = 0;
        s_Ticks = 0;
        s_Frames = 0;
        s_Stage = STAGE_CLASSES;
        s_Rev++;
    }

    protected static void WalkSome()
    {
        for (int k = 0; k < CLASSES_PER_LOOK; k++)
        {
            if (s_Root >= s_Roots.Count())
            {
                s_Stage = STAGE_TABLES;
                s_Rev++;
                OZ_StringTables.BeginLoad();
                return;
            }
            if (s_At >= s_Of)
            {
                s_Root++;
                s_At = 0;
                s_Of = 0;
                if (s_Root < s_Roots.Count())
                    s_Of = GetGame().ConfigGetChildrenCount(s_Roots[s_Root]);
                continue;
            }
            int at = s_At;
            s_At++;
            s_Walked++;
            TakeClass(s_Roots[s_Root], at);
        }
    }

    protected static void TakeClass(string root, int at)
    {
        string name;
        if (!GetGame().ConfigGetChildName(root, at, name))
            return;
        string path = root + " " + name;
        int scope = GetGame().ConfigGetInt(path + " scope");
        if (scope < 1)
            return;
        int kind = ITEM;
        if (root == "CfgVehicles")
            kind = KindOf(path);
        if (kind != OBJECT && scope < 2)
            return;
        string shown;
        GetGame().ConfigGetText(path + " displayName", shown);
        string key;
        GetGame().ConfigGetTextRaw(path + " displayName", key);
        string hay = OZ_Fold.Lower(name);
        if (shown != "")
            hay = hay + "\n" + OZ_Fold.Lower(shown);
        s_Class.Insert(name);
        s_Shown.Insert(shown);
        s_Key.Insert(key);
        s_Kind.Insert(kind);
        s_Hay.Insert(hay);
        // A list open while classes come in asks again now and then (OZ_ClassPick), not at
        // every class.
        int count = s_Class.Count();
        if (count % 500 == 0)
            s_Rev++;
    }

    // What a class of CfgVehicles is, by the classes it comes from.
    protected static int KindOf(string path)
    {
        TStringArray line = new TStringArray();
        GetGame().ConfigGetFullPath(path, line);
        int n = line.Count();
        for (int i = 0; i < n; i++)
        {
            string c = line[i];
            c.ToLower();
            if (c == "transport")
                return VEHICLE;
            if (c == "inventory_base")
                return ITEM;
        }
        return OBJECT;
    }

    protected static void NameSome()
    {
        for (int k = 0; k < NAMES_PER_LOOK; k++)
        {
            if (s_Named >= s_Class.Count())
            {
                OZ_StringTables.Release();
                s_Stage = STAGE_READY;
                s_Rev++;
                SayBuilt();
                return;
            }
            int i = s_Named;
            s_Named++;
            string key = s_Key[i];
            if (key.Length() < 2 || key.Get(0) != "$")
                continue;
            string original;
            string english;
            if (!OZ_StringTables.Resolve(key, original, english))
                continue;
            string shown = s_Shown[i];
            string hay = s_Hay[i];
            bool grew = false;
            if (original != "" && original != shown)
            {
                hay = hay + "\n" + OZ_Fold.Lower(original);
                grew = true;
            }
            if (english != "" && english != shown && english != original)
            {
                hay = hay + "\n" + OZ_Fold.Lower(english);
                grew = true;
            }
            if (grew)
                s_Hay.Set(i, hay);
        }
    }

    protected static void SayBuilt()
    {
        string said = "class index: " + s_Class.Count().ToString() + " classes of " + s_ToWalk.ToString();
        said = said + ", names from " + OZ_StringTables.Tables().ToString() + " string table(s)";
        int ms = s_Ticks / 10000;
        said = said + ", " + ms.ToString() + " ms of work over " + s_Frames.ToString() + " frame(s)";
        OZ_Log.Info(said);
    }

    // ---------- searching ----------

    // What a match sorts by: its class, small (the first line of what a search looks
    // through), then its place.
    protected static string SortKey(string hay, int i)
    {
        string cls = hay;
        int nl = hay.IndexOf("\n");
        if (nl >= 0)
            cls = hay.Substring(0, nl);
        return cls + "\t" + i.ToString();
    }

    protected static bool AtWordStart(string hay, string q)
    {
        if (hay.IndexOf(" " + q) >= 0)
            return true;
        if (hay.IndexOf("_" + q) >= 0)
            return true;
        if (hay.IndexOf("-" + q) >= 0)
            return true;
        return hay.IndexOf("(" + q) >= 0;
    }

    protected static void CollectKeys(array<string> keys, int limit, array<int> found)
    {
        for (int k = 0; k < keys.Count(); k++)
        {
            if (found.Count() >= limit)
                return;
            string key = keys[k];
            int tab = key.IndexOf("\t");
            if (tab < 0)
                continue;
            string place = key.Substring(tab + 1, key.Length() - tab - 1);
            found.Insert(place.ToInt());
        }
    }
}
