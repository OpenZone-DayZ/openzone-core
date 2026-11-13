// A list under a class field of the OpenZone window that finds classes while their name is
// typed (design docs/specs/2026-11-13-openzone-vpp-class-search-design.md): by the class and
// by the names of the string tables, out of OZ_ClassIndex, which this list builds a slice a
// frame while the window is open (3 ms).
//
// The window makes one (OZ_VppAdminMenu.OnCreate), every glue names its fields in
// ClassFields, and the window's Tick drives it. It follows the focused edit box from Tick, as
// the window's own search does, not through OnChange: the pages answer their fields' OnChange
// themselves and do not pass it on. The list's widgets have the list's own handler; arrows and
// Enter are read with KeyState while it is open. A pick is the same as typing: the text goes
// into the field and the window's OnChange hears it, so a page keeps its draft as always.

#ifdef AVPPAdminTools
#ifndef NO_GUI

class OZ_ClassPickField
{
    int Kinds;
    bool List;
}

class OZ_ClassPick : ScriptedWidgetEventHandler
{
    // Rows on view and results at most.
    static const int ROWS = 8;
    static const int MAX = 50;
    // Layout units (OZ_VppFlex.Scale): a row, the note under the rows, the least width.
    static const float ROW_H = 24;
    static const float NOTE_H = 22;
    static const float MIN_W = 440;
    // The index's work a frame in TickCount ticks (3 ms), and how often a list on view asks
    // again while classes are still coming in, in ms.
    static const int BUDGET = 30000;
    static const int AGAIN_MS = 400;

    protected OZ_VppAdminMenu m_Menu;
    protected Widget m_Window;
    protected Widget m_Root;
    protected Widget m_Edge;
    protected Widget m_Bg;
    protected Widget m_RowsBox;
    protected TextWidget m_Note;
    protected ref array<Widget> m_Rows;
    protected ref array<Widget> m_RowLit;
    protected ref array<Widget> m_RowNameBox;
    protected ref array<Widget> m_RowClassBox;
    protected ref array<TextWidget> m_RowName;
    protected ref array<TextWidget> m_RowClass;
    protected ref map<string, ref OZ_ClassPickField> m_Fields;

    // The field the list follows, its text when last looked at, and what it found.
    protected EditBoxWidget m_Edit;
    protected OZ_ClassPickField m_Field;
    protected string m_Seen;
    protected bool m_Open;
    protected ref array<int> m_Found;
    protected int m_Total;
    protected int m_Top;
    protected int m_Lit;
    protected int m_RevSeen;
    protected int m_AskedAt;
    protected bool m_UpWas;
    protected bool m_DownWas;
    protected bool m_EnterWas;

    void OZ_ClassPick(OZ_VppAdminMenu menu, Widget window)
    {
        m_Menu = menu;
        m_Window = window;
        m_Fields = new map<string, ref OZ_ClassPickField>();
        m_Rows = new array<Widget>();
        m_RowLit = new array<Widget>();
        m_RowNameBox = new array<Widget>();
        m_RowClassBox = new array<Widget>();
        m_RowName = new array<TextWidget>();
        m_RowClass = new array<TextWidget>();
        m_Found = new array<int>();
        m_Seen = "";
        if (!window)
            return;
        m_Root = OZ_VppFlex.Create("OpenZone_VPP/gui/layouts/oz_vpp_classpick.layout", window, "OZ_ClassPick");
        if (!m_Root)
        {
            OZ_Log.Error("class search: the list's layout did not load");
            return;
        }
        m_Root.SetHandler(this);
        m_Root.SetSort(900);
        m_Root.Show(false);
        m_Edge = m_Root.FindAnyWidget("OZ_ClassPickEdge");
        m_Bg = m_Root.FindAnyWidget("OZ_ClassPickBg");
        m_RowsBox = m_Root.FindAnyWidget("OZ_ClassPickRows");
        m_Note = TextWidget.Cast(m_Root.FindAnyWidget("OZ_ClassPickNote"));
        if (!m_RowsBox)
            return;
        for (int r = 0; r < ROWS; r++)
        {
            Widget row = OZ_VppFlex.Create("OpenZone_VPP/gui/layouts/oz_vpp_classpick_row.layout", m_RowsBox, "OZ_ClassPickRow");
            if (!row)
                break;
            row.SetHandler(this);
            m_Rows.Insert(row);
            m_RowLit.Insert(row.FindAnyWidget("OZ_ClassPickRowLit"));
            m_RowNameBox.Insert(row.FindAnyWidget("OZ_ClassPickRowNameBox"));
            m_RowClassBox.Insert(row.FindAnyWidget("OZ_ClassPickRowClassBox"));
            m_RowName.Insert(TextWidget.Cast(row.FindAnyWidget("OZ_ClassPickRowName")));
            m_RowClass.Insert(TextWidget.Cast(row.FindAnyWidget("OZ_ClassPickRowClass")));
        }
    }

    // A field of a page: its edit box's name, the kinds of classes it takes (a sum of
    // OZ_ClassIndex.ITEM, OBJECT, VEHICLE), and whether it holds a list ("classes, comma
    // between", class:quantity): then the last word is searched and a pick replaces it.
    void Bind(string name, int kinds, bool list = false)
    {
        OZ_ClassPickField f = new OZ_ClassPickField();
        f.Kinds = kinds;
        f.List = list;
        m_Fields.Set(name, f);
    }

    // Every frame while the window is open.
    void Tick()
    {
        OZ_ClassIndex.Advance(BUDGET);
        if (!m_Root || !m_RowsBox)
            return;

        EditBoxWidget edit = FocusedField();
        if (m_Open && m_Edit && edit != m_Edit && ListKeyDown())
        {
            // The engine moves the focus with the arrows, off the field onto the next widget
            // (measured 2026-11-13 on the trader's goods page): the list takes the key and
            // gives the focus back.
            SetFocus(m_Edit);
            edit = m_Edit;
        }
        if (!edit)
        {
            // A press on a row takes the focus off the field before the click lands: the list
            // stays while the pointer is on it.
            if (m_Open && Hovered())
                return;
            CloseList();
            m_Edit = null;
            return;
        }

        string text = edit.GetText();
        if (edit != m_Edit)
        {
            // A field just entered: the list waits for typing, not for a click into a field
            // that already holds a class.
            m_Edit = edit;
            m_Field = m_Fields.Get(edit.GetName());
            m_Seen = text;
            CloseList();
            return;
        }

        bool changed = text != m_Seen;
        bool fresher = false;
        if (m_Open && OZ_ClassIndex.Revision() != m_RevSeen)
        {
            if (GetGame().GetTime() - m_AskedAt >= AGAIN_MS)
                fresher = true;
        }
        if (changed || fresher)
        {
            m_Seen = text;
            SearchText(changed);
        }
        if (!m_Open)
            return;
        PlaceList();
        ReadKeys();
    }

    // ------------------------------------------------------------------ searching

    protected void SearchText(bool typed)
    {
        string q = QueryOf(m_Seen);
        if (q.LengthUtf8() < 2)
        {
            CloseList();
            return;
        }
        int kinds = OZ_ClassIndex.ANY;
        if (m_Field)
            kinds = m_Field.Kinds;
        m_RevSeen = OZ_ClassIndex.Revision();
        m_AskedAt = GetGame().GetTime();
        array<int> found;
        int total;
        int t0 = TickCount(0);
        OZ_ClassIndex.Find(q, kinds, MAX, found, total);
        if (OZ_Log.IsDebug())
        {
            // TickCount ticks at 10 MHz.
            float ms = TickCount(t0) / 10000.0;
            string said = "class search '" + q + "': " + total.ToString() + " found of " + OZ_ClassIndex.Count().ToString();
            said = said + " classes in " + ms.ToString() + " ms";
            OZ_Log.Dbg(said);
        }
        m_Found = found;
        m_Total = total;
        // A new text starts at the top; more classes for the same text keep the lit row.
        if (typed || m_Lit >= m_Found.Count())
        {
            m_Top = 0;
            m_Lit = 0;
        }
        OpenList();
        PaintRows();
    }

    // What a field's text searches: the whole of it, or a list's last word; never a
    // :quantity after the class.
    protected string QueryOf(string text)
    {
        string q = text;
        if (m_Field && m_Field.List)
        {
            int start = LastWordAt(text);
            q = text.Substring(start, text.Length() - start);
        }
        int colon = q.IndexOf(":");
        if (colon >= 0)
            q = q.Substring(0, colon);
        return q.Trim();
    }

    // Where a list's last word begins: after its last comma or line break.
    protected int LastWordAt(string text)
    {
        int at = 0;
        int n = text.Length();
        for (int i = 0; i < n; i++)
        {
            string c = text.Get(i);
            if (c == "," || c == "\n")
                at = i + 1;
        }
        return at;
    }

    // The class at a place of the results into the field, as if typed.
    protected void PickAt(int place)
    {
        if (!m_Edit || place < 0 || place >= m_Found.Count())
            return;
        string cls = OZ_ClassIndex.ClassAt(m_Found[place]);
        string text = m_Edit.GetText();
        string next = cls;
        if (m_Field && m_Field.List)
        {
            // The last word gives way to the class; its leading spaces and :quantity stay.
            int start = LastWordAt(text);
            string head = text.Substring(0, start);
            string word = text.Substring(start, text.Length() - start);
            string lead = "";
            int w = 0;
            while (w < word.Length() && word.Get(w) == " ")
            {
                lead = lead + " ";
                w++;
            }
            string tail = "";
            int colon = word.IndexOf(":");
            if (colon >= 0)
                tail = word.Substring(colon, word.Length() - colon);
            next = head + lead + cls + tail;
        }
        m_Edit.SetText(next);
        m_Seen = next;
        CloseList();
        SetFocus(m_Edit);
        // The page hears it as typing: its own OnChange keeps its draft.
        if (m_Menu)
            m_Menu.OnChange(m_Edit, 0, 0, false);
    }

    // ------------------------------------------------------------------ the list

    protected void OpenList()
    {
        if (!m_Open)
        {
            // Keys held as the list appears are not presses.
            m_UpWas = KeyState(KeyCode.KC_UP) != 0;
            m_DownWas = KeyState(KeyCode.KC_DOWN) != 0;
            m_EnterWas = EnterDown();
        }
        m_Open = true;
        m_Root.Show(true);
    }

    protected void CloseList()
    {
        m_Open = false;
        if (m_Root)
            m_Root.Show(false);
    }

    protected void PaintRows()
    {
        for (int r = 0; r < m_Rows.Count(); r++)
        {
            int place = m_Top + r;
            if (place >= m_Found.Count())
                continue;
            int i = m_Found[place];
            string name = OZ_ClassIndex.NameAt(i);
            string cls = OZ_ClassIndex.ClassAt(i);
            // A class with no name of its own (a building) is said once, on the left.
            if (name == cls)
                cls = "";
            if (m_RowName[r])
                m_RowName[r].SetText(name);
            if (m_RowClass[r])
                m_RowClass[r].SetText(cls);
            if (m_RowLit[r])
                m_RowLit[r].Show(place == m_Lit);
        }
        if (m_Note)
            m_Note.SetText(NoteText());
        PlaceList();
    }

    // The line under the rows: the index still coming in, nothing found, or more than shown.
    protected string NoteText()
    {
        if (!OZ_ClassIndex.Walked())
        {
            string building = Widget.TranslateString("#STR_OZ_VPP_CP_INDEXING");
            building.Replace("%1", OZ_ClassIndex.Percent().ToString());
            return building;
        }
        if (m_Found.Count() == 0)
            return Widget.TranslateString("#STR_OZ_VPP_CP_NONE");
        if (m_Total > m_Found.Count())
        {
            int extra = m_Total - m_Found.Count();
            string more = Widget.TranslateString("#STR_OZ_VPP_CP_MORE");
            more.Replace("%1", extra.ToString());
            return more;
        }
        return "";
    }

    // Under the field, inside the window, every frame (the window moves and resizes); above
    // the field when there is no room under it.
    protected void PlaceList()
    {
        if (!m_Edit || !m_Window)
            return;
        float s = OZ_VppFlex.Scale();
        float ex;
        float ey;
        float ew;
        float eh;
        m_Edit.GetScreenPos(ex, ey);
        m_Edit.GetScreenSize(ew, eh);
        float wx;
        float wy;
        float ww;
        float wh;
        m_Window.GetScreenPos(wx, wy);
        m_Window.GetScreenSize(ww, wh);

        float w = ew;
        if (w < MIN_W * s)
            w = MIN_W * s;
        if (w > ww)
            w = ww;
        int shown = m_Found.Count() - m_Top;
        if (shown > m_Rows.Count())
            shown = m_Rows.Count();
        if (shown < 0)
            shown = 0;
        float rowH = ROW_H * s;
        float noteH = 0;
        if (NoteText() != "")
            noteH = NOTE_H * s;
        float rowsH = rowH * shown;
        float h = rowsH + noteH + 2;

        float x = ex - wx;
        float y = ey - wy + eh;
        if (x + w > ww)
            x = ww - w;
        if (x < 0)
            x = 0;
        if (y + h > wh)
            y = ey - wy - h;
        m_Root.SetPos(x, y);
        m_Root.SetSize(w, h);
        if (m_Edge)
            m_Edge.SetSize(w, h);
        if (m_Bg)
        {
            m_Bg.SetPos(1, 1);
            m_Bg.SetSize(w - 2, h - 2);
        }
        m_RowsBox.SetPos(1, 1);
        m_RowsBox.SetSize(w - 2, rowsH);

        float inner = w - 2;
        float nameW = inner * 0.56;
        float pad = 8 * s;
        for (int r = 0; r < m_Rows.Count(); r++)
        {
            Widget row = m_Rows[r];
            row.Show(r < shown);
            row.SetPos(0, rowH * r);
            row.SetSize(inner, rowH);
            if (m_RowLit[r])
                m_RowLit[r].SetSize(inner, rowH);
            if (m_RowNameBox[r])
            {
                m_RowNameBox[r].SetPos(pad, 0);
                m_RowNameBox[r].SetSize(nameW - pad * 1.5, rowH);
            }
            if (m_RowName[r])
                m_RowName[r].SetSize(nameW - pad * 1.5, rowH);
            if (m_RowClassBox[r])
            {
                m_RowClassBox[r].SetPos(nameW, 0);
                m_RowClassBox[r].SetSize(inner - nameW - pad, rowH);
            }
            if (m_RowClass[r])
                m_RowClass[r].SetSize(inner - nameW - pad, rowH);
        }
        if (m_Note)
        {
            m_Note.Show(noteH > 0);
            m_Note.SetPos(pad + 2, rowsH + 1);
            m_Note.SetSize(w - pad * 2 - 2, noteH);
        }
    }

    // ------------------------------------------------------------------ input

    protected void ReadKeys()
    {
        bool up = KeyState(KeyCode.KC_UP) != 0;
        bool down = KeyState(KeyCode.KC_DOWN) != 0;
        bool enter = EnterDown();
        bool goUp = up && !m_UpWas;
        bool goDown = down && !m_DownWas;
        bool pick = enter && !m_EnterWas;
        m_UpWas = up;
        m_DownWas = down;
        m_EnterWas = enter;
        if (goDown)
            MoveLit(1);
        if (goUp)
            MoveLit(-1);
        if (pick && m_Found.Count() > 0)
            PickAt(m_Lit);
    }

    protected bool ListKeyDown()
    {
        if (KeyState(KeyCode.KC_UP) != 0)
            return true;
        if (KeyState(KeyCode.KC_DOWN) != 0)
            return true;
        return EnterDown();
    }

    protected bool EnterDown()
    {
        if (KeyState(KeyCode.KC_RETURN) != 0)
            return true;
        return KeyState(KeyCode.KC_NUMPADENTER) != 0;
    }

    protected void MoveLit(int step)
    {
        int n = m_Found.Count();
        if (n == 0)
            return;
        m_Lit = m_Lit + step;
        if (m_Lit < 0)
            m_Lit = 0;
        if (m_Lit >= n)
            m_Lit = n - 1;
        if (m_Lit < m_Top)
            m_Top = m_Lit;
        if (m_Lit >= m_Top + m_Rows.Count())
            m_Top = m_Lit - m_Rows.Count() + 1;
        PaintRows();
    }

    override bool OnClick(Widget w, int x, int y, int button)
    {
        int r = m_Rows.Find(w);
        if (r < 0)
            return false;
        PickAt(m_Top + r);
        return true;
    }

    override bool OnMouseEnter(Widget w, int x, int y)
    {
        int r = m_Rows.Find(w);
        if (r < 0)
            return false;
        int place = m_Top + r;
        if (place != m_Lit && place < m_Found.Count())
        {
            m_Lit = place;
            PaintRows();
        }
        return false;
    }

    override bool OnMouseWheel(Widget w, int x, int y, int wheel)
    {
        int n = m_Found.Count();
        int rows = m_Rows.Count();
        if (n <= rows)
            return true;
        // The wheel turned up (above zero) shows what is above.
        int top = m_Top - wheel;
        if (top < 0)
            top = 0;
        if (top > n - rows)
            top = n - rows;
        if (top != m_Top)
        {
            m_Top = top;
            PaintRows();
        }
        return true;
    }

    // ------------------------------------------------------------------ where things are

    // The focused edit box when it is a bound field of this window.
    protected EditBoxWidget FocusedField()
    {
        EditBoxWidget e = EditBoxWidget.Cast(GetFocus());
        if (!e || !m_Fields.Contains(e.GetName()))
            return null;
        Widget p = e;
        while (p)
        {
            if (p == m_Window)
                return e;
            p = p.GetParent();
        }
        return null;
    }

    protected bool Hovered()
    {
        Widget w = GetWidgetUnderCursor();
        while (w)
        {
            if (w == m_Root)
                return true;
            w = w.GetParent();
        }
        return false;
    }
}

#endif
#endif
