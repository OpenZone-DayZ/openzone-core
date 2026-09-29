// Як назвати людину так, щоб клієнт її впізнав, а стороннього не впізнав ніхто.
//
// Списки контактів і розмов ходять на клієнт із іменами гравців -- показати
// треба саме ім'я. Але ДІЯТИ по імені не можна: імена в DayZ вибирає гравець,
// вони повторюються, і буває, що імені ще немає в кеші зовсім.
//
// Steam-id клієнтові теж не місце: він рідний ідентифікатор гравця, за ним
// людину знаходять поза грою. Тому на клієнт їде хеш -- він стабільний між
// сесіями (одне й те саме число з того самого id), нічого про людину не
// каже, і в межах ОДНОГО списку друзів однозначний.
class OZ_Names
{
    static string KeyOf(string uid)
    {
        if (uid == "")
            return "";
        return uid.Hash().ToString();
    }

    // Знайти в списку. Порожній рядок означає «не знайшов АБО знайшов двох» --
    // і в обох випадках правильна відповідь одна: не вгадувати.
    //
    // Другий випадок тут майже неможливий (два хеші збіглись у межах одного
    // списку друзів), але «майже» вирішується перевіркою на три рядки, а
    // не припущенням.
    static string PickIn(array<string> uids, string key)
    {
        if (!uids)
            return "";
        if (key == "")
            return "";

        string found = "";

        for (int i = 0; i < uids.Count(); i++)
        {
            if (KeyOf(uids[i]) != key)
                continue;

            if (found != "")
                return "";

            found = uids[i];
        }

        return found;
    }
}

// Знайти ЖИВЕ ТІЛО за Steam64 -- один обхід GetPlayers() на все ядро.
//
// Той самий цикл із тими самими null-перевірками стояв двічі: в
// OZ_Link.Online (треба особа) і в OZ_SpawnSection.BodyOf (треба позиція).
// Два написання того самого обходу -- це дві швидкості й два набори помилок,
// а зникає розбіжність між ними мовчки.
//
// І друге питання про ту саму людину -- ЧИЙ ЦЕ ВИХІД (Remember нижче).
class OZ_Players
{
    // ------------------------------------------------- хто саме вийшов
    //
    // ХЕШ -> STEAM64, записаний на вході. Потрібен рівно одному питанню:
    // чий це дисконект, коли особи вже може не бути.
    //
    // CF віддає вихід двома подіями, і жодна не несе Steam64 напряму.
    // OnInvokeDisconnect іде з БАЗОВИМИ аргументами (CF_EventPlayerArgs,
    // MissionServer.InvokeOnDisconnect): Cast до
    // CF_EventPlayerDisconnectedArgs там не вдається ніколи, і всі
    // прибирання серії, що стояли на ньому, мовчки виходили на кожному
    // дисконекті (знайдено 2026-09-28). OnClientDisconnect несе справжні
    // аргументи виходу, але його UID -- ванільний `uid`, тобто
    // identity.GetId(): ХЕШ, а не Steam64, під яким лежать усі наші сховища.
    // А особи на цю мить уже може не бути -- ваніль так і пише в
    // PlayerDisconnected. Перекласти хеш назад можна лише тим, що ми
    // запам'ятали, поки вона була.
    //
    // ЗАПИСІВ НЕ ВИДАЛЯЄМО, навіть прочитавши. Один вихід читають кілька
    // модулів -- ядро, фракції, рація, КПК -- у порядку, якого CF не обіцяє, і
    // перший, хто стер би рядок, осліпив би решту. Мапа обмежена унікальними
    // гравцями ОДНОГО запуску (два короткі рядки на людину), і рестарт її
    // обнуляє.
    //
    // ЛІНИВО, а не `new` у декларації: такий ініціалізатор статика вже раз не
    // виконався на клієнті (OZ_PdaRoute у худі КПК, 2026-08-30), і правило
    // серії -- заводити статичні контейнери при першому зверненні.
    private static ref map<string, string> s_PlainOf;

    // Кличе ядро на кожному вході (OZ_Module.OnInvokeConnect).
    static void Remember(PlayerIdentity id)
    {
        if (!id)
            return;

        string hashed = id.GetId();
        string plain  = id.GetPlainId();
        if (hashed == "" || plain == "")
            return;

        if (!s_PlainOf)
            s_PlainOf = new map<string, string>();
        s_PlainOf.Set(hashed, plain);
    }

    // Steam64 того, хто виходить, або порожньо, коли ми його не знаємо.
    //
    // Аргументи -- ті, що CF дає в OnClientDisconnect. Жива особа, якщо вона
    // ще є, -- найпевніша відповідь; інакше хеш із UID, перекладений тим, що
    // запам'ятав Remember. Порожній рядок викликач мусить сприймати як «не
    // гравець»: клеїти його в дорогу чи в ключ не можна.
    static string PlainOfLeaving(CF_EventArgs args)
    {
        CF_EventPlayerDisconnectedArgs dArgs = CF_EventPlayerDisconnectedArgs.Cast(args);
        if (!dArgs)
            return "";

        if (dArgs.Identity)
        {
            string plain = dArgs.Identity.GetPlainId();
            if (plain != "")
                return plain;
        }

        if (dArgs.UID == "" || !s_PlainOf)
            return "";

        string known;
        if (s_PlainOf.Find(dArgs.UID, known))
            return known;
        return "";
    }

    static Man ManOf(string uid)
    {
        if (uid == "")
            return null;

        array<Man> players = new array<Man>();
        GetGame().GetPlayers(players);

        for (int i = 0; i < players.Count(); i++)
        {
            if (!players[i])
                continue;

            PlayerIdentity id = players[i].GetIdentity();
            if (!id)
                continue;
            if (id.GetPlainId() == uid)
                return players[i];
        }
        return null;
    }
}
