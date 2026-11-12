// The system clipboard for the window's files page (VPP rework 2026-11-12, measured on
// the stand the same night). The engine moves the clipboard as the system's ANSI text:
// CopyFromClipboard returned a Ukrainian file pasted from an editor as cp1251 bytes, one
// a letter, and the edit box given them as UTF-8 dropped everything from the first one;
// CopyToClipboard put UTF-8 bytes there, which another program read as cp1251 mojibake.
// So text going out is recoded to cp1251 and text coming in is recoded from it unless it
// already reads as UTF-8. Characters cp1251 lacks leave as "?". A Windows whose ANSI
// code page is not cp1251 cannot carry Cyrillic this way at all.
//
// The table is code points, not letters: the source stays in the series' two languages.
// Before anything is recoded, Works() checks that the engine reads and makes bytes over
// 0x7F the way this class needs; when it does not, text passes through untouched.

#ifdef AVPPAdminTools
#ifndef NO_GUI

class OZ_VppClip
{
    protected static ref array<int> s_High;
    protected static ref map<int, int> s_Back;
    protected static int s_Works = -1;

    static string Read()
    {
        string raw;
        GetGame().CopyFromClipboard(raw);
        return FromSystem(raw);
    }

    static void Write(string text)
    {
        string outward = ToSystem(text);
        GetGame().CopyToClipboard(outward);
    }

    // Unicode code points of the bytes 0x80..0xFF of cp1251; 0 where it has none.
    protected static void Table()
    {
        if (s_High)
            return;
        array<int> h = new array<int>();
        h.Insert(0x0402); h.Insert(0x0403); h.Insert(0x201A); h.Insert(0x0453); h.Insert(0x201E); h.Insert(0x2026); h.Insert(0x2020); h.Insert(0x2021);
        h.Insert(0x20AC); h.Insert(0x2030); h.Insert(0x0409); h.Insert(0x2039); h.Insert(0x040A); h.Insert(0x040C); h.Insert(0x040B); h.Insert(0x040F);
        h.Insert(0x0452); h.Insert(0x2018); h.Insert(0x2019); h.Insert(0x201C); h.Insert(0x201D); h.Insert(0x2022); h.Insert(0x2013); h.Insert(0x2014);
        h.Insert(0x0000); h.Insert(0x2122); h.Insert(0x0459); h.Insert(0x203A); h.Insert(0x045A); h.Insert(0x045C); h.Insert(0x045B); h.Insert(0x045F);
        h.Insert(0x00A0); h.Insert(0x040E); h.Insert(0x045E); h.Insert(0x0408); h.Insert(0x00A4); h.Insert(0x0490); h.Insert(0x00A6); h.Insert(0x00A7);
        h.Insert(0x0401); h.Insert(0x00A9); h.Insert(0x0404); h.Insert(0x00AB); h.Insert(0x00AC); h.Insert(0x00AD); h.Insert(0x00AE); h.Insert(0x0407);
        h.Insert(0x00B0); h.Insert(0x00B1); h.Insert(0x0406); h.Insert(0x0456); h.Insert(0x0491); h.Insert(0x00B5); h.Insert(0x00B6); h.Insert(0x00B7);
        h.Insert(0x0451); h.Insert(0x2116); h.Insert(0x0454); h.Insert(0x00BB); h.Insert(0x0458); h.Insert(0x0405); h.Insert(0x0455); h.Insert(0x0457);
        h.Insert(0x0410); h.Insert(0x0411); h.Insert(0x0412); h.Insert(0x0413); h.Insert(0x0414); h.Insert(0x0415); h.Insert(0x0416); h.Insert(0x0417);
        h.Insert(0x0418); h.Insert(0x0419); h.Insert(0x041A); h.Insert(0x041B); h.Insert(0x041C); h.Insert(0x041D); h.Insert(0x041E); h.Insert(0x041F);
        h.Insert(0x0420); h.Insert(0x0421); h.Insert(0x0422); h.Insert(0x0423); h.Insert(0x0424); h.Insert(0x0425); h.Insert(0x0426); h.Insert(0x0427);
        h.Insert(0x0428); h.Insert(0x0429); h.Insert(0x042A); h.Insert(0x042B); h.Insert(0x042C); h.Insert(0x042D); h.Insert(0x042E); h.Insert(0x042F);
        h.Insert(0x0430); h.Insert(0x0431); h.Insert(0x0432); h.Insert(0x0433); h.Insert(0x0434); h.Insert(0x0435); h.Insert(0x0436); h.Insert(0x0437);
        h.Insert(0x0438); h.Insert(0x0439); h.Insert(0x043A); h.Insert(0x043B); h.Insert(0x043C); h.Insert(0x043D); h.Insert(0x043E); h.Insert(0x043F);
        h.Insert(0x0440); h.Insert(0x0441); h.Insert(0x0442); h.Insert(0x0443); h.Insert(0x0444); h.Insert(0x0445); h.Insert(0x0446); h.Insert(0x0447);
        h.Insert(0x0448); h.Insert(0x0449); h.Insert(0x044A); h.Insert(0x044B); h.Insert(0x044C); h.Insert(0x044D); h.Insert(0x044E); h.Insert(0x044F);
        s_High = h;
        s_Back = new map<int, int>();
        for (int i = 0; i < s_High.Count(); i++)
        {
            if (s_High[i] != 0)
                s_Back.Set(s_High[i], 0x80 + i);
        }
    }

    protected static int ByteAt(string s, int i)
    {
        string one = s.Substring(i, 1);
        int b = one.ToAscii();
        if (b < 0)
            b += 256;
        return b;
    }

    protected static string Byte(int b)
    {
        return b.AsciiToString();
    }

    // A Ukrainian letter checks both ways: read as its two UTF-8 bytes, and made again
    // from them.
    protected static bool Works()
    {
        if (s_Works != -1)
            return s_Works == 1;
        s_Works = 0;
        string probe = "Ї";
        if (probe.Length() != 2)
            return false;
        int b0 = ByteAt(probe, 0);
        int b1 = ByteAt(probe, 1);
        if (b0 != 0xD0 || b1 != 0x87)
            return false;
        string made = Byte(0xD0);
        made = made + Byte(0x87);
        if (made != probe)
            return false;
        s_Works = 1;
        return true;
    }

    static bool Utf8Valid(string s)
    {
        int n = s.Length();
        int i = 0;
        while (i < n)
        {
            int b = ByteAt(s, i);
            int follow = -1;
            if (b < 0x80)
                follow = 0;
            else if (b >= 0xC2 && b <= 0xDF)
                follow = 1;
            else if (b >= 0xE0 && b <= 0xEF)
                follow = 2;
            else if (b >= 0xF0 && b <= 0xF4)
                follow = 3;
            if (follow < 0 || i + follow >= n)
                return false;
            for (int k = 1; k <= follow; k++)
            {
                int c = ByteAt(s, i + k);
                if (c < 0x80 || c > 0xBF)
                    return false;
            }
            i += follow + 1;
        }
        return true;
    }

    // cp1251 -> UTF-8, unless the text reads as UTF-8 already.
    static string FromSystem(string raw)
    {
        if (raw == "" || !Works() || Utf8Valid(raw))
            return raw;
        Table();
        array<string> parts = new array<string>();
        int n = raw.Length();
        int start = 0;
        for (int i = 0; i < n; i++)
        {
            int b = ByteAt(raw, i);
            if (b < 0x80)
                continue;
            if (i > start)
                parts.Insert(raw.Substring(start, i - start));
            int cp = s_High[b - 0x80];
            if (cp == 0)
                parts.Insert("?");
            else
                parts.Insert(Encode(cp));
            start = i + 1;
        }
        if (start < n)
            parts.Insert(raw.Substring(start, n - start));
        return Join(parts);
    }

    // UTF-8 -> cp1251; what cp1251 lacks becomes "?".
    static string ToSystem(string text)
    {
        if (text == "" || !Works())
            return text;
        Table();
        array<string> parts = new array<string>();
        int n = text.Length();
        int start = 0;
        int i = 0;
        while (i < n)
        {
            int b = ByteAt(text, i);
            if (b < 0x80)
            {
                i++;
                continue;
            }
            int width = 1;
            int cp = 0;
            if (b >= 0xC0 && b < 0xE0 && i + 1 < n)
            {
                width = 2;
                int c1 = ByteAt(text, i + 1);
                cp = (b & 0x1F) << 6;
                cp = cp | (c1 & 0x3F);
            }
            else if (b >= 0xE0 && b < 0xF0 && i + 2 < n)
            {
                width = 3;
                int d1 = ByteAt(text, i + 1);
                int d2 = ByteAt(text, i + 2);
                cp = (b & 0x0F) << 12;
                cp = cp | ((d1 & 0x3F) << 6);
                cp = cp | (d2 & 0x3F);
            }
            else if (b >= 0xF0 && i + 3 < n)
            {
                width = 4;
            }
            if (i > start)
                parts.Insert(text.Substring(start, i - start));
            int code = 0;
            if (cp != 0 && s_Back.Find(cp, code))
                parts.Insert(Byte(code));
            else
                parts.Insert("?");
            i += width;
            start = i;
        }
        if (start < n)
            parts.Insert(text.Substring(start, n - start));
        return Join(parts);
    }

    protected static string Encode(int cp)
    {
        if (cp < 0x80)
            return Byte(cp);
        if (cp < 0x800)
        {
            int a = 0xC0 | (cp >> 6);
            int z = 0x80 | (cp & 0x3F);
            string two = Byte(a);
            two = two + Byte(z);
            return two;
        }
        int hi = 0xE0 | (cp >> 12);
        int mid = 0x80 | ((cp >> 6) & 0x3F);
        int lo = 0x80 | (cp & 0x3F);
        string three = Byte(hi);
        three = three + Byte(mid);
        three = three + Byte(lo);
        return three;
    }

    // Neighbours joined pairwise, level by level: adding piece by piece copies the whole
    // text each time (the core's OZ_ConfigSection.ReadFileText says how much).
    protected static string Join(array<string> parts)
    {
        if (parts.Count() == 0)
            return "";
        array<string> cur = parts;
        while (cur.Count() > 1)
        {
            array<string> next = new array<string>();
            int n = cur.Count();
            for (int i = 0; i + 1 < n; i += 2)
                next.Insert(cur[i] + cur[i + 1]);
            if (n % 2 == 1)
                next.Insert(cur[n - 1]);
            cur = next;
        }
        return cur[0];
    }
}

#endif
#endif
