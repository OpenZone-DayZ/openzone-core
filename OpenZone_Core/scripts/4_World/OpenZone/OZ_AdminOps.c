// Адмінська консоль: серверна половина, з якою говорить будь-який
// адмінський UI (першою -- вкладка VPP).
//
// РОЗДІЛИ, А НЕ СТОРІНКИ (рішення власника 2026-09-01, ТЗ-5 §C2-C3). Досі це
// була сторінка "admin" у спільному реєстрі, i щоб її пропустити, гейт КПК
// тримав виняток першим рядком -- тобто в межі безпеки були двері збоку.
// Тепер розділи живуть у власному реєстрі (OZ_AdminRegistry), їдуть власним
// конвертом (OZ_AdminReq/OZ_AdminRes), а ворота одні й без винятків:
// OZ_Perm.IsAdmin, на сервері, першим рядком диспетчера в OZ_Module.
//
// Ядро приносить два розділи: "config" (редактор зареєстрованих конфігів) i
// "spawns" (зони, стейджинґ, особисті точки). Мод фракцій доклада свій.

// Один редагований конфіг: як його звати, де лежить файл i хто вміє
// ПЕРЕВІРИТИ й ЗАСТОСУВАТИ новий текст. Реєструє власник конфігу -- ядро
// свої, КПК свої; ядро не знає чужих типів i знати не мусить.
class OZ_AdminCfgApplier
{
    // Розібрати текст, зберегти файл i перечитати живий конфіг. Повертає
    // false, коли текст не розбирається, -- ФАЙЛ ТОДІ НЕ ЧІПАЄТЬСЯ: сміття
    // не досягає диска, i чинна версія лишається чинною.
    bool Apply(string json)
    {
        return false;
    }
}

class OZ_AdminCfgEntry
{
    string Name;
    string Path;
    // Чий конфіг: "core" чи "pda" -- вкладки адмінки діляться за власником.
    string Owner;
    ref OZ_AdminCfgApplier Applier;
}

class OZ_AdminCfg
{
    // Контейнер -- одразу при оголошенні. Ensure()/«if (!s_X) s_X = new ...»
    // перед кожним зверненням повторювався у восьми класах ядра, хоч поруч
    // (OZ_BridgeClient.s_UidProviders) статик уже ініціювався при оголошенні
    // й працював.
    private static ref array<ref OZ_AdminCfgEntry> s_All = new array<ref OZ_AdminCfgEntry>();

    static void Register(string name, string path, OZ_AdminCfgApplier applier, string owner = "core")
    {
        OZ_AdminCfgEntry e = new OZ_AdminCfgEntry();
        e.Name    = name;
        e.Path    = path;
        e.Owner   = owner;
        e.Applier = applier;
        s_All.Insert(e);
    }

    static OZ_AdminCfgEntry Find(string name)
    {
        for (int i = 0; i < s_All.Count(); i++)
        {
            if (s_All[i].Name == name)
                return s_All[i];
        }
        return null;
    }

    static int Count()
    {
        return s_All.Count();
    }

    // The files page shows where each file lives (outPaths, VPP rework 2026-11-12).
    static void Names(array<string> outNames, array<string> outOwners, array<string> outPaths = null)
    {
        for (int i = 0; i < s_All.Count(); i++)
        {
            outNames.Insert(s_All[i].Name);
            outOwners.Insert(s_All[i].Owner);
            if (outPaths)
                outPaths.Insert(s_All[i].Path);
        }
    }
}

// Конверти операцій. OZ_AdminAsk тут БІЛЬШЕ НЕМАЄ: ім'я конфігу їде в самій
// операції, а тіло -- сирим тілом запиту (див. OZ_ConfigSection.Handle), тож
// конверт зі строковим полем був оголошений i не вжитий жодного разу.

class OZ_AdminCfgList
{
    ref array<string> Names;
    ref array<string> Owners;
    // Where each file lives ($profile:...), side by side with Names.
    ref array<string> Paths;

    void OZ_AdminCfgList()
    {
        Names  = new array<string>();
        Owners = new array<string>();
        Paths  = new array<string>();
    }
}

// The answer of `identity` (VPP rework 2026-11-12, design §5.5): who gives the
// factions and what that source can say.
class OZ_AdminIdentity
{
    bool   Present;
    string Source;
    bool   HasLeaders;
    bool   HasPosts;
    string LeaderMeans;

    // The faction ids the source knows (FactionIds) and their names (FactionName),
    // side by side: the spawns page offers them as the zones' owners.
    ref array<string> Factions;
    ref array<string> FactionNames;

    void OZ_AdminIdentity()
    {
        Factions = new array<string>();
        FactionNames = new array<string>();
    }

    // A copy made by script (the OZ_ConfigBase header): the page keeps the lists.
    OZ_AdminIdentity Copy()
    {
        OZ_AdminIdentity c = new OZ_AdminIdentity();
        c.Present = Present;
        c.Source = Source;
        c.HasLeaders = HasLeaders;
        c.HasPosts = HasPosts;
        c.LeaderMeans = LeaderMeans;
        if (Factions)
        {
            for (int i = 0; i < Factions.Count(); i++)
                c.Factions.Insert(Factions[i]);
        }
        if (FactionNames)
        {
            for (int k = 0; k < FactionNames.Count(); k++)
                c.FactionNames.Insert(FactionNames[k]);
        }
        return c;
    }
}

class OZ_ConfigSection : OZ_AdminSection
{
    override bool IsRead(string op)
    {
        if (op.IndexOf("cfg_get:") == 0)
            return true;
        array<string> reads = {"cfg_list", "mirror_list", "identity", "overview"};
        return reads.Find(op) != -1;
    }

    override string Handle(string op, string json, PlayerIdentity sender, out bool ok, out string error)
    {
        ok = false;

        // Прав тут БІЛЬШЕ НЕ ПЕРЕВІРЯЄМО: їх перевірив диспетчер, до розбору
        // операції. Друга перевірка в кожному розділі виглядала б обережною,
        // але робила б межу безпеки розсипаною по модах -- а її треба вміти
        // прочитати в одному місці.

        if (op == "cfg_list")
            return CfgList(ok, error);

        // Ім'я конфігу живе В САМІЙ операції, а тіло їде СИРИМ тілом
        // запиту/відповіді. Конверт зі строковим полем тут заборонений:
        // JsonFileLoader ріже строкове ЗНАЧЕННЯ на 1023 байтах при розборі
        // (зміряно 2026-08-30: envelope=2925, body=1023), i будь-який конфіг
        // довший за кілобайт приїздив обрубком.
        if (op.IndexOf("cfg_get:") == 0)
            return CfgGet(op.Substring(8, op.Length() - 8), ok, error);
        if (op.IndexOf("cfg_set:") == 0)
            return CfgSet(op.Substring(8, op.Length() - 8), json, sender, ok, error);

        // Дзеркало Discord -- операція, не правка файла (ТЗ-2 R5.1): вмикання
        // спершу заливає історію ботом. Живе в розділі конфігів, бо міняє
        // саме Settings, і адмін шукатиме її поруч із RAW JSON.
        if (op == "mirror_list")
            return OZ_MirrorOps.List(ok, error);

        // Who gives the factions and what it can say (design §5.5): the pages
        // that decide who may do what dim the fields its source does not know.
        if (op == "identity")
            return IdentityInfo(ok, error);

        // Ядро › Огляд (design §4.3): the server's state and a card per module;
        // every module's files read again from disk.
        if (op == "overview")
            return Overview(ok, error);
        if (op == "reload_all")
            return ReloadAll(ok, error);
        if (op.IndexOf("mirror_set:") == 0)
            return OZ_MirrorOps.Set(op.Substring(11, op.Length() - 11), op, sender, ok, error);

        error = "STR_OZ_ERR_UNKNOWN_OP";
        return "";
    }

    private string Overview(out bool ok, out string error)
    {
        OZ_AdminOverview ov = new OZ_AdminOverview();
        array<Man> men = new array<Man>();
        GetGame().GetPlayers(men);
        ov.Players = men.Count();
        ov.MaxPlayers = GetGame().ServerConfigGetInt("maxPlayers");
        ov.Fps = OZ_ServerFps.Get();
        if (ov.Fps <= 0)
            ov.Fps = GetGame().GetFps();
        ov.Bridge = BridgeState();

        int problems = 0;
        for (int i = 0; i < OZ_AdminFacts.Count(); i++)
        {
            OZ_AdminFactsProvider p = OZ_AdminFacts.At(i);
            if (!p)
                continue;
            OZ_AdminModuleCard card = new OZ_AdminModuleCard();
            p.Fill(card);
            if (card.Id == "")
                continue;
            if (card.Problems > 0)
                problems += card.Problems;
            ov.Modules.Insert(card);
        }
        ov.Problems = problems;

        string outJson;
        string err;
        if (!JsonFileLoader<OZ_AdminOverview>.MakeData(ov, outJson, err, false))
        {
            error = "STR_OZ_ERR_INTERNAL";
            return "";
        }
        ok = true;
        return outJson;
    }

    private int BridgeState()
    {
        OZ_Settings s = OZ_Settings.Get();
        if (!s || !s.Bridge || !s.Bridge.Enabled)
            return OZ_AdminBridge.OFF;
        if (OZ_BridgeClient.Alive())
            return OZ_AdminBridge.UP;
        return OZ_AdminBridge.DOWN;
    }

    // Every module reads its files again, as at the start; each says what it did.
    private string ReloadAll(out bool ok, out string error)
    {
        OZ_AdminReloadReport rep = new OZ_AdminReloadReport();
        int done = 0;
        int problems = 0;
        for (int i = 0; i < OZ_AdminFacts.Count(); i++)
        {
            OZ_AdminFactsProvider p = OZ_AdminFacts.At(i);
            if (!p)
                continue;
            OZ_AdminReloadLine line = new OZ_AdminReloadLine();
            bool did = p.Reload(line.Said);
            OZ_AdminModuleCard card = new OZ_AdminModuleCard();
            p.Fill(card);
            if (card.Id == "")
                continue;
            line.Id = card.Id;
            line.Name = card.Name;
            line.Problems = card.Problems;
            if (did)
            {
                line.Done = 1;
                done++;
            }
            if (card.Problems > 0)
                problems += card.Problems;
            rep.Lines.Insert(line);
        }
        rep.Done = done;
        rep.Problems = problems;
        string said = "admin: every module's files read again: " + done.ToString();
        said += " modules, problems=" + problems.ToString();
        OZ_Log.Info(said);

        string outJson;
        string err;
        if (!JsonFileLoader<OZ_AdminReloadReport>.MakeData(rep, outJson, err, false))
        {
            error = "STR_OZ_ERR_INTERNAL";
            return "";
        }
        ok = true;
        return outJson;
    }

    private string IdentityInfo(out bool ok, out string error)
    {
        OZ_IdentityService svc = OZ_Identity.Get();
        OZ_AdminIdentity info = new OZ_AdminIdentity();
        info.Present = OZ_Identity.Present();
        info.Source = svc.Source();
        info.HasLeaders = svc.HasLeaders();
        info.HasPosts = svc.HasPosts();
        info.LeaderMeans = svc.LeaderMeans();

        array<string> ids;
        svc.FactionIds(ids);
        if (ids)
        {
            for (int i = 0; i < ids.Count(); i++)
            {
                string id = ids[i];
                info.Factions.Insert(id);
                info.FactionNames.Insert(svc.FactionName(id));
            }
        }

        string outJson;
        string err;
        if (!JsonFileLoader<OZ_AdminIdentity>.MakeData(info, outJson, err, false))
        {
            error = "STR_OZ_ERR_INTERNAL";
            return "";
        }
        ok = true;
        return outJson;
    }

    private string CfgList(out bool ok, out string error)
    {
        OZ_AdminCfgList l = new OZ_AdminCfgList();
        OZ_AdminCfg.Names(l.Names, l.Owners, l.Paths);

        string outJson;
        string err;
        if (!JsonFileLoader<OZ_AdminCfgList>.MakeData(l, outJson, err, false))
        {
            error = "STR_OZ_ERR_INTERNAL";
            return "";
        }

        ok = true;
        return outJson;
    }

    private string CfgGet(string name, out bool ok, out string error)
    {
        OZ_AdminCfgEntry e = OZ_AdminCfg.Find(name);
        if (!e)
        {
            error = "STR_OZ_ERR_NO_SUCH_CFG";
            return "";
        }

        // ФАЙЛА НЕМАЄ -- ЦЕ ВІДМОВА, а не порожній конфіг.
        //
        // Раніше сюди приїздив ok=true з порожнім тілом: редактор показував
        // чисте поле, і APPLY поверх нього записував порожній рядок як увесь
        // конфіг. Конфіг, зареєстрований, але ще не створений на диску, --
        // звичайний стан першого запуску, і мовчати про нього не можна.
        if (!FileExist(e.Path))
        {
            error = "STR_OZ_ERR_CFG_MISSING";
            return "";
        }

        // Сирий текст файлу, а не пересеріалізований об'єкт: адмін править
        // САМЕ ТЕ, що лежить на диску, разом із відсутніми полями й усім.
        ok = true;
        return ReadFileText(e.Path);
    }

    private string CfgSet(string name, string body, PlayerIdentity sender, out bool ok, out string error)
    {
        OZ_AdminCfgEntry e = OZ_AdminCfg.Find(name);
        if (!e || !e.Applier)
        {
            error = "STR_OZ_ERR_NO_SUCH_CFG";
            return "";
        }

        // Розбір -- ДО диска: сміття не досягає файлу, чинна версія чинна.
        // Хто саме зламався -- скаже лог; клієнтові досить «не розібралось».
        if (!e.Applier.Apply(body))
        {
            error = "STR_OZ_ERR_CFG_REJECTED";
            return "";
        }

        OZ_Log.Info("admin: config " + name + " applied by " + sender.GetPlainId());

        ok = true;
        return "{}";
    }

    // Прочитати файл як текст. FGets ріже переноси -- склеюємо назад.
    private string ReadFileText(string path)
    {
        if (!FileExist(path))
            return "";

        FileHandle f = OpenFile(path, FileMode.READ);
        if (!f)
            return "";

        array<string> parts = new array<string>();
        string line;
        while (FGets(f, line) >= 0)
            parts.Insert(line);
        CloseFile(f);

        // СКЛЕЮВАННЯ ПАРАМИ, ДЕРЕВОМ. Рядки Enforce незмінні, тож кожне `+=`
        // копіює ВЕСЬ накопичений текст, і склейка підряд квадратична: для
        // файла на 750 рядків -- близько одинадцяти мегабайт копій на одне
        // натискання RAW у вкладці VPP, для правил дослідження (~200 КБ) --
        // сотні. Масив, що стояв тут до 2026-11-09, цього не міняв: за ним
        // ішов той самий цикл `text +=`. string.Join рушія -- той самий цикл
        // (enstring.c:424). Сусіди парами, рівень за рівнем, копіюють увесь
        // текст раз на рівень: розмір на log2 рядків.
        if (parts.Count() == 0)
            return "";

        while (parts.Count() > 1)
        {
            array<string> next = new array<string>();
            int n = parts.Count();
            for (int i = 0; i + 1 < n; i += 2)
                next.Insert(parts[i] + "\n" + parts[i + 1]);
            if (n % 2 == 1)
                next.Insert(parts[n - 1]);
            parts = next;
        }
        return parts[0];
    }
}

// Аплаєр ядра: спавни. Розбір у ТИМЧАСОВИЙ об'єкт, збереження через той
// самий лоадер (він робить .bak), потім Reload -- живий конфіг
// перечитується з уже перевіреного диска повним конвеєром валідації.
//
// Аплаєра фракцій тут БІЛЬШЕ НЕМАЄ: фракції народжуються й вмирають
// тільки через бота (рішення власника 2026-08-30), i редактор файла був
// би обхідною стежкою повз це правило.
class OZ_SpawnsCfgApplier : OZ_AdminCfgApplier
{
    override bool Apply(string json)
    {
        // ОБ'ЄКТ СТВОРЮЄ СКРИПТ, а не серіалізатор: те, що виділив він, не
        // має жодного ініціалізатора полів (шапка OZ_ConfigBase).
        OZ_SpawnsConfig tmp = new OZ_SpawnsConfig();
        string err;
        if (!JsonFileLoader<OZ_SpawnsConfig>.LoadData(json, tmp, err) || !tmp)
        {
            OZ_Log.Warn("admin: Spawns.json rejected: " + err);
            return false;
        }

        // ПЕРЕВІРЯЄМО ПЕРЕД ЗАПИСОМ, І ПИШЕМО РІВНО ОДИН РАЗ.
        //
        // Було два записи: цей, із бекапом, і другий -- усередині Reload(),
        // бо Load() перезаписує файл, щойно Validate знайшов ХОЧ ОДНЕ
        // зауваження (порожній Center, від'ємний радіус -- звичайні одруківки
        // адміна). Другий запис теж робив бекап, і той затирав щойно створену
        // копію ДОправочного файла тією самою правкою. Адмін лишався без
        // undo рівно тоді, коли він потрібен.
        int warnings;
        tmp.Validate(warnings);
        tmp.Version = tmp.LatestVersion();

        OZ_ConfigLoader<OZ_SpawnsConfig>.Save(OZ_Const.PROFILE_DIR + "\\OZ_Core_Spawns.json", "spawns", tmp);
        // Тихо: файл щойно записаний і перевірений, другий бекап і другий
        // запис тут нічого не додають.
        OZ_Spawns.Reload(true);
        return true;
    }
}

// ---------------------------------------------------------------- спавни
//
// РОЗДІЛ ЯДРА, а не мода фракцій (рішення власника, ТЗ-5 §C1 R6). Досі цей
// обробник жив у @OpenZone_Factions i їздив конвертом ролей -- тобто на
// сервері без мода фракцій адмін не міг завести ЖОДНОЇ зони, хоч зони й
// файл зон лежать у ядрі, i хоч панель SPAWNS у вкладці VPP -- теж ядрова.
// Правило серії каже протилежне: будь-який мод працює, маючи одне лише ядро.
//
// Аргумент операції їде ТІЛОМ запиту рядком, а не JSON-об'єктом: він
// коротенький ("duty 30", "76561198... 25"), i конверт зі строковим полем
// різався б на 1023 байтах при серверному розборі -- та сама пастка, що й у
// cfg_get.
class OZ_SpawnSection : OZ_AdminSection
{
    override string Handle(string op, string json, PlayerIdentity sender, out bool ok, out string error)
    {
        ok = false;

        if (op == OZ_SpawnOp.UID_HERE || op == OZ_SpawnOp.UID_CLEAR)
            return Personal(op, json, sender, ok, error);

        if (op == OZ_SpawnOp.HERE || op == OZ_SpawnOp.CLEAR)
            return Zone(op, json, sender, ok, error);

        if (op == OZ_SpawnOp.ENABLE)
            return SwitchService(json, sender, ok, error);

        error = "STR_OZ_ERR_UNKNOWN_OP";
        return "";
    }

    // The service's main switch: "1" on, "0" off. Read from disk and written once,
    // like every other spawn op (OZ_Spawns.SetEnabled).
    private string SwitchService(string arg, PlayerIdentity sender, out bool ok, out string error)
    {
        string want = arg.Trim();
        if (want != "1" && want != "0")
        {
            error = "STR_OZ_ERR_UNKNOWN_OP";
            return "";
        }

        bool on = want == "1";
        string err = OZ_Spawns.SetEnabled(on);
        if (err != "")
        {
            error = err;
            return "";
        }

        string who = "?";
        if (sender)
            who = sender.GetPlainId();
        OZ_Log.Info("spawns: the service switched " + want + " by " + who);

        ok = true;
        return "{}";
    }

    // arg -- "слаг" або "слаг радіус". Радіус необов'язковий: без нього
    // двадцять метрів, бо зона в одну точку -- це купа тіл, а не табір.
    private string Zone(string op, string arg, PlayerIdentity sender, out bool ok, out string error)
    {
        string role;
        float radius;
        if (!Split(arg, role, radius, error))
            return "";

        // ПРОБІЛ -- НЕ СЛАГ.
        //
        // Слаг із пробілом попереду (« duty») різався на порожній слаг і
        // хвіст, а порожній слаг означає ЗАПАСНУ зону -- ту, куди потрапляють
        // усі, в кого нічого не збіглося. Один зайвий пробіл у команді
        // переносив спавн усього сервера, і відповідь була «готово».
        //
        // Порожній слаг писати в поле незручно, тож домовляємось: "-" означає
        // порожній, "*" -- стейджинґ (його розбирає сам OZ_Spawns).
        if (role == "-")
            role = "";

        string err;

        if (op == OZ_SpawnOp.CLEAR)
        {
            err = OZ_Spawns.ClearZone(role);
        }
        else
        {
            // Позицію беремо з ЙОГО тіла на сервері, а не з чогось, що прислав
            // клієнт: інакше зону можна було б поставити куди завгодно, не
            // сходячи з місця.
            vector here = BodyOf(sender);
            if (here == vector.Zero)
            {
                error = "STR_OZ_ERR_INTERNAL";
                return "";
            }

            err = OZ_Spawns.SetZoneHere(role, here, radius);
        }

        if (err != "")
        {
            error = err;
            return "";
        }

        ok = true;
        return "{}";
    }

    // Особиста точка гравця. Позиція -- тіло АДМІНА на сервері, як і в
    // зоні: «стань там, де його дім, і натисни».
    private string Personal(string op, string arg, PlayerIdentity sender, out bool ok, out string error)
    {
        string uid;
        float radius;
        if (!Split(arg, uid, radius, error))
            return "";

        if (uid == "")
        {
            error = "STR_OZ_ERR_NO_TARGET";
            return "";
        }

        string err;

        if (op == OZ_SpawnOp.UID_CLEAR)
        {
            err = OZ_Spawns.ClearPersonal(uid);
        }
        else
        {
            vector here = BodyOf(sender);
            if (here == vector.Zero)
            {
                error = "STR_OZ_ERR_INTERNAL";
                return "";
            }

            err = OZ_Spawns.SetPersonalHere(uid, here, radius);
        }

        if (err != "")
        {
            error = err;
            return "";
        }

        ok = true;
        return "{}";
    }

    // «слово» або «слово число». Радіус за замовчуванням -- двадцять метрів.
    private bool Split(string arg, out string head, out float radius, out string error)
    {
        // Trim() у Enforce Є (enstring.c:304) -- ручний цикл тут був
        // дванадцятьма рядками навколо рушійного виклику, а сусідній
        // рядок того самого файла вже кликав .Trim().
        string rest = arg.Trim();
        head   = rest;
        radius = 20;

        int sp = rest.IndexOf(" ");
        if (sp == -1)
        {
            head = head.Trim();
            return true;
        }

        head = rest.Substring(0, sp).Trim();

        // РАДІУС МУСИТЬ БУТИ ЧИСЛОМ.
        //
        // ToFloat() на будь-якому смітті чесно повертає нуль, і зона ставала
        // точкою: усі спавняться в одному пікселі, один в одному. Помилку
        // набору не видно ніде -- команда відповідала «готово».
        string tail = rest.Substring(sp + 1, rest.Length() - sp - 1).Trim();
        if (!Number(tail))
        {
            error = "STR_OZ_ERR_BAD_RADIUS";
            return false;
        }

        radius = tail.ToFloat();

        // НУЛЬ -- ТЕЖ ВІДМОВА, і це та сама причина, від якої захищає перевірка
        // вище. Зона нульового радіуса -- точка, у якій усі спавняться один в
        // одному; саме її обіцяв не пустити коментар про «купу тіл, а не
        // табір», а число `0` крізь Number() проходило й давало рівно це.
        if (radius <= 0)
        {
            error = "STR_OZ_ERR_BAD_RADIUS";
            return false;
        }

        return true;
    }

    // Тіло гравця НА СЕРВЕРІ -- не координата, яку прислав клієнт.
    //
    // Обхід GetPlayers() за uid живе в одному місці на все ядро
    // (OZ_Players.ManOf): тут і в OZ_Link.Online стояли дві копії того самого
    // циклу з тими самими null-перевірками.
    private vector BodyOf(PlayerIdentity who)
    {
        if (!who)
            return vector.Zero;

        Man m = OZ_Players.ManOf(who.GetPlainId());
        if (!m)
            return vector.Zero;

        return m.GetPosition();
    }

    // Чи це взагалі число. ToFloat() не вміє сказати «ні», тож питаємо самі.
    //
    // ХОЧА Б ОДНА ЦИФРА обов'язкова. Без цієї умови рядок "." проходив як
    // число: крапка дозволена, інших символів немає, цикл закінчується
    // успіхом -- а ToFloat(".") дає нуль, тобто рівно ту точкову зону, яку ця
    // перевірка й мала не пустити.
    private bool Number(string s)
    {
        if (s == "")
            return false;

        bool dot = false;
        bool digit = false;

        for (int i = 0; i < s.Length(); i++)
        {
            string c = s.Substring(i, 1);

            if (c == ".")
            {
                if (dot)
                    return false;
                dot = true;
                continue;
            }

            // Через набір, а не через порівняння рядків: у Enforce «менше»
            // для string не визначене, і покластись на нього не можна.
            if ("0123456789".IndexOf(c) == -1)
                return false;

            digit = true;
        }

        return digit;
    }
}
