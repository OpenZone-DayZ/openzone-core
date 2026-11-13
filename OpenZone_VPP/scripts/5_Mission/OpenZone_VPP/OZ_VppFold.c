// Small letters for a search that survives Cyrillic. The engine's string.ToLower turns every
// byte outside ASCII into a space (measured 2026-09-16 on a stand mod's search): a Cyrillic
// query lowered that way matches any name with as many letters. Folding goes through a table
// instead: ASCII A..Z and the Cyrillic capitals a player's name may hold, in place.
// The storage mod's OZS_Case and the factions page's OZF_VppFold do the same; this is the
// core window's copy, for the page search, Гравці and Точки появи.

class OZ_VppFold
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
        Build();
        string r = s;
        int n = s_Upper.Count();
        for (int i = 0; i < n; i++)
            r.Replace(s_Upper[i], s_Lower[i]);
        return r;
    }
}
