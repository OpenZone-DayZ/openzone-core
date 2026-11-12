// The window's journal (VPP rework 2026-11-12, design §3.2): what was asked and what came
// back, oldest first, the last KEEP entries in this client's memory. It replaces the one
// hint line a pane had: an answer no longer overwrites the one before it. The status bar
// shows the newest entry; Ядро › Огляд shows them all.
//
// An entry is opened by the shell's Ask and closed by the answer with the same section
// and op -- the wire carries no request id (design §3.1) -- or by "no answer" after
// WAIT_MS. A line a pane writes with Hint() is an entry of its own.

#ifdef AVPPAdminTools
#ifndef NO_GUI

class OZ_VppJournalEntry
{
    string Time;
    string Module;
    string What;
    string Result;
    int    Level;
    string Section;
    string Op;
    int    Asked;
}

class OZ_VppJournal
{
    static const int KEEP    = 300;
    static const int WAIT_MS = 15000;

    // Levels.
    static const int LV_WAIT = -1;
    static const int LV_INFO = 0;
    static const int LV_OK   = 1;
    static const int LV_WARN = 2;
    static const int LV_ERR  = 3;

    protected static ref array<ref OZ_VppJournalEntry> s_Entries = new array<ref OZ_VppJournalEntry>();

    // Grows with every change: the status bar and the journal list repaint when it moved.
    protected static int s_Revision = 0;

    static int Revision()
    {
        return s_Revision;
    }

    static int Count()
    {
        return s_Entries.Count();
    }

    static OZ_VppJournalEntry At(int i)
    {
        if (i < 0 || i >= s_Entries.Count())
            return null;
        return s_Entries[i];
    }

    static OZ_VppJournalEntry Last()
    {
        return At(s_Entries.Count() - 1);
    }

    static string Clock()
    {
        int h, m, s;
        GetHourMinuteSecond(h, m, s);
        return Two(h) + ":" + Two(m) + ":" + Two(s);
    }

    protected static string Two(int n)
    {
        if (n < 10)
            return "0" + n.ToString();
        return n.ToString();
    }

    static OZ_VppJournalEntry Add(string module, string what, string result, int level)
    {
        OZ_VppJournalEntry e = new OZ_VppJournalEntry();
        e.Time = Clock();
        e.Module = module;
        e.What = what;
        e.Result = result;
        e.Level = level;
        s_Entries.Insert(e);
        while (s_Entries.Count() > KEEP)
            s_Entries.RemoveOrdered(0);
        s_Revision++;
        return e;
    }

    // A request on its way: the entry waits for its answer.
    static void Asked(string module, string section, string op, string what)
    {
        OZ_VppJournalEntry e = Add(module, what, "", LV_WAIT);
        e.Section = section;
        e.Op = op;
        e.Asked = GetGame().GetTime();
    }

    // The answer closes the oldest request with the same section and op. An answer
    // nobody asked for here (a refresh another pane started) is written as its own
    // entry only when it failed.
    static void Answered(string section, string op, bool ok, string error)
    {
        for (int i = 0; i < s_Entries.Count(); i++)
        {
            OZ_VppJournalEntry e = s_Entries[i];
            if (e.Level != LV_WAIT || e.Section != section || e.Op != op)
                continue;
            Close(e, ok, error);
            return;
        }
        if (!ok)
            Add(section, op, Words(error), LV_ERR);
    }

    protected static void Close(OZ_VppJournalEntry e, bool ok, string error)
    {
        if (ok)
        {
            e.Result = "#STR_OZ_VPP_DONE";
            e.Level = LV_OK;
        }
        else
        {
            e.Result = Words(error);
            e.Level = LV_ERR;
        }
        s_Revision++;
    }

    // A request that waited longer than WAIT_MS has no answer coming.
    static void Expire()
    {
        int now = GetGame().GetTime();
        for (int i = 0; i < s_Entries.Count(); i++)
        {
            OZ_VppJournalEntry e = s_Entries[i];
            if (e.Level != LV_WAIT || now - e.Asked < WAIT_MS)
                continue;
            e.Result = "#STR_OZ_VPP_NO_ANSWER";
            e.Level = LV_ERR;
            s_Revision++;
        }
    }

    // An error is a stringtable key or the bridge's own words: keys are translated,
    // words are shown as they came.
    static string Words(string error)
    {
        if (error.IndexOf("STR_") == 0)
            return Widget.TranslateString("#" + error);
        if (error.IndexOf("#STR_") == 0)
            return Widget.TranslateString(error);
        return error;
    }

    // What a line says, translated when it is a key.
    static string Text(string s)
    {
        if (s.IndexOf("#STR_") == 0)
            return Widget.TranslateString(s);
        return s;
    }
}

#endif
#endif
