// Small letters for a search that survives Cyrillic. The engine's string.ToLower turns every
// byte outside ASCII into a space (measured 2026-09-16 on a stand mod's search), so a text with
// such bytes is folded through a table: ASCII A..Z and the Cyrillic capitals, in place. A text
// of ASCII alone (a class name) takes ToLower, which costs one call instead of sixty.
// The window's OZ_VppFold, the storage mod's OZS_Case and the factions page's OZF_VppFold fold
// the same way; this one is in 3_Game for what is not a page (OZ_ClassIndex).

class OZ_Fold
{
    protected static ref array<string> s_Upper;
    protected static ref array<string> s_Lower;

    protected static void Build()
    {
        if (s_Upper)
            return;
        s_Upper = {"A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M", "N", "O", "P", "Q", "R", "S", "T", "U", "V", "W", "X", "Y", "Z",
                   "А", "Б", "В", "Г", "Ґ", "Д", "Е", "Є", "Ё", "Ж", "З", "И", "І", "Ї", "Й", "К", "Л", "М", "Н", "О", "П", "Р", "С", "Т", "У", "Ф", "Х", "Ц", "Ч", "Ш", "Щ", "Ъ", "Ы", "Ь", "Э", "Ю", "Я"};
        s_Lower = {"a", "b", "c", "d", "e", "f", "g", "h", "i", "j", "k", "l", "m", "n", "o", "p", "q", "r", "s", "t", "u", "v", "w", "x", "y", "z",
                   "а", "б", "в", "г", "ґ", "д", "е", "є", "ё", "ж", "з", "и", "і", "ї", "й", "к", "л", "м", "н", "о", "п", "р", "с", "т", "у", "ф", "х", "ц", "ч", "ш", "щ", "ъ", "ы", "ь", "э", "ю", "я"};
    }

    // The text with every capital it knows made small; everything else as it was.
    static string Lower(string s)
    {
        string r = s;
        // LengthUtf8 counts characters, Length bytes: equal only for ASCII.
        if (s.LengthUtf8() == s.Length())
        {
            r.ToLower();
            return r;
        }
        Build();
        int n = s_Upper.Count();
        for (int i = 0; i < n; i++)
            r.Replace(s_Upper[i], s_Lower[i]);
        return r;
    }
}
