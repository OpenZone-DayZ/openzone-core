// The window's palette at run time (VPP rework 2026-11-12, design §2.3).
//
// The layout generator bakes colours into a layout when it is built, and every button,
// field and label a description leaves uncoloured gets the series palette of
// ui/tokens.json -- the PDA's steel and cold light. The window wears the trader's metal
// instead (the owner's choice), and the PDA keeps its own look. So a pane, the core's or
// a glue's, is repainted when the shell takes it: every widget whose colour is one of the
// series colours gets the window's colour of the same role (the vpp* tokens). A colour
// that is not in the table -- the trader's own, one a script set on purpose -- is left as
// it is. A pane that recolours its widgets from script later is repainted again after
// each event and answer (OZ_VppAdminMenu.RepaintSoon), until it is moved onto the page
// class and colours itself.

#ifdef AVPPAdminTools
#ifndef NO_GUI

class OZ_VppPaint
{
    protected static ref array<int> s_From;
    protected static ref array<int> s_To;

    protected static void Pair(float r1, float g1, float b1, float a1, float r2, float g2, float b2, float a2)
    {
        s_From.Insert(ARGBF(a1, r1, g1, b1));
        s_To.Insert(ARGBF(a2, r2, g2, b2));
    }

    // The series colour -> the window's colour of the same role. Both sides are the
    // values of openzone-core\ui\tokens.json (`color`, and its vpp* names).
    protected static void Table()
    {
        if (s_From)
            return;

        s_From = new array<int>();
        s_To = new array<int>();
        Pair(0.055, 0.075, 0.095, 1,   0.035, 0.035, 0.035, 0.97);  // screen
        Pair(0.10, 0.135, 0.17, 1,     0.047, 0.047, 0.047, 1);     // panel
        Pair(0.135, 0.18, 0.225, 1,    0.118, 0.118, 0.114, 1);     // raised
        Pair(0.22, 0.30, 0.38, 1,      0.235, 0.235, 0.227, 1);     // edge
        Pair(0.90, 0.93, 0.95, 1,      0.88, 0.875, 0.855, 1);      // text
        Pair(0.58, 0.65, 0.71, 1,      0.627, 0.624, 0.6, 1);       // muted
        Pair(0.37, 0.45, 0.52, 1,      0.36, 0.353, 0.345, 1);      // faint
        Pair(0.31, 0.71, 0.91, 1,      0.91, 0.47, 0.17, 1);        // accent
        Pair(0.94, 0.54, 0.14, 1,      0.96, 0.78, 0.25, 1);        // alert
        Pair(0.39, 0.76, 0.36, 1,      0.4, 0.78, 0.36, 1);         // ok
        Pair(0.88, 0.32, 0.32, 1,      0.88, 0.32, 0.29, 1);        // bad
        Pair(0.31, 0.71, 0.91, 0.18,   0.91, 0.47, 0.17, 0.16);     // pick
        Pair(0.31, 0.71, 0.91, 0.10,   0.91, 0.47, 0.17, 0.08);     // hover
        Pair(0.043, 0.063, 0.082, 1,   0.035, 0.035, 0.035, 1);     // rail
    }

    // The window's colour for a series colour; any other colour as it is.
    static int Of(int c)
    {
        Table();
        for (int i = 0; i < s_From.Count(); i++)
        {
            if (Near(c, s_From[i]))
                return s_To[i];
        }
        return c;
    }

    // Every widget under root, root included. A list of widgets to visit, not
    // recursion: a pane is a few hundred widgets deep and wide.
    static void Repaint(Widget root)
    {
        if (!root)
            return;

        Table();
        array<Widget> todo = new array<Widget>();
        todo.Insert(root);
        int at = 0;
        while (at < todo.Count())
        {
            Widget w = todo[at];
            at++;
            int was = w.GetColor();
            int now = Of(was);
            if (now != was)
                w.SetColor(now);
            Widget kid = w.GetChildren();
            while (kid)
            {
                todo.Insert(kid);
                kid = kid.GetSibling();
            }
        }
    }

    // Each channel within 2 of 255: the engine keeps what the layout said as bytes.
    protected static bool Near(int a, int b)
    {
        for (int shift = 0; shift < 32; shift = shift + 8)
        {
            int x = (a >> shift) & 255;
            int y = (b >> shift) & 255;
            if (Math.AbsInt(x - y) > 2)
                return false;
        }
        return true;
    }
}

#endif
#endif
