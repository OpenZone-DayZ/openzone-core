// Перелік класів сервера для моста й будь-якого редактора на ньому: усі
// п'ять коренів, які перевіряють моди серії (класи предметів, магазинів,
// нерухомих об'єктів, боєприпасів, зброї), по рядку на клас -- корінь, ім'я,
// базовий клас і назва двома мовами. Пишеться раз на старт сервера у
// $profile:OpenZone\classes.tsv; міст читає файл при першому опитуванні
// сервера й будує з нього індекс, а сайт перевіряє «клас існує», родину
// (IsKindOf) і показує ігрові назви проти ЖИВОГО сервера, без розбору PBO
// поза грою (рішення власника 2026-09-20: «вивантажуй усе з сервера», і
// служба ця -- ядра, бо потрібна кожному моду з полем «клас»).
//
// Формат рядка: <корінь>\t<ім'я>\t<база>\t<original>\t<english>\t<ширина>\t
// <висота>\t<ширина вантажу>\t<висота вантажу> (itemSize та itemsCargoSize у
// клітинках, 0 коли їх немає). Порядок коренів збігається з індексом класів
// моста (0..4). Назви: ключ
// displayName розв'язується таблицями рядків, які сервер читає з архівів
// (OZ_StringTables); ключ, якого там немає, і буквальна назва без ключа
// дають текст мовою сервера в обидві колонки. Табуляції й переноси в назві
// замінюються пробілом. Виміряно 2026-09-20: 12 937 класів, 6 таблиць,
// 0,8-1,8 с старту.

class OZ_ClassDump
{
    private static int s_LastCount = 0;
    private static int s_Resolved = 0;
    private static int s_Ms = 0;

    // Порядок = індекс кореня в мостовому індексі класів.
    static const string ROOT_0 = "CfgVehicles";
    static const string ROOT_1 = "CfgMagazines";
    static const string ROOT_2 = "CfgNonAIVehicles";
    static const string ROOT_3 = "CfgAmmo";
    static const string ROOT_4 = "cfgWeapons";

    static int Write(string path)
    {
        int t0 = GetGame().GetTime();
        OZ_StringTables.Load();
        FileHandle f = OpenFile(path, FileMode.WRITE);
        if (f == 0)
        {
            OZ_Log.Warn("classes: cannot write " + path);
            OZ_StringTables.Release();
            return 0;
        }
        s_Resolved = 0;
        int written = 0;
        written += WriteRoot(f, 0, ROOT_0);
        written += WriteRoot(f, 1, ROOT_1);
        written += WriteRoot(f, 2, ROOT_2);
        written += WriteRoot(f, 3, ROOT_3);
        written += WriteRoot(f, 4, ROOT_4);
        CloseFile(f);
        OZ_StringTables.Release();
        s_LastCount = written;
        s_Ms = GetGame().GetTime() - t0;
        return written;
    }

    private static int WriteRoot(FileHandle f, int rootIdx, string root)
    {
        int n = GetGame().ConfigGetChildrenCount(root);
        int written = 0;
        string tab = "\t";
        string prefix = rootIdx.ToString() + tab;
        for (int i = 0; i < n; i++)
        {
            string name;
            if (!GetGame().ConfigGetChildName(root, i, name))
                continue;
            if (name == "")
                continue;
            string path = root + " " + name;
            string base = "";
            GetGame().ConfigGetBaseName(path, base);
            string raw = "";
            GetGame().ConfigGetTextRaw(path + " displayName", raw);
            string original;
            string english;
            if (OZ_StringTables.Resolve(raw, original, english))
            {
                s_Resolved++;
            }
            else
            {
                original = GetGame().ConfigGetTextOut(path + " displayName");
                english = original;
            }
            // Розмір предмета в клітинках і розмір його вантажу (для ящиків):
            // з них міст рахує зайняте місце, не питаючи гру.
            int w = 0;
            int h = 0;
            TIntArray size = new TIntArray();
            GetGame().ConfigGetIntArray(path + " itemSize", size);
            if (size.Count() >= 2)
            {
                w = size[0];
                h = size[1];
            }
            // Ваніль тримає itemsCargoSize просто в класі, ящики серії (і
            // частина модів) -- у підкласі Cargo.
            int cw = 0;
            int ch = 0;
            TIntArray cargo = new TIntArray();
            GetGame().ConfigGetIntArray(path + " itemsCargoSize", cargo);
            if (cargo.Count() < 2)
                GetGame().ConfigGetIntArray(path + " Cargo itemsCargoSize", cargo);
            if (cargo.Count() >= 2)
            {
                cw = cargo[0];
                ch = cargo[1];
            }
            FPrintln(f, prefix + name + tab + base + tab + Clean(original) + tab + Clean(english) + tab + w.ToString() + tab + h.ToString() + tab + cw.ToString() + tab + ch.ToString());
            written++;
        }
        return written;
    }

    private static string Clean(string s)
    {
        string cleaned = s;
        cleaned.Replace("\t", " ");
        cleaned.Replace("\r", "");
        cleaned.Replace("\n", " ");
        return cleaned;
    }

    static int LastCount()
    {
        return s_LastCount;
    }

    // Один рядок для журналу: скільки класів, скільки назв розв'язано
    // таблицями, скільки таблиць і ключів, скільки часу.
    static string Summary()
    {
        return s_LastCount.ToString() + " written, " + s_Resolved.ToString() + " named from " + OZ_StringTables.Tables().ToString() + " stringtable(s) with " + OZ_StringTables.Keys().ToString() + " key(s), " + s_Ms.ToString() + " ms";
    }
}
