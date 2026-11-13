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
// Сервер читає їх раз на старт і звільняє після дампа класів; клієнт -- кроками, для пошуку
// класів у вікні OpenZone (OZ_ClassIndex), і теж звільняє, коли назви розібрано.

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

    // Читання кроками (2026-11-13): клієнт будує перелік класів для пошуку у вікні OpenZone
    // (OZ_ClassIndex) і не має права стати на кадр. Виміряно на стенді (15 таблиць, 10 118
    // ключів): пошук таблиць по архівах 2,1 с, читання 0,32 с. Begin, потім Step з бюджетом у
    // тіках TickCount, доки не відповість true; Load() -- ті самі кроки без бюджету, для старту
    // сервера (OZ_ClassDump). Порядок тек і таблиць той самий, що був у Load(): пізніша
    // таблиця перекриває ключ ранішої.
    private static const int STAGE_IDLE = 0;
    private static const int STAGE_ROOT = 1;
    private static const int STAGE_ENTRIES = 2;
    private static const int STAGE_CFGMODS = 3;
    private static const int STAGE_TABLES = 4;
    private static const int STAGE_DONE = 5;
    // Скільки рядків читається між двома поглядами на годинник: рядок коштує мікросекунди.
    private static const int LINES_PER_LOOK = 64;

    private static int s_Stage = 0;
    private static ref array<string> s_Entries;
    private static int s_EntryAt = 0;
    private static ref map<string, bool> s_Seen;
    private static ref array<string> s_Dirs;
    private static int s_DirAt = 0;

    // Таблиця, що читається: файл і де стоїть її читання.
    private static FileHandle s_File;
    private static bool s_Open = false;
    private static string s_Dir;
    private static string s_Path;
    private static int s_OrigIdx = 1;
    private static int s_EngIdx = 2;
    private static int s_Want = 3;
    private static bool s_First = true;
    private static int s_Empties = 0;
    private static int s_TblRows = 0;
    private static int s_TblLines = 0;
    private static int s_TblDropped = 0;

    static void Load()
    {
        Begin();
        bool done = false;
        while (!done)
            done = Step(2000000000);
    }

    // Нове читання: усе, що було прочитано, забувається.
    static void Begin()
    {
        Abort();
        s_Rows = new map<string, ref OZ_StrRow>();
        s_Found = new array<string>();
        s_Tables = 0;
        s_Keys = 0;
        s_Candidates = 0;
        s_RootEntries = 0;
        s_MaxLine = 0;
        s_Dropped = 0;
        s_Entries = new array<string>();
        s_EntryAt = 0;
        s_Seen = new map<string, bool>();
        s_Dirs = new array<string>();
        s_DirAt = 0;
        s_Stage = STAGE_ROOT;
    }

    // Читання, обірване на півдорозі (кінець місії): відкрита таблиця закривається.
    static void Abort()
    {
        if (s_Open)
        {
            CloseFile(s_File);
            s_Open = false;
        }
        s_Stage = STAGE_IDLE;
    }

    static bool Done()
    {
        return s_Stage == STAGE_DONE;
    }

    // Робота, доки не вийде бюджет (тіки TickCount); true -- усе прочитано. Одиниця роботи --
    // один запис кореня архівів, одна тека з CfgMods разом, LINES_PER_LOOK рядків таблиці.
    static bool Step(int budget)
    {
        if (s_Stage == STAGE_IDLE || s_Stage == STAGE_DONE)
            return s_Stage == STAGE_DONE;
        int t0 = TickCount(0);
        while (s_Stage != STAGE_DONE)
        {
            if (s_Stage == STAGE_ROOT)
            {
                array<string> roots = new array<string>();
                s_RootEntries = ListEntries("", roots);
                s_Entries = roots;
                s_Stage = STAGE_ENTRIES;
            }
            else if (s_Stage == STAGE_ENTRIES)
            {
                if (s_EntryAt >= s_Entries.Count())
                {
                    s_Stage = STAGE_CFGMODS;
                }
                else
                {
                    string entry = s_Entries[s_EntryAt];
                    s_EntryAt++;
                    // Запис без своєї таблиці перевіряється ще на рівень глибше.
                    if (Has(entry))
                        Add(s_Seen, s_Dirs, entry);
                    else
                        ListArchives(entry, s_Seen, s_Dirs);
                }
            }
            else if (s_Stage == STAGE_CFGMODS)
            {
                FromCfgMods(s_Seen, s_Dirs);
                Add(s_Seen, s_Dirs, "languagecore");
                s_Candidates = s_Dirs.Count();
                s_Stage = STAGE_TABLES;
            }
            else if (!s_Open)
            {
                if (s_DirAt >= s_Dirs.Count())
                {
                    Finish();
                    s_Stage = STAGE_DONE;
                }
                else
                {
                    string dir = s_Dirs[s_DirAt];
                    s_DirAt++;
                    OpenTable(dir);
                }
            }
            else
            {
                ReadLines();
            }
            if (TickCount(t0) >= budget)
                break;
        }
        return s_Stage == STAGE_DONE;
    }

    private static void Finish()
    {
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

    // Записи архівів на рівні `base` ("" = корінь), у порядку, в якому їх дає рушій.
    private static int ListEntries(string base, out array<string> entries)
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
        return n;
    }

    // Запис кореня без своєї таблиці: його власні записи, на рівень нижче.
    private static void ListArchives(string base, map<string, bool> seen, out array<string> dirs)
    {
        array<string> entries = new array<string>();
        ListEntries(base, entries);
        foreach (string e : entries)
        {
            if (Has(e))
                Add(seen, dirs, e);
        }
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

    // Таблиця теки починає читатися; теки без таблиці просто немає (файл не відкрився).
    private static void OpenTable(string dir)
    {
        s_Path = dir + "/" + FILE_NAME;
        FileHandle f = OpenFile(s_Path, FileMode.READ);
        if (f == 0)
            return;
        s_File = f;
        s_Open = true;
        s_Dir = dir;
        s_OrigIdx = 1;
        s_EngIdx = 2;
        s_Want = 3;
        s_First = true;
        s_Empties = 0;
        s_TblRows = 0;
        s_TblLines = 0;
        s_TblDropped = 0;
    }

    // До LINES_PER_LOOK рядків відкритої таблиці в s_Rows; кінець файлу закриває таблицю.
    // Шапка називає колонки (original/english можуть стояти не на своїх місцях); ключ --
    // завжди перше поле. Поле в лапках, яке не закрилося на рядку, продовжується на
    // наступному.
    private static void ReadLines()
    {
        string line;
        string more;
        array<string> fields = new array<string>();
        bool ended = false;
        for (int k = 0; k < LINES_PER_LOOK; k++)
        {
            int n = FGets(s_File, line);
            if (n < 0)
            {
                ended = true;
                break;
            }
            if (n == 0)
            {
                s_Empties++;
                if (s_Empties > MAX_EMPTY_LINES)
                {
                    ended = true;
                    break;
                }
                continue;
            }
            s_Empties = 0;
            s_TblLines++;
            if (n > s_MaxLine)
                s_MaxLine = n;
            int need = s_Want;
            if (s_First)
                need = MAX_HEADER_FIELDS;
            fields.Clear();
            int end = Fields(line, need, fields);
            int joins = 0;
            while (end < 0 && joins < MAX_JOINED_LINES)
            {
                if (FGets(s_File, more) < 0)
                    break;
                s_TblLines++;
                line = line + "\n" + more;
                fields.Clear();
                end = Fields(line, need, fields);
                joins++;
            }
            if (end < 0)
            {
                s_TblDropped++;
                continue;
            }
            if (s_First)
            {
                s_First = false;
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
                            s_OrigIdx = c;
                        else if (col == "english")
                            s_EngIdx = c;
                    }
                    s_Want = s_OrigIdx + 1;
                    if (s_EngIdx >= s_OrigIdx)
                        s_Want = s_EngIdx + 1;
                    continue;
                }
            }
            string key = fields[0];
            key.TrimInPlace();
            if (key == "")
                continue;
            key.ToLower();
            OZ_StrRow row = new OZ_StrRow();
            if (s_OrigIdx < fields.Count())
                row.Original = fields[s_OrigIdx];
            if (s_EngIdx < fields.Count())
                row.English = fields[s_EngIdx];
            s_Rows.Set(key, row);
            s_TblRows++;
        }
        if (!ended)
            return;
        CloseFile(s_File);
        s_Open = false;
        s_Tables++;
        s_Found.Insert(s_Dir);
        s_Keys += s_TblRows;
        s_Dropped += s_TblDropped;
        OZ_Log.Dbg("stringtables: " + s_Path + ": " + s_TblRows.ToString() + " key(s) from " + s_TblLines.ToString() + " line(s), " + s_TblDropped.ToString() + " dropped");
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
