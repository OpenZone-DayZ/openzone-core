// Пакет синхронізації: скласти й відправити одному гравцеві.
//
// Було приватним методом OZ_Module і їхало рівно раз -- коли клієнт, з'явившись
// у світі, сам його попросив. ТЗ-5 R-C1.3 вимагає ДЕЛЬТИ: стан прив'язки
// Discord міняється посеред сесії (гравець пройшов /link), і ворота на
// клієнті мусять дізнатись про це зараз, а не після перезаходу. Тому
// відправник -- окремий і статичний: його кличе і OZ_Module на запит, і
// OZ_Link.Confirm на зміну.
//
// Пакет крихітний і подія рідкісна, тож жодного «лише різниця»: їде той самий
// повний конверт, і клієнт застосовує його так само, як перший. Одна форма
// пакета -- одна дорога його розбору.

class OZ_SyncSender
{
    static void Send(PlayerIdentity to, string why)
    {
        if (!to)
            return;

        OZ_SyncPayload p = new OZ_SyncPayload();
        p.DebugMode = OZ_Settings.Get().DebugMode;

        // Прив'язка їде тим самим конвертом: на вході це найраніша мить, коли
        // є кому показати ворота, а посеред сесії -- та сама дорога, якою
        // вони й відчиняються.
        p.Linked       = OZ_Link.IsLinked(to.GetPlainId());
        p.LinkRequired = OZ_Link.Gated(to.GetPlainId());
        OZ_PageRegistry.FillPayload(p);

        // Моди докладають своє (OZ_SyncExtras): ядро не знає, що саме, і не
        // мусить.
        OZ_SyncExtras.OnFill().Invoke(p);

        string json;
        string err;
        // prettyPrint=false: у провід не треба ані відступів, ані переносів.
        if (!JsonFileLoader<OZ_SyncPayload>.MakeData(p, json, err, false))
        {
            OZ_Log.Error("cannot serialise sync payload: " + err);
            return;
        }

        OZ_Rpc.SendSync(to, json);

        string line = "sync: sent to " + to.GetPlainId() + " " + why;
        line += " (extras=" + p.Extras.Count().ToString();
        line += ", " + json.Length().ToString() + " b)";
        OZ_Log.Dbg(line);
    }

    // Усім, хто в грі. Потрібно, коли стан, який їде пакетом, змінив адмін
    // посеред сесії -- профілі рацій у вкладці VPP (2026-09-20): клієнт
    // отримує пакет на вході й далі не питає, тож без цієї розсилки він
    // лишався б зі старим ефіром до перезаходу. Повний пакет кожному, а не
    // «лише різниця»: подія рідкісна, пакет малий, а одна форма пакета --
    // одна дорога його розбору.
    static void SendAll(string why)
    {
        array<Man> players = new array<Man>();
        GetGame().GetPlayers(players);

        int sent = 0;
        for (int i = 0; i < players.Count(); i++)
        {
            if (!players[i])
                continue;

            PlayerIdentity to = players[i].GetIdentity();
            if (!to)
                continue;

            Send(to, why);
            sent++;
        }

        OZ_Log.Info("sync: re-sent to " + sent.ToString() + " player(s): " + why);
    }
}
