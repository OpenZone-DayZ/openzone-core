// Other admins' actions in the window's journal (VPP rework 2026-11-12, design §3.2,
// phase 6). Every admin answer the server sends (OZ_Rpc.AdminRespond, a deferred one
// too) is told to every other admin online, except a section's reads; their window
// journals it under the asker's name. The window leaves out the reads it knows as
// well (IsReadOp), so a section that declares none still stays out of the journal.
class OZ_AdminTell
{
    static void Answered(PlayerIdentity to, string sectionId, string op, bool ok, string error)
    {
        if (!to || !GetGame().IsServer())
            return;
        OZ_AdminSection sec = OZ_AdminRegistry.Get(sectionId);
        if (sec && sec.IsRead(op))
            return;

        array<Man> men = new array<Man>();
        GetGame().GetPlayers(men);
        if (men.Count() < 2)
            return;

        string asker = to.GetId();
        string who = to.GetName();
        for (int i = 0; i < men.Count(); i++)
        {
            Man m = men[i];
            if (!m)
                continue;
            PlayerIdentity id = m.GetIdentity();
            if (!id || id.GetId() == asker)
                continue;
            if (!OZ_Perm.IsAdmin(id))
                continue;
            OZ_Rpc.AdminEcho(id, who, sectionId, op, ok, error);
        }
    }
}
