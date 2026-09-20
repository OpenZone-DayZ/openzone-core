// Таблиці рядків, які сервер САМ читає з архівів (PBO) при старті, щоб
// перелік класів для моста (OZ_ClassDump) ніс назви обома мовами серії:
// колонка `original` (українська в серії, англійська у ванілі) і колонка
// `english`. Рушій тримає в пам'яті лише мову сервера (`language=` у
// DayZ.cfg) і саме її повертає ConfigGetText; інші колонки є тільки в самому
// CSV. Файл stringtable.csv лежить у корені префікса кожного PBO -- ванільний
// `languagecore` і кожен мод, який має власні рядки.
//
// Звідки беруться префікси: перелік архівів у корені віртуальної файлової
// системи (FindFile з FindFileFlags.ARCHIVES, на рівень нижче для префіксів
// на кшталт DayZExpansion/Core), теки з `CfgMods` (dir і files[] модулів
// скриптів) та `languagecore`. Таблиця, якої немає, просто пропускається;
// клас, чий ключ ніде не знайдено, отримує текст мовою сервера.
//
// Виміряно 2026-09-20 на стенді: OpenFile відкриває упаковані файли модів
// і dta/languagecore; FGets повертає рядки по 8198 символів цілими; FileAttr
// -- звичайний enum (DIRECTORY = 0), тож атрибут запису не перевіряється.
// Читається раз на старт, звільняється після дампа класів.

class OZ_StrRow
{
    string Original;
    string English;
}

class OZ_StringTables
{
    private static ref map<string, ref OZ_StrRow> s_Rows;
    private static ref array<string> s_Found;
    private static int s_Tables = 0;
    private static int s_Keys = 0;
    private static int s_Candidates = 0;
    private static int s_RootEntries = 0;
    private static int s_MaxLine = 0;
    private static int s_Dropped = 0;

    static const string FILE_NAME = "stringtable.csv";
    static const int MAX_HEADER_FIELDS = 40;
    static const int MAX_JOINED_LINES = 20;
    static const int MAX_EMPTY_LINES = 1000;

    static void Load()
    {
        s_Rows = new map<string, ref OZ_StrRow>();
        s_Found = new array<string>();
        s_Tables = 0;
        s_Keys = 0;
        s_MaxLine = 0;
        s_Dropped = 0;
        array<string> dirs = new array<string>();
        Discover(dirs);
        s_Candidates = dirs.Count();
        foreach (string dir : dirs)
        {
            if (LoadTable(dir + "/" + FILE_NAME))
            {
                s_Tables++;
                s_Found.Insert(dir);
            }
        }
        string found = "";
        foreach (string f : s_Found)
        {
            if (found != "")
                found += ", ";
            found += f;
        }
        OZ_Log.Info("stringtables: " + s_Tables.ToString() + " table(s) [" + found + "], " + s_Keys.ToString() + " key(s), " + s_Dropped.ToString() + " row(s) dropped, longest line " + s_MaxLine.ToString() + ", " + s_Candidates.ToString() + " candidate dir(s), " + s_RootEntries.ToString() + " archive root entr(ies)");
    }

    static void Release()
    {
        s_Rows = null;
    }

    static int Tables()
    {
        return s_Tables;
    }

    static int Keys()
    {
        return s_Keys;
    }

    static array<string> Found()
    {
        return s_Found;
    }

    // Ключ із конфігу ($STR_..., регістр байдуже) -> дві колонки. Порожня
    // колонка береться з сусідньої; ключ, якого немає, або текст без ключа
    // (буквальна назва в конфігу) -- false, і той, хто питає, бере текст
    // мовою сервера.
    static bool Resolve(string raw, out string original, out string english)
    {
        if (!s_Rows)
            return false;
        string key = raw;
        if (key.Length() > 0 && key.Get(0) == "$")
            key = key.Substring(1, key.Length() - 1);
        key.ToLower();
        OZ_StrRow row;
        if (!s_Rows.Find(key, row))
            return false;
        original = row.Original;
        english = row.English;
        if (original == "")
            original = english;
        if (english == "")
            english = original;
        return original != "";
    }

    // ---------- пошук таблиць ----------

    private static void Discover(out array<string> dirs)
    {
        map<string, bool> seen = new map<string, bool>();
        s_RootEntries = ListArchives("", seen, dirs);
        FromCfgMods(seen, dirs);
        Add(seen, dirs, "languagecore");
    }

    // Перелік архівних записів на рівні `base` ("" = корінь). Запис без
    // своєї таблиці перевіряється ще на рівень глибше; для файла той рівень
    // порожній.
    private static int ListArchives(string base, map<string, bool> seen, out array<string> dirs)
    {
        string name;
        FileAttr attr;
        string pattern = "*";
        if (base != "")
            pattern = base + "/*";
        FindFileHandle h = FindFile(pattern, name, attr, FindFileFlags.ARCHIVES);
        if (h == 0)
            return 0;
        int n = 0;
        array<string> entries = new array<string>();
        bool more = true;
        while (more)
        {
            if (name != "" && name != "." && name != "..")
            {
                n++;
                string entry = name;
                if (base != "")
                    entry = base + "/" + name;
                entries.Insert(entry);
            }
            more = FindNextFile(h, name, attr);
        }
        CloseFindFile(h);
        foreach (string e : entries)
        {
            if (Has(e))
                Add(seen, dirs, e);
            else if (base == "")
                ListArchives(e, seen, dirs);
        }
        return n;
    }

    private static bool Has(string dir)
    {
        FileHandle f = OpenFile(dir + "/" + FILE_NAME, FileMode.READ);
        if (f == 0)
            return false;
        CloseFile(f);
        return true;
    }

    // Теки модів з CfgMods: `dir` і перші один-два сегменти кожного шляху
    // files[] модулів скриптів ("A/B/scripts/3_Game" -> "A", "A/B").
    private static void FromCfgMods(map<string, bool> seen, out array<string> dirs)
    {
        string root = "CfgMods";
        int n = GetGame().ConfigGetChildrenCount(root);
        for (int i = 0; i < n; i++)
        {
            string mod;
            if (!GetGame().ConfigGetChildName(root, i, mod))
                continue;
            string dir;
            if (GetGame().ConfigGetText(root + " " + mod + " dir", dir))
                AddPrefixes(seen, dirs, dir);
            string defs = root + " " + mod + " defs";
            int dn = GetGame().ConfigGetChildrenCount(defs);
            for (int j = 0; j < dn; j++)
            {
                string def;
                if (!GetGame().ConfigGetChildName(defs, j, def))
                    continue;
                TStringArray files = new TStringArray();
                GetGame().ConfigGetTextArray(defs + " " + def + " files", files);
                foreach (string file : files)
                    AddPrefixes(seen, dirs, file);
            }
        }
    }

    private static void AddPrefixes(map<string, bool> seen, out array<string> dirs, string path)
    {
        string p = path;
        p.Replace("\\", "/");
        while (p.Length() > 0 && p.Get(0) == "/")
            p = p.Substring(1, p.Length() - 1);
        int first = p.IndexOf("/");
        if (first < 0)
        {
            Add(seen, dirs, p);
            return;
        }
        Add(seen, dirs, p.Substring(0, first));
        int second = p.IndexOfFrom(first + 1, "/");
        if (second > first)
            Add(seen, dirs, p.Substring(0, second));
    }

    private static void Add(map<string, bool> seen, out array<string> dirs, string dir)
    {
        if (dir == "")
            return;
        string key = dir;
        key.ToLower();
        if (seen.Contains(key))
            return;
        seen.Insert(key, true);
        dirs.Insert(dir);
    }

    // ---------- читання CSV ----------

    // Один файл у s_Rows. Шапка називає колонки (original/english можуть
    // стояти не на своїх місцях); ключ -- завжди перше поле. Поле в лапках,
    // яке не закрилося на рядку, продовжується на наступному.
    private static bool LoadTable(string path)
    {
        FileHandle f = OpenFile(path, FileMode.READ);
        if (f == 0)
            return false;
        int origIdx = 1;
        int engIdx = 2;
        int want = 3;
        bool first = true;
        int empties = 0;
        int rows = 0;
        int lines = 0;
        int dropped = 0;
        string line;
        string more;
        array<string> fields = new array<string>();
        while (true)
        {
            int n = FGets(f, line);
            if (n < 0)
                break;
            if (n == 0)
            {
                empties++;
                if (empties > MAX_EMPTY_LINES)
                    break;
                continue;
            }
            empties = 0;
            lines++;
            if (n > s_MaxLine)
                s_MaxLine = n;
            int need = want;
            if (first)
                need = MAX_HEADER_FIELDS;
            fields.Clear();
            int end = Fields(line, need, fields);
            int joins = 0;
            while (end < 0 && joins < MAX_JOINED_LINES)
            {
                if (FGets(f, more) < 0)
                    break;
                lines++;
                line = line + "\n" + more;
                fields.Clear();
                end = Fields(line, need, fields);
                joins++;
            }
            if (end < 0)
            {
                dropped++;
                continue;
            }
            if (first)
            {
                first = false;
                string head = fields[0];
                head.ToLower();
                if (head.Contains("language"))
                {
                    for (int c = 1; c < fields.Count(); c++)
                    {
                        string col = fields[c];
                        col.TrimInPlace();
                        col.ToLower();
                        if (col == "original")
                            origIdx = c;
                        else if (col == "english")
                            engIdx = c;
                    }
                    want = origIdx + 1;
                    if (engIdx >= origIdx)
                        want = engIdx + 1;
                    continue;
                }
            }
            string key = fields[0];
            key.TrimInPlace();
            if (key == "")
                continue;
            key.ToLower();
            OZ_StrRow row = new OZ_StrRow();
            if (origIdx < fields.Count())
                row.Original = fields[origIdx];
            if (engIdx < fields.Count())
                row.English = fields[engIdx];
            s_Rows.Set(key, row);
            rows++;
        }
        CloseFile(f);
        s_Keys += rows;
        s_Dropped += dropped;
        OZ_Log.Dbg("stringtables: " + path + ": " + rows.ToString() + " key(s) from " + lines.ToString() + " line(s), " + dropped.ToString() + " dropped");
        return true;
    }

    // Перші `want` полів рядка CSV: поле в лапках може містити коми й
    // подвоєні лапки; поле без лапок закінчується комою. Повертає позицію
    // після останнього прочитаного поля, або -1, коли поле в лапках не
    // закрилося на цьому рядку.
    private static int Fields(string line, int want, out array<string> outFields)
    {
        int len = line.Length();
        int pos = 0;
        int q;
        int comma;
        string val;
        for (int i = 0; i < want; i++)
        {
            if (pos >= len)
            {
                outFields.Insert("");
                continue;
            }
            if (line.Get(pos) == "\"")
            {
                pos++;
                val = "";
                while (true)
                {
                    q = line.IndexOfFrom(pos, "\"");
                    if (q < 0)
                        return -1;
                    val = val + line.Substring(pos, q - pos);
                    if (q + 1 < len && line.Get(q + 1) == "\"")
                    {
                        val = val + "\"";
                        pos = q + 2;
                        continue;
                    }
                    pos = q + 1;
                    break;
                }
                outFields.Insert(val);
                comma = line.IndexOfFrom(pos, ",");
                if (comma < 0)
                    pos = len;
                else
                    pos = comma + 1;
            }
            else
            {
                comma = line.IndexOfFrom(pos, ",");
                if (comma < 0)
                {
                    outFields.Insert(line.Substring(pos, len - pos));
                    pos = len;
                }
                else
                {
                    outFields.Insert(line.Substring(pos, comma - pos));
                    pos = comma + 1;
                }
            }
        }
        return pos;
    }
}
