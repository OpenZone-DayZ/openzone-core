// Реєстр СЛУЖБ -- третій реєстр поруч зі сторінками й адмінськими розділами,
// і третій він з тієї самої причини, з якої другий не був першим.
//
// Сторінка живе на ПРИСТРОЇ: її роздає профіль КПК, і гейт OZ_PageAccess питає
// «чи є вона в того приладу, що в руках». Розділ живе в АДМІНА: OZ_Perm.IsAdmin
// першим рядком. Служба не живе ніде з цього: настройка ручної рації й край її
// гашетки потрібні гравцеві без КПК і без прав, а що саме в нього в руках,
// знає лише той, хто рацію приніс. Тому ворота тут -- у самого обробника, а
// диспетчер (OZ_Module.OZ_SvcReq) перевіряє лише те, що спільне для всіх:
// особу з sender, стелі на частини й існування служби.
//
// До 2026-09-20 таких конвертів ядро не мало, і рація тримала сім власних
// CF-RPC -- другий транспорт поруч із ядерним. Один реєстр на всіх, і ним
// користується сам автор: точка розширення, якою не користуються, гниє.

class OZ_ServiceHandler
{
    // Повертає JSON відповіді. error -- КЛЮЧ стрінгтейбла, не готове речення:
    // текст складає клієнт, бо тільки він знає мову гравця. OZ_Const.DEFER --
    // «відповім пізніше сам», OZ_Const.NO_REPLY -- «це була подія, відповідати
    // нічого»; в обох випадках диспетчер мовчить.
    string Handle(string op, string json, PlayerIdentity sender, out bool ok, out string error)
    {
        ok = false;
        error = "STR_OZ_ERR_UNKNOWN_OP";
        return "";
    }
}

class OZ_ServiceRegistry
{
    // Контейнер -- одразу при оголошенні, як у решти реєстрів ядра.
    private static ref map<string, ref OZ_ServiceHandler> s_Services = new map<string, ref OZ_ServiceHandler>();

    // ПЕРШИЙ ВИГРАЄ, і про другого гучно кажемо (правило серії, ТЗ-5 §C1 R1).
    // Порожній обробник -- відмова в реєстрації: диспетчер бере Get() і кличе
    // Handle, і одруківка в чужому моді не має ставати NULL pointer-ом, який
    // будь-який клієнт дістає звичайним запитом.
    static void Register(string serviceId, OZ_ServiceHandler handler)
    {
        if (!handler)
        {
            OZ_Log.Error("service \"" + serviceId + "\" registered with no handler, ignored");
            return;
        }

        if (s_Services.Contains(serviceId))
        {
            OZ_Log.Warn("service \"" + serviceId + "\" registered twice, the second registration is ignored");
            return;
        }

        s_Services.Insert(serviceId, handler);
        OZ_Log.Dbg("service registered: " + serviceId);
    }

    static OZ_ServiceHandler Get(string serviceId)
    {
        return s_Services.Get(serviceId);
    }

    // Перелік служб РЯДКОМ -- для рядка готовності й для відмови «такої
    // служби немає»: ім'я каже більше за число.
    static string Describe()
    {
        string line = "";
        for (int i = 0; i < s_Services.Count(); i++)
        {
            if (line != "")
                line += ",";
            line += s_Services.GetKey(i);
        }

        if (line == "")
            return "none";
        return line;
    }
}
