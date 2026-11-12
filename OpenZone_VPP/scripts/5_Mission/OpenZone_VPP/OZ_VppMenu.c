// Вкладка «OpenZone» у VPP Admin Tools: цей pbo реєструє рівно чотири
// панелі -- SPAWNS, RAW JSON, NEWS, PLAYERS. Панель іншого мода додає СКЛЕЙКА --
// окремий pbo, що чіпляється сюди через modded class, де точки
// розширення позначені словом protected. Панель FACTIONS, наприклад,
// приїжджає з OpenZone_Factions_VPP, з репозиторію фракцій.
//
// Клієнтський UI i НІЧОГО більше: кожна дія їде на сервер АДМІНСЬКИМ
// конвертом ядра (OZ_Rpc.AdminRequest у розділ "config" чи "spawns"), i
// кожну сервер перевіряє САМ -- OZ_Perm.IsAdmin першим рядком диспетчера.
//
// Досі це був конверт СТОРІНОК на сторінку "admin", i звідти ж росли три з
// чотирьох поломок вкладки: панель фракцій адресувала свій ростер на
// "admin", де про нього не чули; за цим стояв гейт КПК, який пропускав саме
// й тільки "admin"; а роль-конверт, яким їхали спавни, обробляв мод фракцій,
// тобто без нього панель SPAWNS говорила в порожнечу.
//
// Пастки VPP (зміряно раніше, повторено i доповнено тут):
//  - обидва гарди обов'язкові (NO_GUI валить сервер, AVPPAdminTools -- то
//    ім'я класу CfgMods, не CfgPatches);
//  - super.DefineButtons(), інакше зникають рідні кнопки;
//  - перший аргумент InsertButton = ім'я права I класу підменю; право
//    реєструє сервер ядра (OZ_Perm);
//  - віджет ЖИВЕ В БАЗОВОМУ M_SUB_WIDGET: базові Show/Hide/OnUpdate
//    працюють з ним, власне поле лишає базі NULL i диаг-збірка висне;
//  - вкладені лапки в text-значенні розмітки вішають парсер НАМЕРТВО
//    (бісекція 2026-08-30);
//  - вікно живе там, де вікна самого VPP: у Panel_Content, створене
//    базовим CreateWidgets, і порядок вікон тримає VPP (SetWindowPriorty).
//    Власний SetSort(1000) на корені робочої області, що стояв тут до
//    2026-11-09, і був багом «не клікається з першого відкриття»: перше ж
//    натискання віддавало вікну SetSort(Count) -- нижче hud_root VPP (85),
//    і кліки йшли в нікуди, доки повторне відкриття не ставило 1000 знову.

#ifdef AVPPAdminTools
#ifndef NO_GUI

// Слаги фракцій ДЛЯ ПАНЕЛІ СПАВНіВ -- маленький шов, а не знання.
//
// Зона спавну може належати фракції, тож циклер мусить показати перелік. Але
// звідки той перелік узявся -- справа мода фракцій, якого може й не бути:
// тоді список порожній, циклер показує саму лише запасну зону "-", i панель
// працює далі -- слаг набирається в полі поруч.
//
// Заповнює його склейка @OpenZone_Factions_VPP, коли отримує ростер. До
// 2026-09-01 Set() не кликав НІХТО в усьому дереві, тож циклер вічно стояв
// на "-", i завести фракційну зону з вкладки не можна було взагалі.
class OZ_VppFactionSlugs
{
    // ПУБЛІЧНИЙ статик: панель фракцій кладе сюди свої слаги зі свого pbo,
    // а ядро про фракції не знає й знати не мусить.
    static ref array<string> Slugs = new array<string>();

    // Копію робимо СВОЮ: масив приходить із чужого pbo, і жити він може
    // рівно доти, доки живе його панель.
    static void Set(array<string> slugs)
    {
        Slugs.Clear();
        if (!slugs)
            return;

        for (int i = 0; i < slugs.Count(); i++)
            Slugs.Insert(slugs[i]);
    }

    // Count() І At() ТУТ БІЛЬШЕ НЕМАЄ: лічильник і захищений індекс навколо
    // масиву, який сам має і Count(), і межі. Читач у цього списку один, і він
    // за два рядки нижче.
}

modded class VPPAdminHud
{
    override void DefineButtons()
    {
        super.DefineButtons();
        InsertButton("OZ_VppAdminMenu", "OpenZone", "set:dayz_gui_vpp image:vpp_icon_xml_editor", "OpenZone: spawns, configs, news");
    }

    // VPP показує нове підменю ДО того, як вносить його в свій перелік
    // (CreateSubMenu: ShowSubMenu, потім Insert), тож SetWindowPriorty
    // зсередини ShowSubMenu його ще не знаходить, і нове вікно лишається під
    // уже відкритими -- під changelog, який VPP відкриває сам. Свій changelog
    // VPP латає одразу після CreateSubMenu (OpenChangelog); наше вікно --
    // тут, у той самий спосіб. Створює підменю лише VPPButton.OnClick, і
    // лише цим методом.
    override void CreateSubMenu(typename subMenuType)
    {
        bool fresh = (GetSubMenuByType(subMenuType) == null);
        super.CreateSubMenu(subMenuType);
        if (!fresh || subMenuType != OZ_VppAdminMenu)
            return;

        OZ_VppAdminMenu oz = OZ_VppAdminMenu.Cast(GetSubMenuByType(subMenuType));
        if (oz)
            oz.PlaceAndRaise();
    }
}

class OZ_VppAdminMenu : AdminHudSubMenu
{
    static OZ_VppAdminMenu s_Inst;

    // ------------------------------------------------- точки розширення
    protected ref array<string> m_TabIds;
    protected ref map<string, Widget> m_Panes;

    // The rail (VPP rework 2026-11-12, design §2.1): modules, each with its pages. A
    // page is a pane of m_TabIds/m_Panes; m_PageModule and m_PageLabel run beside
    // m_TabIds. A glue's pane from before the rail is a module with one page.
    protected ref array<string> m_PageModule;
    protected ref array<string> m_PageLabel;
    protected ref array<string> m_ModIds;
    protected ref array<string> m_ModLabels;
    protected ref array<int>    m_ModOpen;

    // The rows on screen now, made again by LayoutRail: "mod" or "page", and its key.
    protected ref array<Widget> m_RailRows;
    protected ref array<string> m_RailKind;
    protected ref array<string> m_RailKey;
    protected string m_Shown = "";
    protected bool   m_Sized = false;

    // The confirm strip (design §3.4): the action its "yes" sends to OnConfirmed.
    protected string m_ConfirmAction = "";

    // The journal as the status bar last showed it, and the filter of Ядро › Огляд.
    protected int  m_JournalSeen = -1;
    protected bool m_JournalErrors = false;
    protected bool m_RepaintQueued = false;

    // Ім'я віджета підказки для кожної панелі -- називає сама панель.
    protected ref map<string, string> m_PaneHints;

    // Перелік редагованих конфігів з сервера: ім'я + власник (core/pda).
    protected ref array<string> m_CfgNames;
    protected ref array<string> m_CfgOwners;

    // ЧЕРГА cfg_get: рівно один запит у польоті.
    //
    // Причина, з якої вона з'явилась, ЗНЯТА 2026-09-01: частини склеювались
    // за ключем «сторінка|операція», i два одночасних cfg_get змішували свої
    // шматки в одну кашу (зміряно 2026-08-30: Factions на три чанки не
    // розбирався, поки поруч летів другий запит). Тепер ключ -- номер
    // повідомлення, i змішатись вони не можуть.
    //
    // Черга ЛИШАЄТЬСЯ, i це не забутий код: конфіги -- десятки кілобайт, а
    // черга рівняє навантаження й тримає відповіді в передбачуваному порядку.
    // Тепер це вибір, а не обхід дефекту.
    protected ref array<string> m_CfgQ;
    protected bool m_CfgBusy = false;

    protected bool m_Ears = false;

    // Перемальовування списку кличе SelectRow, а якщо рушій відповість на
    // нього OnItemSelected -- вийде рекурсія. Прапорець рве це коло.
    protected bool m_Repaint = false;

    // ------------------------------------------------- спавни
    protected ref OZ_SpawnsConfig m_SpawnsCfg;
    protected int m_SpFacAt = 0;

    // Рядок списку -> що це таке i чим воно зветься. Без цього рядки були
    // інертні: показану особисту точку з цього екрана не було чим торкнути.
    protected ref array<string> m_SpawnRowKind;
    protected ref array<string> m_SpawnRowKey;

    // ------------------------------------------------- raw
    protected string m_RawPicked = "";
    protected ref array<string> m_RawRows;

    // ------------------------------------------------- гравці
    //
    // Рядок списку -> uid. Список показує лише ПРИСУТНІХ (сервер уміє
    // перелічити їх сам, без моста); відсутню ціль адмін набирає полем, і
    // поле перемагає -- та сама ідіома, що поверхом вище в панелі спавнів.
    protected ref array<string> m_PlrUids;

    // Пермадес -- у два натискання, як усе руйнівне у вкладці. Перше йде по
    // відповідь player_peek і НАЗИВАЄ ЛЮДИНУ; курок зводить лише вона.
    protected bool   m_PlrArmed = false;
    protected string m_PlrUid   = "";

    override void OnCreate(Widget RootW)
    {
        super.OnCreate(RootW);
        s_Inst = this;

        // БАЗОВИЙ CreateWidgets: батько -- Panel_Content VPP, як у вікон самого
        // VPP, і він же знімає m_SizeW/m_SizeH. Сорту свого немає: порядок
        // вікон тримає SetWindowPriorty (коментар на початку файлу).
        M_SUB_WIDGET = CreateWidgets("OpenZone_VPP/gui/layouts/oz_vpp_admin.layout");
        if (!M_SUB_WIDGET)
        {
            OZ_Log.Error("vpp tab: layout failed to load");
            return;
        }

        M_SUB_WIDGET.SetHandler(this);
        M_SUB_WIDGET.Show(true);

        // Шапка -- m_TitlePanel базового класу: за неї вікно тягнеться, а
        // подвійний клік розгортає його (ResizeWindow нижче). Прапорця
        // draggable генератор розмітки не знає, тож ставимо його тут.
        m_TitlePanel = M_SUB_WIDGET.FindAnyWidget("Header");
        if (m_TitlePanel)
            m_TitlePanel.SetFlags(WidgetFlags.DRAGGABLE);
        m_Grip = M_SUB_WIDGET.FindAnyWidget("Grip");
        if (m_Grip)
            m_Grip.SetFlags(WidgetFlags.DRAGGABLE);
        m_closeButton = ButtonWidget.Cast(M_SUB_WIDGET.FindAnyWidget("BtnVppClose"));

        m_TabIds  = new array<string>();
        m_PageModule = new array<string>();
        m_PageLabel  = new array<string>();
        m_ModIds     = new array<string>();
        m_ModLabels  = new array<string>();
        m_ModOpen    = new array<int>();
        m_RailRows   = new array<Widget>();
        m_RailKind   = new array<string>();
        m_RailKey    = new array<string>();
        m_Panes   = new map<string, Widget>();
        m_PaneHints = new map<string, string>();
        m_CfgNames  = new array<string>();
        m_CfgOwners = new array<string>();
        m_CfgQ      = new array<string>();
        m_RawRows = new array<string>();
        m_PlrUids = new array<string>();
        m_SpawnRowKind = new array<string>();
        m_SpawnRowKey  = new array<string>();
        m_NwVoices = new array<string>();
        m_NwPick   = 0;
        m_NwSelf   = "";
        m_MirrorKinds = new array<string>();
        m_MirrorOn    = new array<int>();
        m_MirrorNotes = new array<string>();
        m_MirrorBtns  = new array<Widget>();

        // Ядро's pages (VPP rework 2026-11-12): the journal first, the config files last.
        AddOwnPane("overview", "#STR_OZ_VPP_PAGE_OVERVIEW", "JrHint");
        AddOwnPane("spawns",   "#STR_OZ_VPP_PAGE_SPAWNS",   "SpawnHint");
        AddOwnPane("news",     "#STR_OZ_VPP_PAGE_NEWS",     "NewsHint");

        // PLAYERS -- ЯДРОВА, і саме тому вона тут (дизайн 2026-09-08).
        // Пермадес переїхав у ядро службою; без цієї панелі перенесення було
        // б наполовину -- операція є, а натиснути її нічим. Кнопка WIPE
        // лишається й у панелі фракцій: логіка одна, кнопки дві, і панель
        // фракцій уже водить ядрові операції спавнів зі свого ростера.
        AddOwnPane("players", "#STR_OZ_VPP_PAGE_PLAYERS", "PlrHint");
        AddOwnPane("raw",     "#STR_OZ_VPP_PAGE_FILES",   "RawHint");

        if (!m_Ears)
        {
            m_Ears = true;
            OZ_ClientState.AdminWatch().Insert(this.OnAdminResponse);
        }

        // Панель покаже OnMenuShow: ShowSubMenu приходить одразу після
        // OnCreate, а подвійний показ подвоював би cfg_get.
    }

    void ~OZ_VppAdminMenu()
    {
        if (s_Inst == this)
            s_Inst = null;

        if (m_Ears)
        {
            OZ_ClientState.AdminWatch().Remove(this.OnAdminResponse);
        }

        GetGame().GetCallQueue(CALL_CATEGORY_GUI).Remove(this.RepaintShown);

        if (M_SUB_WIDGET)
            M_SUB_WIDGET.Unlink();
    }

    // Вкладка: кнопка з окремої розмітки, панель -- від того, хто реєструє.
    // Субмод КПК кличе це саме з modded OnCreate.
    // ------------------------------------------------------------ NEWS
    //
    // Розділ адміна (ТЗ-6 R2.2). Список імен, якими він може підписати, дає
    // МІСТ (news_voices); панель його лише малює й по колу перебирає. Вона
    // не вирішує прав і не вигадує імен: список -- підказка, грант -- факт,
    // і маршрут запису перевіряє це ще раз (R3.2).
    // Дзеркала Discord (ТЗ-2 §8): стан приходить mirror_list, перемикання --
    // mirror_set у два натискання, бо вмикання заливає в Discord усю
    // історію, а це дія назовні.
    //
    // ПО РЯДКУ НА РІД, І РОДИ ПРИХОДЯТЬ ІЗ СЕРВЕРА (дизайн платформи §4).
    // Тут стояли два жорстких перемикачі -- BtnMirror для "chat" і
    // BtnMirrorRoles для "roles", -- обидва названі за родами ЧУЖИХ модів:
    // чат реєструє КПК, ролі -- фракції. Сервер без фракцій показував
    // тумблер для роду, якого в нього немає, а третій мод зі своїм родом не
    // з'явився б у панелі ніколи. Тепер кнопки будуються з відповіді
    // mirror_list, а вона -- з того, що моди зареєстрували.
    protected bool m_MirrorKnown = false;

    // Паралельні списки: рід, його стан, речення «що буде, як натиснути» і
    // сама кнопка. Клік шукається по кнопці, тобто рід їде разом із рядком,
    // а не зашитий в ім'я віджета.
    protected ref array<string> m_MirrorKinds;
    protected ref array<int>    m_MirrorOn;
    protected ref array<string> m_MirrorNotes;
    protected ref array<Widget> m_MirrorBtns;

    // Який рядок зведений на друге натискання. -1 -- жоден.
    protected int m_MirrorArmed = -1;

    // Ширина шаблону рядка -- з першої створеної кнопки, до того як
    // LayoutMirror її стисне. Те саме, що m_TabW для вкладок.
    protected float m_MirrorW = 0;

    protected ref array<string> m_NwVoices;
    protected int               m_NwPick;
    protected string            m_NwSelf;

    // Зазор між вкладками в одиницях розмітки -- дзеркало vpp.tabGap із
    // ui/tokens.json. Рушій того файлу не читає, тому число живе двічі:
    // змінюєш токен -- зміни й цю сталу.
    static const float TAB_GAP = 6;


    // ------------------------------------------------- вікно як у VPP
    //
    // Куток зміни розміру і де саме його схопили (від миші до кута вікна);
    // позиція й розмір до розгортання (пікселі: GetPos і GetSize віджета
    // точного розміру віддають пікселі, коментар LayoutTabs).
    protected Widget m_Grip;
    protected float m_GripDX, m_GripDY;
    protected float m_RestX, m_RestY, m_RestW, m_RestH;

    // Дзеркала токенів ui/tokens.json (рушій того файлу не читає, як і для
    // TAB_GAP): vpp.paneY -- де починаються панелі, vpp.windowX/windowY --
    // місце першого показу частками батька. MIN_W/MIN_H -- найменше вікно,
    // в одиницях розмітки: смуга з кількома вкладками і ще видно панель.
    static const float PANE_Y = 96;
    static const float WIN_X  = 0.14;
    static const float WIN_Y  = 0.08;
    static const float MIN_W  = 960;
    static const float MIN_H  = 600;

    // The window of the rework (ui/tokens.json group vpp, mirrored like the rest): the
    // title band, the rail, the status bar, a rail row, the confirm strip, the size the
    // window opens at. Layout units; times UnitScale() for pixels.
    static const float TOP     = 48;
    static const float RAIL    = 220;
    static const float STATUS  = 32;
    static const float ROW     = 32;
    static const float CONFIRM = 48;
    static const float DEF_W   = 1280;
    static const float DEF_H   = 760;


    // Своя панель -- окремою розміткою, точно як у склейок: створити під
    // коренем і зареєструвати. Ім'я файлу росте з того самого id, тож
    // розійтися їм нема де.
    protected void AddOwnPane(string id, string label, string hintName)
    {
        string layout = "OpenZone_VPP/gui/layouts/oz_vpp_pane_" + id + ".layout";

        Widget pane = GetGame().GetWorkspace().CreateWidgets(layout, M_SUB_WIDGET);
        if (!pane)
        {
            OZ_Log.Error("vpp pane: layout failed to load: " + layout);
            return;
        }

        RegisterPage("core", "#STR_OZ_VPP_MOD_CORE", id, label, pane, hintName);
    }

    // Screen pixels per layout unit: the engine's own idiom, screen height over 1080
    // (tabberui.c:48). Exact-size widgets read and take pixels (measured 2026-09-06).
    protected float UnitScale()
    {
        int scrW, scrH;
        GetScreenSize(scrW, scrH);
        float h = scrH;
        if (h <= 0)
            return 1;
        return h / 1080.0;
    }

    // A glue's pane from before the rail: a module of its own with one page. The glues
    // keep calling this until they move onto pages of their module (design §2.2).
    protected void RegisterPane(string id, string label, Widget pane, string hintName = "")
    {
        RegisterPage(id, label, id, label, pane, hintName);
    }

    // A page of a module: the rail gains the module the first time and the page under it.
    // The pane is repainted in the window's colours and moved into the content area.
    protected void RegisterPage(string module, string moduleLabel, string id, string label, Widget pane, string hintName = "")
    {
        if (!pane)
        {
            OZ_Log.Error("vpp page: no widget to register: " + id);
            return;
        }
        if (m_TabIds.Find(id) != -1)
        {
            OZ_Log.Error("vpp page: registered twice: " + id);
            return;
        }

        if (m_ModIds.Find(module) == -1)
        {
            m_ModIds.Insert(module);
            m_ModLabels.Insert(moduleLabel);
            m_ModOpen.Insert(0);
        }

        // A pane registered before under another page id is shared: it is set up once.
        bool shared = false;
        for (int k = 0; k < m_TabIds.Count(); k++)
        {
            if (m_Panes.Get(m_TabIds[k]) == pane)
                shared = true;
        }

        m_TabIds.Insert(id);
        m_PageModule.Insert(module);
        m_PageLabel.Insert(label);
        m_Panes.Set(id, pane);
        m_PaneHints.Set(id, hintName);

        if (!shared)
        {
            // A window smaller than its page cuts the page at its edge (measured 2026-11-09).
            pane.SetFlags(WidgetFlags.CLIPCHILDREN);
            OZ_VppPaint.Repaint(pane);
            PlacePane(pane);
        }
        LayoutRail();
    }

    // The content area: right of the rail, under the title band, over the status bar.
    protected void PlacePane(Widget pane)
    {
        float s = UnitScale();
        float w, h;
        M_SUB_WIDGET.GetSize(w, h);
        if (w <= 0 || h <= 0)
        {
            w = DEF_W * s;
            h = DEF_H * s;
        }
        pane.SetPos(RAIL * s + 1, TOP * s);
        pane.SetSize(w - RAIL * s - 1, h - (TOP + STATUS) * s);
    }

    protected int PagesOf(string module)
    {
        int n = 0;
        for (int i = 0; i < m_PageModule.Count(); i++)
        {
            if (m_PageModule[i] == module)
                n++;
        }
        return n;
    }

    protected string FirstPageOf(string module)
    {
        for (int i = 0; i < m_PageModule.Count(); i++)
        {
            if (m_PageModule[i] == module)
                return m_TabIds[i];
        }
        return "";
    }

    protected string ModuleOf(string page)
    {
        int i = m_TabIds.Find(page);
        if (i < 0)
            return "";
        return m_PageModule[i];
    }

    protected string ModuleLabelOf(string page)
    {
        int m = m_ModIds.Find(ModuleOf(page));
        if (m < 0)
            return "OpenZone";
        return m_ModLabels[m];
    }

    // The rail's rows, made again: few, and a module opening or closing moves the rest.
    protected void LayoutRail()
    {
        Widget rail = M_SUB_WIDGET.FindAnyWidget("Rail");
        if (!rail)
            return;

        for (int r = 0; r < m_RailRows.Count(); r++)
        {
            if (m_RailRows[r])
                m_RailRows[r].Unlink();
        }
        m_RailRows.Clear();
        m_RailKind.Clear();
        m_RailKey.Clear();

        float s = UnitScale();
        float y = 8 * s;
        for (int m = 0; m < m_ModIds.Count(); m++)
        {
            string mod = m_ModIds[m];
            int pages = PagesOf(mod);
            bool open = m_ModOpen[m] == 1;
            y = AddRailRow(rail, "mod", mod, m_ModLabels[m], pages, open, y, s);
            if (pages < 2 || !open)
                continue;
            for (int p = 0; p < m_TabIds.Count(); p++)
            {
                if (m_PageModule[p] == mod)
                    y = AddRailRow(rail, "page", m_TabIds[p], m_PageLabel[p], 0, false, y, s);
            }
        }
    }

    protected float AddRailRow(Widget rail, string kind, string key, string label, int pages, bool open, float y, float s)
    {
        Widget row = GetGame().GetWorkspace().CreateWidgets("OpenZone_VPP/gui/layouts/oz_vpp_rail_row.layout", rail);
        if (!row)
        {
            OZ_Log.Error("vpp rail: layout failed to load");
            return y;
        }

        int n = m_RailRows.Count();
        row.SetName("rail:" + n.ToString());
        row.SetPos(0, y);
        m_RailRows.Insert(row);
        m_RailKind.Insert(kind);
        m_RailKey.Insert(key);

        // The open page; a module of one page is that page. A module row is lit while it
        // holds the open page.
        bool current = false;
        bool holds = false;
        if (kind == "page")
            current = key == m_Shown;
        else
        {
            holds = ModuleOf(m_Shown) == key;
            if (pages < 2)
                current = holds;
        }

        int text = ARGBF(1, 0.88, 0.875, 0.855);
        int muted = ARGBF(1, 0.627, 0.624, 0.6);

        TextWidget t = TextWidget.Cast(row.FindAnyWidget("RailRowText"));
        if (t)
        {
            t.SetText(label);
            if (kind == "page")
                t.SetPos(30 * s, 0);
            if (current || holds)
                t.SetColor(text);
            else
                t.SetColor(muted);
        }

        TextWidget mark = TextWidget.Cast(row.FindAnyWidget("RailRowMark"));
        if (mark)
        {
            if (kind == "mod" && pages > 1 && open)
                mark.SetText("-");
            else if (kind == "mod" && pages > 1)
                mark.SetText("+");
            else
                mark.SetText("");
        }

        Widget bar = row.FindAnyWidget("RailRowBar");
        if (bar)
            bar.Show(current);
        Widget bg = row.FindAnyWidget("RailRowBg");
        if (bg && current)
            bg.SetColor(ARGBF(0.16, 0.91, 0.47, 0.17));

        return y + ROW * s;
    }

    // A module row opens its pages and shows the first, or closes them when it holds
    // the open page already; a page row shows its page.
    protected void RailClick(int n)
    {
        if (n < 0 || n >= m_RailKind.Count())
            return;

        string kind = m_RailKind[n];
        string key = m_RailKey[n];
        if (kind == "page")
        {
            ShowPane(key);
            return;
        }

        int m = m_ModIds.Find(key);
        if (m < 0)
            return;

        bool holds = ModuleOf(m_Shown) == key;
        if (PagesOf(key) > 1 && m_ModOpen[m] == 1 && holds)
        {
            m_ModOpen[m] = 0;
            LayoutRail();
            return;
        }

        m_ModOpen[m] = 1;
        if (!holds)
        {
            ShowPane(FirstPageOf(key));
            return;
        }
        LayoutRail();
    }

    protected void ShowPane(string id)
    {
        if (!m_Panes.Contains(id))
            return;

        if (m_ConfirmAction != "")
            CloseConfirm();

        m_Shown = id;

        // Several pages may share one pane (a glue that switches its own inner
        // page in OnPaneShown, as research's editor does): a pane is shown when
        // it is the shown page's pane, whichever id it was registered under.
        Widget shownPane = m_Panes.Get(id);
        for (int i = 0; i < m_TabIds.Count(); i++)
        {
            Widget pane = m_Panes.Get(m_TabIds[i]);
            if (pane && pane != shownPane)
                pane.Show(false);
        }
        if (shownPane)
            shownPane.Show(true);

        int m = m_ModIds.Find(ModuleOf(id));
        if (m != -1)
            m_ModOpen[m] = 1;

        PaintCrumb();
        LayoutRail();
        OnPaneShown(id);
        RepaintSoon();
    }

    // The title band names the open page: its module, and the page when it has several.
    protected void PaintCrumb()
    {
        TextWidget t = TextWidget.Cast(M_SUB_WIDGET.FindAnyWidget("VppCrumb"));
        if (!t)
            return;

        int i = m_TabIds.Find(m_Shown);
        if (i < 0)
        {
            t.SetText("");
            return;
        }

        string mod = Widget.TranslateString(ModuleLabelOf(m_Shown));
        if (PagesOf(m_PageModule[i]) < 2)
        {
            t.SetText(mod);
            return;
        }
        t.SetText(mod + "  /  " + Widget.TranslateString(m_PageLabel[i]));
    }

    // Панель показано -- свіжі дані. Субмод КПК довантажує своє тут.
    //
    // Гілки "factions" тут БІЛЬШЕ НЕМАЄ: ростер просить та панель, яка його
    // малює, i ядро про неї не знає. Поки цей рядок стояв тут, ядро питало
    // ростер у розділу, якого без мода фракцій не існує.
    protected void OnPaneShown(string id)
    {
        if (id == "overview")
            RebuildJournal();
        if (id == "spawns")
            AskCfg("Spawns");
        if (id == "raw")
        {
            Ask(OZ_AdminSect.CONFIG, "cfg_list", "{}");
            m_MirrorArmed = -1;
            PaintMirror();
            Ask(OZ_AdminSect.CONFIG, "mirror_list", "{}");
        }
        if (id == "news")
            Ask(OZ_AdminSect.NEWS, OZ_NewsOp.VOICES, "{}");
        if (id == "players")
        {
            m_PlrArmed = false;
            Ask(OZ_AdminSect.PLAYERS, OZ_PlayerOp.HERE, "{}");
        }
    }

    override void OnMenuShow()
    {
        super.OnMenuShow();

        // The first show lays the window out at its default size: every size is written,
        // none read back, so it holds even before the engine's first layout pass
        // (ShowSubMenu comes right after OnCreate). Later shows keep the admin's size.
        if (!m_Sized)
            ApplyWindowSize(DEF_W * UnitScale(), DEF_H * UnitScale());
        LayoutRail();
        PaintStatus();

        Ask(OZ_AdminSect.CONFIG, "cfg_list", "{}");

        // ПЕРША ЗАРЕЄСТРОВАНА, а не названа рядком.
        //
        // Тут стояло ShowPane("factions") -- панель, яку ядро не реєструє. Без
        // @OpenZone_Factions_VPP вікно відкривалось ПОРОЖНІМ: жодна панель не
        // показана, CurrentPane() повертає порожній рядок, i всі
        // повідомлення -- включно з рядком помилки -- летіли в нікуди.
        if (m_Shown != "" && m_Panes.Contains(m_Shown))
            ShowPane(m_Shown);
        else if (m_TabIds.Count() > 0)
            ShowPane(m_TabIds[0]);
    }

    // Місце першого показу -- там, де вікно стояло завжди (WIN_X / WIN_Y
    // від батька), і нагору серед вікон VPP. Кличе modded
    // VPPAdminHud.CreateSubMenu угорі файлу, раз на створення вікна.
    void PlaceAndRaise()
    {
        if (!M_SUB_WIDGET)
            return;

        Widget parent = M_SUB_WIDGET.GetParent();
        if (parent)
        {
            float pw, ph;
            parent.GetScreenSize(pw, ph);
            if (pw > 0 && ph > 0)
                M_SUB_WIDGET.SetPos(pw * WIN_X, ph * WIN_Y);
        }

        VPPAdminHud hud = GetToolbarMenu();
        if (hud)
            hud.SetWindowPriorty(this);
    }

    // Подвійний клік по шапці (базовий OnDoubleClick). Базова версія ставить
    // ДРОБИ (0.848 x 1.0) і повертає m_SizeW/m_SizeH -- обидва в розумінні
    // віджета пропорційного розміру; наш корінь точного розміру, і 0.848 у
    // ньому -- менше пікселя. Тут пікселі: розгорнути на 0.848 x 1.0 батька
    // (та сама частка, що в VPP: праворуч лишається його панель кнопок),
    // повернути те місце й той розмір, що були.
    override void ResizeWindow(bool expand)
    {
        if (!M_SUB_WIDGET)
            return;

        if (expand && !m_WindowExpanded)
        {
            Widget parent = M_SUB_WIDGET.GetParent();
            if (!parent)
                return;

            float pw, ph;
            parent.GetScreenSize(pw, ph);
            M_SUB_WIDGET.GetPos(m_RestX, m_RestY);
            M_SUB_WIDGET.GetSize(m_RestW, m_RestH);
            m_WindowExpanded = true;
            M_SUB_WIDGET.SetPos(0, 0);
            ApplyWindowSize(pw * 0.848, ph);
            return;
        }

        if (!expand && m_WindowExpanded)
        {
            m_WindowExpanded = false;
            M_SUB_WIDGET.SetPos(m_RestX, m_RestY);
            ApplyWindowSize(m_RestW, m_RestH);
        }
    }

    // The window's new size in pixels.
    //
    // NOTHING FOLLOWS THE WINDOW BY ITSELF: the layout generator resolves "fill" into
    // exact sizes when it builds (oz_vpp_admin.layout), so the ground, the border, the
    // title band, the rail and the status bar are sized from here. The close button,
    // the grip, the status bar and the confirm strip are anchored; the engine moves
    // them. Every page gets the content area; glues hear OnWindowResized.
    protected void ApplyWindowSize(float w, float h)
    {
        float s = UnitScale();
        if (w < MIN_W * s)
            w = MIN_W * s;
        if (h < MIN_H * s)
            h = MIN_H * s;

        M_SUB_WIDGET.SetSize(w, h);
        m_Sized = true;

        // Every size is written, none read back: before the engine's first layout pass
        // a widget reads as 0 (gui-layouts §24).
        float body = h - (TOP + STATUS) * s;
        float content = w - RAIL * s - 1;
        FitChrome("VppBgEdge", w + 2 * s, h + 2 * s);
        FitChrome("VppBg", w, h);
        FitChrome("VppHeader", w, TOP * s);
        FitChrome("VppHeadBg", w, TOP * s);
        FitChrome("Header", w, TOP * s);
        FitChrome("Rail", RAIL * s, body);
        FitChrome("RailEdge", 1, body);
        FitChrome("StatusBar", w, STATUS * s);
        FitChrome("ConfirmBar", content, CONFIRM * s);
        FitChrome("ConfirmLine", content, 1);

        for (int i = 0; i < m_TabIds.Count(); i++)
        {
            Widget pane = m_Panes.Get(m_TabIds[i]);
            if (!pane)
                continue;
            pane.SetPos(RAIL * s + 1, TOP * s);
            pane.SetSize(content, body);
        }

        if (m_Grip)
            m_Grip.SetPos(0, 0);

        OnWindowResized(content, body);
    }

    // Один віджет хрому -- на новий розмір у пікселях; h < 0 -- висота
    // лишається своя (шапка, смуга).
    protected void FitChrome(string name, float w, float h)
    {
        Widget c = M_SUB_WIDGET.FindAnyWidget(name);
        if (c)
            c.SetSize(w, h);
    }

    // Точка розширення для панелей склейок: вікно змінило розмір, панель
    // тепер w x h пікселів (корінь панелі вже такого розміру). Свої панелі
    // ядра розкладені генератором у точні місця, тож лишаються як були, у
    // лівому верхньому куті, а що не влазить у менше вікно, ріже
    // CLIPCHILDREN кореня панелі (RegisterPane).
    protected void OnWindowResized(float w, float h)
    {
    }

    // Куток зміни розміру. Шапку (m_TitlePanel) веде база, куток -- ми; рушій
    // тягне сам віджет за мишею, тож щоразу повертаємо його в кут.
    override bool OnDrag(Widget w, int x, int y)
    {
        if (m_Grip && w == m_Grip)
        {
            // Де саме схопили куток: від миші до правого нижнього кута вікна.
            // Без цього кут вікна стрибав під мишу на першому ж русі -- на
            // половину кутка (13 px на 3840x1600, зміряно 2026-11-09).
            float wx, wy, ww, wh;
            M_SUB_WIDGET.GetScreenPos(wx, wy);
            M_SUB_WIDGET.GetScreenSize(ww, wh);
            m_GripDX = wx + ww - x;
            m_GripDY = wy + wh - y;
            GetVPPUIManager().SetDraggingWindow(true);
            return false;
        }
        return super.OnDrag(w, x, y);
    }

    override bool OnDragging(Widget w, int x, int y, Widget reciever)
    {
        if (m_Grip && w == m_Grip)
        {
            ResizeToPointer(x, y);
            return false;
        }
        return super.OnDragging(w, x, y, reciever);
    }

    override bool OnDrop(Widget w, int x, int y, Widget reciever)
    {
        if (m_Grip && w == m_Grip)
        {
            ResizeToPointer(x, y);
            GetVPPUIManager().SetDraggingWindow(false);
            return false;
        }
        return super.OnDrop(w, x, y, reciever);
    }

    protected void ResizeToPointer(int x, int y)
    {
        float wx, wy;
        M_SUB_WIDGET.GetScreenPos(wx, wy);
        m_WindowExpanded = false;
        ApplyWindowSize(x + m_GripDX - wx, y + m_GripDY - wy);
    }

    bool IsOpen()
    {
        return M_SUB_WIDGET && M_SUB_WIDGET.IsVisible();
    }

    // Every frame while the window is open (MissionGameplay.OnUpdate below): a request
    // that waited too long is closed, and the status bar and the journal follow it.
    void Tick()
    {
        OZ_VppJournal.Expire();
        int rev = OZ_VppJournal.Revision();
        if (rev == m_JournalSeen)
            return;
        m_JournalSeen = rev;
        PaintStatus();
        if (m_Shown == "overview")
            RebuildJournal();
    }

    protected int LevelColor(int level)
    {
        if (level == OZ_VppJournal.LV_OK)
            return ARGBF(1, 0.4, 0.78, 0.36);
        if (level == OZ_VppJournal.LV_ERR)
            return ARGBF(1, 0.88, 0.32, 0.29);
        if (level == OZ_VppJournal.LV_WAIT || level == OZ_VppJournal.LV_WARN)
            return ARGBF(1, 0.96, 0.78, 0.25);
        return ARGBF(1, 0.627, 0.624, 0.6);
    }

    protected string JournalLine(OZ_VppJournalEntry e)
    {
        string line = e.Time + "   " + OZ_VppJournal.Text(e.Module);
        line += "   " + OZ_VppJournal.Text(e.What);
        if (e.Result != "")
            line += "   ->   " + OZ_VppJournal.Text(e.Result);
        return line;
    }

    protected void PaintStatus()
    {
        if (!M_SUB_WIDGET)
            return;
        TextWidget t = TextWidget.Cast(M_SUB_WIDGET.FindAnyWidget("StatusText"));
        Widget dot = M_SUB_WIDGET.FindAnyWidget("StatusDot");
        OZ_VppJournalEntry e = OZ_VppJournal.Last();
        if (!t || !e)
            return;
        int c = LevelColor(e.Level);
        t.SetText(JournalLine(e));
        t.SetColor(c);
        if (dot)
            dot.SetColor(c);
    }

    // Ядро › Огляд: the journal, newest first; "errors" keeps the failed ones only.
    protected void RebuildJournal()
    {
        TextListboxWidget lb = TextListboxWidget.Cast(M_SUB_WIDGET.FindAnyWidget("JournalList"));
        if (!lb)
            return;

        m_Repaint = true;
        lb.ClearItems();
        for (int i = OZ_VppJournal.Count() - 1; i >= 0; i--)
        {
            OZ_VppJournalEntry e = OZ_VppJournal.At(i);
            if (m_JournalErrors && e.Level != OZ_VppJournal.LV_ERR)
                continue;
            int row = lb.AddItem(JournalLine(e), NULL, 0);
            lb.SetItemColor(row, 0, LevelColor(e.Level));
        }
        m_Repaint = false;
    }

    // ---------------------------------------------------------- the confirm strip

    // A destructive action asks here, at the bottom of the page (design §3.4); "yes"
    // gives `action` to OnConfirmed. One question at a time: a new one replaces it.
    protected void Confirm(string text, string yesLabel, string action)
    {
        Widget bar = M_SUB_WIDGET.FindAnyWidget("ConfirmBar");
        if (!bar)
        {
            OZ_Log.Error("vpp confirm: the window's layout has no ConfirmBar; nothing is done");
            return;
        }

        m_ConfirmAction = action;
        TextWidget t = TextWidget.Cast(M_SUB_WIDGET.FindAnyWidget("ConfirmText"));
        if (t)
            t.SetText(text);
        TextWidget yes = TextWidget.Cast(M_SUB_WIDGET.FindAnyWidget("BtnConfirmYesText"));
        if (yes)
            yes.SetText(yesLabel);
        bar.Show(true);
    }

    protected void CloseConfirm()
    {
        m_ConfirmAction = "";
        Widget bar = M_SUB_WIDGET.FindAnyWidget("ConfirmBar");
        if (bar)
            bar.Show(false);
    }

    // The strip's "yes". A glue's layer overrides it for its own actions and calls super.
    protected void OnConfirmed(string action)
    {
        if (action.IndexOf("wipe:") == 0)
        {
            string wuid = action.Substring(5, action.Length() - 5);
            Ask(OZ_AdminSect.PLAYERS, OZ_PlayerOp.WIPE + ":" + wuid, "{}");
            Hint("wiping...");
            return;
        }

        if (action.IndexOf("mirror:") == 0)
        {
            MirrorSet(action.Substring(7, action.Length() - 7).ToInt());
            return;
        }

        if (action.IndexOf("cfg:") == 0)
        {
            string cname = action.Substring(4, action.Length() - 4);
            MultilineEditBoxWidget ed = MultilineEditBoxWidget.Cast(M_SUB_WIDGET.FindAnyWidget("RawEdit"));
            if (!ed)
                return;
            string body;
            ed.GetText(body);
            SendCfg(cname, body);
        }
    }

    // ---------------------------------------------------------- repainting

    // A pane that colours its widgets from script (a picked row, a state) paints in the
    // series palette; the open page is repainted shortly after each event and answer.
    protected void RepaintSoon()
    {
        if (m_RepaintQueued)
            return;
        m_RepaintQueued = true;
        GetGame().GetCallQueue(CALL_CATEGORY_GUI).CallLater(this.RepaintShown, 40, false);
    }

    void RepaintShown()
    {
        m_RepaintQueued = false;
        if (!M_SUB_WIDGET)
            return;
        Widget p = m_Panes.Get(m_Shown);
        if (p)
            OZ_VppPaint.Repaint(p);
    }

    // ---------------------------------------------------------- транспорт

    // Один розділ на виклик, і розділ називає ВИКЛИКАЧ. Панель фракцій просить
    // свій "factions", ядрова половина -- "config" і "spawns". Поки адреса
    // була захована тут і завжди дорівнювала "admin", склейка фракцій слала
    // ростер туди, де про нього не чули, і мовчала про це.
    protected void Ask(string sectionId, string op, string json)
    {
        // Reads (a list, a file, a peek) are asked all the time and are not news; their
        // failures still reach the journal (OZ_VppJournal.Answered).
        if (!IsReadOp(op))
            OZ_VppJournal.Asked(ModuleLabelOf(CurrentPane()), sectionId, op, sectionId + " / " + op);
        OZ_Rpc.AdminRequest(sectionId, op, json);
    }

    protected bool IsReadOp(string op)
    {
        if (op.IndexOf("cfg_get:") == 0 || op.IndexOf("player_peek:") == 0)
            return true;
        array<string> reads = {"cfg_list", "mirror_list", "players_here", "news_voices", "roster", "list", "overview"};
        return reads.Find(op) != -1;
    }

    protected void AskCfg(string name)
    {
        if (m_CfgQ.Find(name) != -1)
            return;
        m_CfgQ.Insert(name);
        PumpCfg();
    }

    protected void PumpCfg()
    {
        if (m_CfgBusy || m_CfgQ.Count() == 0)
            return;

        m_CfgBusy = true;
        Ask(OZ_AdminSect.CONFIG, "cfg_get:" + m_CfgQ[0], "{}");
    }

    protected void CfgDone(string name)
    {
        if (m_CfgQ.Count() > 0 && m_CfgQ[0] == name)
            m_CfgQ.Remove(0);
        m_CfgBusy = false;
        PumpCfg();
    }

    protected void SendCfg(string name, string body)
    {
        // Тіло їде СИРИМ: конверт зі строковим полем різався б на 1023
        // байтах при серверному розборі (та сама пастка, що й у cfg_get).
        Ask(OZ_AdminSect.CONFIG, "cfg_set:" + name, body);
    }

    // Підказка -- у рядок ТІЄЇ панелі, що на екрані.
    //
    // Ім'я віджета називає САМА панель у RegisterPane, і більше нічого тут
    // не вгадується.
    //
    // ПЕРЕЛІКУ ЧУЖИХ ІМЕН ТУТ БІЛЬШЕ НЕМАЄ (2026-09-06). Ядро тримало
    // ланцюжок SpawnHint/RawHint/FacHint/PdaHint/RadHint -- запасний хід для
    // панелей, зареєстрованих старим викликом без імені. Він же й показував,
    // чому такий перелік не працює: панель рації викидала всі свої підказки
    // рівно доти, доки її ім'я не дописали сюди руками. Ядро не мусить знати
    // імена віджетів чужих модів, і тепер не знає: усі три чужі панелі (КПК,
    // фракції, рація) передають hintName самі, а три власні -- поготів.
    //
    // Панель, яка імені не назвала, лишається без підказок, і це видно
    // одразу: мовчазний рядок замість чужого рядка з іншої панелі.
    protected void Hint(string t)
    {
        NoteHint(t);

        string id = CurrentPane();
        Widget p = m_Panes.Get(id);
        if (!p)
            return;

        string named = "";
        if (!m_PaneHints.Find(id, named) || named == "")
            return;

        TextWidget h = TextWidget.Cast(p.FindAnyWidget(named));
        if (h)
            h.SetText(t);
    }

    // A pane's line is an entry of the journal of its own (design §3.2); the same line
    // twice in a row is written once.
    protected void NoteHint(string t)
    {
        if (t == "")
            return;

        string mod = ModuleLabelOf(CurrentPane());
        int level = OZ_VppJournal.LV_INFO;
        if (t.IndexOf("#STR_") == 0 && t.IndexOf("ERR") > 0)
            level = OZ_VppJournal.LV_ERR;

        OZ_VppJournalEntry last = OZ_VppJournal.Last();
        if (last && last.What == t && last.Module == mod)
            return;
        OZ_VppJournal.Add(mod, t, "", level);
    }

    protected string CurrentPane()
    {
        // The shown page first: pages may share a pane, and the shown id is the
        // one that page's answers belong to.
        Widget shown = m_Panes.Get(m_Shown);
        if (shown && shown.IsVisible())
            return m_Shown;

        for (int i = 0; i < m_TabIds.Count(); i++)
        {
            Widget pane = m_Panes.Get(m_TabIds[i]);
            if (pane && pane.IsVisible())
                return m_TabIds[i];
        }
        return "";
    }

    // ---------------------------------------------------------- відповіді

    void OnAdminResponse(string sectionId, string op, bool ok, string json, string error)
    {
        // Every answer, any section: the journal closes its request (design §3.2).
        OZ_VppJournal.Answered(sectionId, op, ok, error);
        RepaintSoon();

        // Спавни -- ТЕЖ НАШІ, i це нове.
        //
        // Раніше відповідь на SPAWN HERE приходила рольовим конвертом, i
        // підписаний на неї був лише pbo фракцій. Без нього натискання не
        // давало ні підказки, ні помилки, ні оновлення списку: успіх був
        // невідрізненний від мовчазної відмови.
        if (sectionId == OZ_AdminSect.SPAWNS)
        {
            OnSpawnAnswer(op, ok, error);
            return;
        }

        if (sectionId == OZ_AdminSect.NEWS)
        {
            OnNewsAnswer(op, ok, json, error);
            return;
        }

        if (sectionId == OZ_AdminSect.PLAYERS)
        {
            OnPlayerAnswer(op, ok, json, error);
            return;
        }

        if (sectionId != OZ_AdminSect.CONFIG)
            return;

        if (!ok)
        {
            // Відмова на cfg_get мусить звільнити чергу, інакше вона стане.
            if (op.IndexOf("cfg_get:") == 0)
                CfgDone(op.Substring(8, op.Length() - 8));
            if (op.IndexOf("mirror_set:") == 0)
            {
                m_MirrorArmed = -1;
                PaintMirror();
            }
            Hint("#" + error);
            return;
        }

        if (op == "mirror_list")
        {
            // Корінь створює скрипт, а не серіалізатор (шапка OZ_ConfigBase):
            // тоді Mirrors на відповіді без цього поля лишається нулем, а не
            // сирою пам'яттю, і перевірка нижче має що перевіряти.
            OZ_MirrorState parsed = new OZ_MirrorState();
            string merr;
            if (JsonFileLoader<OZ_MirrorState>.LoadData(json, parsed, merr) && parsed && parsed.Mirrors)
            {
                // КОПІЯ ПЕРЕД РОЗБОРОМ (шапка OZ_ConfigBase ядра): нижче
                // будуються віджети й складаються рядки, тобто пам'ять
                // виділяється МІЖ читаннями конверта, який виділив
                // серіалізатор. Раніше цикл лише знімав два прапорці й обходився.
                OZ_MirrorState mst = parsed.Copy();

                m_MirrorKnown = true;
                BuildMirrorRows(mst);
            }
            return;
        }

        if (op.IndexOf("mirror_set:") == 0)
        {
            // Корінь створює скрипт, а не серіалізатор (шапка OZ_ConfigBase):
            // без цього Note й лічильники, яких у звіті немає, читаються з
            // сирої пам'яті ще до знімання в локальні змінні нижче.
            OZ_MirrorReport mrep = new OZ_MirrorReport();
            string rerr;
            m_MirrorArmed = -1;
            if (JsonFileLoader<OZ_MirrorReport>.LoadData(json, mrep, rerr) && mrep)
            {
                // ЗНІМАЄМО ВСЕ ВІДРАЗУ, а вже тоді складаємо рядок. Складання
                // рядка -- це виділення пам'яті, а конверт виділив
                // серіалізатор (шапка OZ_ConfigBase): читати з нього далі,
                // між двома склейками, означає читати з чужої сторінки.
                string kind    = mrep.Kind;
                bool   on      = mrep.On;
                int    pushed  = mrep.Pushed;
                int    skipped = mrep.Skipped;
                int    failed  = mrep.Failed;
                string note    = mrep.Note;

                // Рядок цього роду -- по імені, а не по гілці «ролі чи все
                // інше»: рід приходить з відповіді, і панель не мусить знати
                // жодного імені наперед.
                int row = m_MirrorKinds.Find(kind);
                if (row != -1)
                {
                    m_MirrorKnown = true;
                    if (on)
                        m_MirrorOn.Set(row, 1);
                    else
                        m_MirrorOn.Set(row, 0);
                }

                string line = kind + " mirror ";
                if (on)
                    line += "ON";
                else
                    line += "OFF";
                if (skipped == 1 && pushed == 0 && failed == 0 && note.IndexOf("already") == 0)
                    line += " (unchanged)";
                else
                    line += ": pushed " + pushed.ToString() + ", skipped " + skipped.ToString() + ", failed " + failed.ToString();
                if (note != "")
                    line += " - " + note;
                Hint(line);
            }
            PaintMirror();

            // Речення «що буде, як натиснути» складає СЕРВЕР і воно щойно
            // застаріло -- стан протилежний. Перепитуємо: дія рідка, лист
            // порожній, а підказка інакше обіцяла б те, що вже сталося.
            Ask(OZ_AdminSect.CONFIG, "mirror_list", "{}");
            return;
        }

        if (op == "cfg_list")
        {
            // Корінь створює скрипт: тоді Names лишається тим масивом, що
            // його зробив конструктор, навіть коли відповідь порожня.
            // Раніше тут стояв голий OZ_AdminCfgList l -- і Names.Count()
            // нижче падав би на відповіді без цього поля.
            OZ_AdminCfgList l = new OZ_AdminCfgList();
            string lerr;
            if (JsonFileLoader<OZ_AdminCfgList>.LoadData(json, l, lerr) && l && l.Names)
            {
                m_CfgNames.Clear();
                m_CfgOwners.Clear();
                for (int i = 0; i < l.Names.Count(); i++)
                {
                    m_CfgNames.Insert(l.Names[i]);
                    if (l.Owners && i < l.Owners.Count())
                        m_CfgOwners.Insert(l.Owners[i]);
                    else
                        m_CfgOwners.Insert("core");
                }
                RebuildRawList();
            }
            return;
        }

        if (op.IndexOf("cfg_get:") == 0)
        {
            string gname = op.Substring(8, op.Length() - 8);
            CfgDone(gname);
            OnCfgText(gname, json);
            return;
        }

        if (op.IndexOf("cfg_set:") == 0)
        {
            Hint(op.Substring(8, op.Length() - 8) + " applied");
            OnCfgApplied();
            return;
        }

    }

    // Відповідь на спавнову операцію: сказати, що вийшло, i перечитати список.
    protected void OnSpawnAnswer(string op, bool ok, string error)
    {
        if (!IsOpen())
            return;

        if (!ok)
        {
            // ВІДМОВУ ЛИШАЄМО НА ЕКРАНІ й списку не чіпаємо.
            //
            // Тут стояло перечитування «в обох випадках, щоб адмін бачив, що
            // нічого не змінилось». Ціною була сама причина: відповідь на той
            // cfg_get приходила за мить і писала в той самий рядок своє
            // «Spawns loaded», а рядок один. Зміряно на стенді: слаг "zzz"
            // мовчки не додався, i єдиним слідом операції було слово
            // «loaded». Відмова нічого не змінила -- отже й перечитувати
            // нічого.
            Hint(op + ": " + Widget.TranslateString("#" + error));
            return;
        }

        Hint(op + ": done");
        AskCfg("Spawns");
    }

    // Відповідь розділу NEWS: список імен або результат публікації.
    protected void OnNewsAnswer(string op, bool ok, string json, string error)
    {
        if (!IsOpen())
            return;

        if (!ok)
        {
            // ВІДМОВА «НЕ ТВОЯ ПЕРСОНА» НЕСЕ ПЕРЕЛІК (ТЗ-6 R1.3, приймання
            // 5.3), і панель його домальовує: сказати адмінові, що ім'я не
            // його, і не сказати, які його, -- це відповідь, після якої йдуть
            // читати вихідники. Та сама поведінка, що на сторінці КПК.
            if (error == "not_your_voice" && json != "")
            {
                // Корінь створює скрипт, а не серіалізатор (шапка
                // OZ_ConfigBase): без цього Allowed на відповіді без такого
                // поля читався б із сирої пам'яті.
                OZ_NewsAdminAnswer f = new OZ_NewsAdminAnswer();
                string ferr;
                if (JsonFileLoader<OZ_NewsAdminAnswer>.LoadData(json, f, ferr) && f && f.Allowed && f.Allowed.Count() > 0)
                {
                    // Склеюємо після розбору й один раз: кожна склейка --
                    // виділення, а конверт розібрав серіалізатор.
                    string names = f.Allowed[0];
                    for (int fi = 1; fi < f.Allowed.Count(); fi++)
                        names = names + ", " + f.Allowed[fi];

                    Hint(op + ": " + Words(error) + " - yours are: " + names);
                    return;
                }
            }

            // ЗАДОВГЕ ТІЛО -- ЧИСЛОМ МОСТА (розбіжність 96). Перевірка перед
            // надсиланням каже наше число, а це -- те, яке справді
            // застосували: якщо вони колись розійдуться, адмін побачить межу,
            // об яку насправді спіткнувся.
            if (error == "body_too_long" && json != "")
            {
                // Корінь створює скрипт, а не серіалізатор (шапка
                // OZ_ConfigBase): без цього Max на відповіді без такого поля
                // читався б із сирої пам'яті.
                OZ_NewsAdminAnswer tl = new OZ_NewsAdminAnswer();
                string terr;
                if (JsonFileLoader<OZ_NewsAdminAnswer>.LoadData(json, tl, terr) && tl && tl.Max > 0)
                {
                    Hint(op + ": the body is over the ceiling of " + tl.Max.ToString() + " b");
                    return;
                }
            }

            Hint(op + ": " + Words(error));
            return;
        }

        if (op == OZ_NewsOp.VOICES)
        {
            // Корінь створює скрипт, а не серіалізатор (шапка OZ_ConfigBase):
            // Self без цього -- сира пам'ять, а Voices мусить лишитись нулем,
            // щоб перевірка нижче мала що перевіряти.
            OZ_NewsAdminVoices v = new OZ_NewsAdminVoices();
            string verr;
            if (!JsonFileLoader<OZ_NewsAdminVoices>.LoadData(json, v, verr) || !v)
            {
                Hint("voices: unreadable answer");
                return;
            }

            // Копії не треба: обидва читання перекладають рядки з конверта
            // у власні поля вікна, і саме це переписування Є копією.
            m_NwSelf = v.Self;
            m_NwVoices.Clear();
            if (v.Voices)
            {
                for (int i = 0; i < v.Voices.Count(); i++)
                    m_NwVoices.Insert(v.Voices[i]);
            }

            // Вибір не зберігаємо між відповідями: список міг змінитись, і
            // старий індекс показував би одне ім'я, а підписував інше.
            m_NwPick = 0;
            PaintNewsWho();

            if (m_NwVoices.Count() == 0)
                Hint("no personas granted to you; posts go under your own name");
            else
                Hint(m_NwVoices.Count().ToString() + " persona(s) available");
            return;
        }

        if (op == OZ_NewsOp.POST)
        {
            // Корінь створює скрипт, а не серіалізатор (шапка OZ_ConfigBase):
            // Who їде просто в підказку адміну, і без цього на відповіді без
            // такого поля туди поїхав би шматок чужої сторінки.
            OZ_NewsAdminAnswer a = new OZ_NewsAdminAnswer();
            string aerr;
            string who = "";
            if (JsonFileLoader<OZ_NewsAdminAnswer>.LoadData(json, a, aerr) && a)
                who = a.Who;

            SetEdit("NwTitle", "");
            MultilineEditBoxWidget body = MultilineEditBoxWidget.Cast(M_SUB_WIDGET.FindAnyWidget("NwBody"));
            if (body)
                body.SetText("");

            Hint("posted as \"" + who + "\"");
            return;
        }
    }

    // Відповідь розділу PLAYERS: хто в Зоні, чий це файл, і сам пермадес.
    protected void OnPlayerAnswer(string op, bool ok, string json, string error)
    {
        bool wipeOp = op.IndexOf(OZ_PlayerOp.WIPE + ":") == 0;
        bool peekOp = op.IndexOf(OZ_PlayerOp.PEEK + ":") == 0;

        // КУРОК ГАСИМО НЕЗАЛЕЖНО ВІД ТОГО, ЩО ЗАРАЗ НА ЕКРАНІ: озброєна
        // кнопка, яка пережила відповідь, вистрелила б з наступного
        // натискання й без жодного попередження.
        if (wipeOp)
            m_PlrArmed = false;

        // ПІДКАЗКУ ПИШЕ ЛИШЕ ВІДКРИТА ПАНЕЛЬ (R-W4.5).
        //
        // OZ_ClientState.AdminWatch() -- розголос: цю саму відповідь чує й
        // панель фракцій, у якої кнопка WIPE теж є, а Hint() пише в рядок
        // ПОТОЧНОЇ вкладки. Без цієї перевірки останній записувач затирав би
        // першого, і адмін читав би чужу відповідь у своїй панелі.
        bool mine = CurrentPane() == "players";

        if (!ok)
        {
            if (peekOp)
                m_PlrArmed = false;
            if (mine)
                Hint(Words(error));
            return;
        }

        if (op == OZ_PlayerOp.HERE)
        {
            // Корінь створює скрипт, а не серіалізатор (шапка OZ_ConfigBase):
            // тоді Uids на відповіді без цього поля лишається нулем, а не
            // сирою пам'яттю, і перевірка нижче має що перевіряти.
            OZ_PlayersHere h = new OZ_PlayersHere();
            string herr;
            if (JsonFileLoader<OZ_PlayersHere>.LoadData(json, h, herr) && h && h.Uids)
                RebuildPlayerList(h);
            return;
        }

        if (peekOp)
        {
            OZ_PlayerPeeked p = new OZ_PlayerPeeked();
            string perr;
            if (!JsonFileLoader<OZ_PlayerPeeked>.LoadData(json, p, perr) || !p)
            {
                m_PlrArmed = false;
                if (mine)
                    Hint("peek: unreadable answer");
                return;
            }

            // ФАЙЛА НЕМАЄ -- КУРОК НЕ ЗВОДИТЬСЯ ВЗАГАЛІ. Це і захист від
            // одруківки в сімнадцяти цифрах, і єдине місце, де ті сімнадцять
            // цифр бачить людина.
            if (!p.Found)
            {
                m_PlrArmed = false;
                if (mine)
                    Hint("no such character on this server");
                return;
            }

            // ЗНІМАЄМО ВСЕ ВІДРАЗУ, а вже тоді складаємо рядок: складання
            // рядка -- це виділення пам'яті, а конверт розібрав серіалізатор
            // (шапка OZ_ConfigBase).
            string pname = p.Name;
            int    pgen  = p.Gen;

            if (mine)
            {
                string ask = Widget.TranslateString("#STR_OZ_VPP_WIPE_ASK") + " " + pname + " (gen " + pgen.ToString() + ")";
                Confirm(ask, "#STR_OZ_VPP_WIPE_YES", "wipe:" + m_PlrUid);
            }
            return;
        }

        if (wipeOp)
        {
            if (mine)
                Hint("wiped: the old life is frozen, a new character starts here");
            Ask(OZ_AdminSect.PLAYERS, OZ_PlayerOp.HERE, "{}");
            return;
        }
    }

    // Список присутніх: ім'я й Steam64 в одному рядку -- інакше клік по рядку
    // не сказав би адміну, чий саме uid поїхав у поле.
    protected void RebuildPlayerList(OZ_PlayersHere h)
    {
        TextListboxWidget lb = TextListboxWidget.Cast(M_SUB_WIDGET.FindAnyWidget("PlrList"));
        if (!lb)
            return;

        m_Repaint = true;

        lb.ClearItems();
        m_PlrUids.Clear();

        for (int i = 0; i < h.Uids.Count(); i++)
        {
            string nm = h.Uids[i];
            if (h.Names && i < h.Names.Count() && h.Names[i] != "")
                nm = h.Names[i];

            lb.AddItem(nm + "   " + h.Uids[i], NULL, 0);
            m_PlrUids.Insert(h.Uids[i]);
        }

        m_Repaint = false;
    }

    // Помилка -- або ключ таблиці рядків, або слова моста. Міст відмовляє
    // словами (not_your_voice, no_title), і перекладати їх нема куди: показуємо
    // як є, а ключі -- через таблицю.
    protected string Words(string error)
    {
        if (error.IndexOf("STR_") == 0)
            return Widget.TranslateString("#" + error);
        return error;
    }

    // Кнопка підпису показує поточний вибір: нуль -- своє ім'я.
    protected void PaintNewsWho()
    {
        TextWidget t = TextWidget.Cast(M_SUB_WIDGET.FindAnyWidget("BtnNwWhoText"));
        if (!t)
            return;

        if (m_NwPick <= 0 || m_NwPick > m_NwVoices.Count())
        {
            m_NwPick = 0;
            if (m_NwSelf != "")
                t.SetText(m_NwSelf + "  (myself)");
            else
                t.SetText("myself");
            return;
        }

        t.SetText(m_NwVoices[m_NwPick - 1] + "  (persona)");
    }

    protected string PickedVoice()
    {
        if (m_NwPick <= 0 || m_NwPick > m_NwVoices.Count())
            return "";
        return m_NwVoices[m_NwPick - 1];
    }

    // Текст конфігу приїхав. Субмод КПК перехоплює свої імена через super.
    protected void OnCfgText(string name, string body)
    {
        // НЕ return: той самий Spawns може бути потрібен ОБОМ панелям --
        // списку зон i сирому редактору, якщо в ньому вибрано саме його.
        // Раніше тут стояв ранній вихід, i через нього єдиний рядок списку
        // RAW JSON (а «Spawns» -- єдиний ядровий конфіг у ньому) не міг
        // наповнити поле редактора НІКОЛИ: клік перемальовував список зон,
        // поле лишалось порожнім, а APPLY слав порожній рядок як cfg_set.
        if (name == "Spawns")
        {
            // СТВОРЮЄМО САМІ Й ОДРАЗУ ПЕРЕСАДЖУЄМО ВКЛАДЕНЕ.
            //
            // Панель тримає цей об'єкт, доки адмін дивиться на список зон, і
            // читає з нього Role та Center на кожен клік -- тобто хвилинами
            // після розбору. Усе, що виділив серіалізатор, стільки не живе
            // (шапка OZ_ConfigBase); Validate тут і є та пересадка.
            OZ_SpawnsConfig sc = new OZ_SpawnsConfig();
            string serr;
            if (JsonFileLoader<OZ_SpawnsConfig>.LoadData(body, sc, serr) && sc)
            {
                int swarn;
                sc.Validate(swarn);
                m_SpawnsCfg = sc;
                RebuildSpawnList();
            }
        }

        // Сирий редактор -- окремим питанням, а не «інакше».
        if (name == m_RawPicked)
        {
            MultilineEditBoxWidget ed = MultilineEditBoxWidget.Cast(M_SUB_WIDGET.FindAnyWidget("RawEdit"));
            if (ed)
                ed.SetText(body);

            // Підказку -- ЛИШЕ коли редактор на екрані. Рядок підказки в
            // панелі один, i «Spawns loaded», написане поверх повідомлення
            // сусідньої панелі, стирає саме те, заради чого воно писалось.
            if (CurrentPane() == "raw")
                Hint(name + " loaded");
        }
    }

    // Після вдалого cfg_set перечитуємо те, що на екрані.
    protected void OnCfgApplied()
    {
        if (CurrentPane() == "spawns")
            AskCfg("Spawns");
    }

    // OnCfgListChanged ТУТ БІЛЬШЕ НЕМАЄ: обгортка в один рядок навколо
    // RebuildRawList, з одним викликачем і без жодного override у КПК,
    // фракціях чи рації.

    // ---------------------------------------------------------- спавни

    protected void RebuildSpawnList()
    {
        TextListboxWidget lb = TextListboxWidget.Cast(M_SUB_WIDGET.FindAnyWidget("SpawnList"));
        if (!lb || !m_SpawnsCfg)
            return;

        m_Repaint = true;

        lb.ClearItems();
        m_SpawnRowKind.Clear();
        m_SpawnRowKey.Clear();

        // СТЕЙДЖИНҐ -- ПЕРШИМ РЯДКОМ, i навіть коли його немає.
        //
        // Це точка найпершої появи персонажа, i досі її не було видно у
        // вкладці зовсім: задати чи зняти її можна було лише з карти КПК.
        // Рядок «staging   (not set)» відповідає на питання, яке інакше
        // вимагало б читати файл руками.
        string stg = "staging   (not set)";
        if (m_SpawnsCfg.Staging && m_SpawnsCfg.Staging.Center != "")
            stg = "staging   " + m_SpawnsCfg.Staging.Center + "   r=" + m_SpawnsCfg.Staging.Radius.ToString();
        lb.AddItem(stg, NULL, 0);
        m_SpawnRowKind.Insert("zone");
        m_SpawnRowKey.Insert("*");

        if (m_SpawnsCfg.Zones)
        {
            for (int i = 0; i < m_SpawnsCfg.Zones.Count(); i++)
            {
                OZ_SpawnZone z = m_SpawnsCfg.Zones[i];
                string slug = z.Role;
                string shown = slug;
                if (shown == "")
                    shown = "- (fallback)";
                string line = shown + "   " + z.Center + "   r=" + z.Radius.ToString();
                lb.AddItem(line, NULL, 0);
                m_SpawnRowKind.Insert("zone");
                // Порожній слаг у полі -- це «нічого не набрано», тобто
                // циклер. Пишемо "-", який сервер розуміє як порожній.
                if (slug == "")
                    slug = "-";
                m_SpawnRowKey.Insert(slug);
            }
        }

        // Особисті точки -- у тому ж списку, з міткою гравця: адмін бачить
        // УСЕ, що впливає на спавни, на одному екрані.
        if (m_SpawnsCfg.Personal)
        {
            for (int k = 0; k < m_SpawnsCfg.Personal.Count(); k++)
            {
                OZ_SpawnPersonal p = m_SpawnsCfg.Personal[k];
                string pline = "player " + p.Uid + "   " + p.Center + "   r=" + p.Radius.ToString();
                lb.AddItem(pline, NULL, 0);
                m_SpawnRowKind.Insert("uid");
                m_SpawnRowKey.Insert(p.Uid);
            }
        }

        m_Repaint = false;
    }

    protected void PaintSpawnCycler()
    {
        TextWidget t = TextWidget.Cast(M_SUB_WIDGET.FindAnyWidget("BtnSpFacText"));
        if (!t)
            return;
        t.SetText("cycler: " + SpawnSlugAt(m_SpFacAt));
    }

    // ЩО САМЕ поїде в операцію: поле сильніше за циклер.
    //
    // Циклер перелічує відоме -- організації й базові фракції з ростера. Поле
    // приймає те, чого в переліку немає й бути не може: "*" (стейджинґ), "-"
    // (запасна зона) i будь-який слаг, про який ця збірка ще не чула. Рівно
    // той самий договір, що був на карті КПК, де полем слага служило поле
    // імені мітки, -- лише тепер він живе там, де решта спавнів.
    protected string PickedSlug()
    {
        string typed = GetEdit("SpSlug");

        // Пробіли з країв: людина набирає в полі, i « duty» різалось би на
        // порожній слаг -- тобто мовчки переносило б ЗАПАСНУ зону.
        while (typed.Length() > 0 && typed.Substring(0, 1) == " ")
            typed = typed.Substring(1, typed.Length() - 1);
        while (typed.Length() > 0 && typed.Substring(typed.Length() - 1, 1) == " ")
            typed = typed.Substring(0, typed.Length() - 1);

        if (typed != "")
            return typed;

        return SpawnSlugAt(m_SpFacAt);
    }

    protected string SpawnSlugAt(int at)
    {
        // Останній пункт циклу -- запасна зона "-". Без мода фракцій перелік
        // порожній, i вона лишається єдиним варіантом.
        int n = OZ_VppFactionSlugs.Slugs.Count();
        if (n == 0 || at >= n)
            return "-";
        return OZ_VppFactionSlugs.Slugs[at];
    }

    protected void CycleSpawnFaction()
    {
        int n = OZ_VppFactionSlugs.Slugs.Count();
        m_SpFacAt = (m_SpFacAt + 1) % (n + 1);
        PaintSpawnCycler();
    }

    // ---------------------------------------------------------- raw

    protected void RebuildRawList()
    {
        TextListboxWidget lb = TextListboxWidget.Cast(M_SUB_WIDGET.FindAnyWidget("RawList"));
        if (!lb)
            return;

        lb.ClearItems();
        m_RawRows.Clear();

        // Every module's configs (design §1 item 5): the owner names the module.
        for (int i = 0; i < m_CfgNames.Count(); i++)
        {
            lb.AddItem(m_CfgOwners[i] + " / " + m_CfgNames[i], NULL, 0);
            m_RawRows.Insert(m_CfgNames[i]);
        }
    }

    // ---------------------------------------------------------- ввід

    override bool OnItemSelected(Widget w, int x, int y, int row, int column, int oldRow, int oldColumn)
    {
        if (!M_SUB_WIDGET)
            return super.OnItemSelected(w, x, y, row, column, oldRow, oldColumn);

        if (m_Repaint)
            return true;

        RepaintSoon();

        string nm = w.GetName();

        if (nm == "RawList")
        {
            if (row >= 0 && row < m_RawRows.Count())
            {
                m_RawPicked = m_RawRows[row];
                AskCfg(m_RawPicked);
            }
            return true;
        }

        // Рядок списку спавнів КЛАДЕ СЕБЕ В ПОЛЕ, а не робить нічого.
        //
        // Обробника в цього списку не було жодного, тож єдиний спосіб зняти
        // показану особисту точку був -- набрати Steam64 з екрана руками.
        // Тепер клік заповнює те поле, якого стосується рядок, i друга дія
        // (CLEAR) б'є саме туди, куди дивиться адмін.
        if (nm == "SpawnList")
        {
            if (row >= 0 && row < m_SpawnRowKind.Count())
            {
                if (m_SpawnRowKind[row] == "uid")
                {
                    SetEdit("SpUid", m_SpawnRowKey[row]);
                    Hint("player " + m_SpawnRowKey[row] + " picked");
                }
                else
                {
                    SetEdit("SpSlug", m_SpawnRowKey[row]);
                    Hint("zone \"" + m_SpawnRowKey[row] + "\" picked");
                }
            }
            return true;
        }

        // Рядок присутнього КЛАДЕ СЕБЕ В ПОЛЕ -- те саме, що робить рядок
        // списку спавнів. Курок при цьому гасне: підтвердженням для одного
        // не може стати натискання, зроблене для іншого.
        if (nm == "PlrList")
        {
            if (row >= 0 && row < m_PlrUids.Count())
            {
                m_PlrArmed = false;
                SetEdit("PlrUid", m_PlrUids[row]);
                Hint("player " + m_PlrUids[row] + " picked");
            }
            return true;
        }

        return super.OnItemSelected(w, x, y, row, column, oldRow, oldColumn);
    }

    // OnChange ТУТ БІЛЬШЕ НЕМАЄ: override, який лише кликав super, тобто
    // рівно те, що станеться й без нього.

    override bool OnClick(Widget w, int x, int y, int button)
    {
        if (!w || !M_SUB_WIDGET)
            return super.OnClick(w, x, y, button);

        if (w == m_closeButton)
            return super.OnClick(w, x, y, button);

        string nm = w.GetName();

        RepaintSoon();

        if (nm.IndexOf("rail:") == 0)
        {
            RailClick(nm.Substring(5, nm.Length() - 5).ToInt());
            return true;
        }

        if (nm == "BtnConfirmYes")
        {
            string act = m_ConfirmAction;
            CloseConfirm();
            if (act != "")
                OnConfirmed(act);
            return true;
        }

        if (nm == "BtnConfirmNo")
        {
            CloseConfirm();
            Hint("#STR_OZ_VPP_CANCELLED");
            return true;
        }

        if (nm == "BtnJrAll" || nm == "BtnJrErr")
        {
            bool errs = nm == "BtnJrErr";
            m_JournalErrors = errs;
            RebuildJournal();
            return true;
        }

        if (nm == "BtnSpFac")
        {
            CycleSpawnFaction();
            return true;
        }

        if (nm == "BtnSpHere" || nm == "BtnSpClear")
        {
            string slug = PickedSlug();
            string zarg = slug;

            if (nm == "BtnSpHere")
            {
                string zrad = GetEdit("SpRadius");
                if (zrad != "")
                    zarg += " " + zrad;
                Ask(OZ_AdminSect.SPAWNS, OZ_SpawnOp.HERE, zarg);
            }
            else
            {
                Ask(OZ_AdminSect.SPAWNS, OZ_SpawnOp.CLEAR, slug);
            }
            return true;
        }

        // ОСОБИСТА ТОЧКА -- звідси, а не з панелі фракцій.
        //
        // Вона про спавн, а не про фракцію, i мусить бути там, де решта
        // спавнів: на сервері без мода фракцій її не було де поставити
        // взагалі, хоч сам механізм -- ядровий.
        if (nm == "BtnSpUidHere" || nm == "BtnSpUidClear")
        {
            string uid = GetEdit("SpUid");
            if (uid == "")
            {
                Hint("type a Steam64 first");
                return true;
            }

            if (nm == "BtnSpUidHere")
            {
                string urad = GetEdit("SpRadius");
                string uarg = uid;
                if (urad != "")
                    uarg += " " + urad;
                Ask(OZ_AdminSect.SPAWNS, OZ_SpawnOp.UID_HERE, uarg);
            }
            else
            {
                Ask(OZ_AdminSect.SPAWNS, OZ_SpawnOp.UID_CLEAR, uid);
            }
            return true;
        }

        // ПЕРМАДЕС -- у два натискання, і перше з них НАЗИВАЄ ЛЮДИНУ.
        //
        // Поле сильніше за вибір у списку (та сама ідіома, що й PickedSlug у
        // панелі спавнів): у списку лише присутні, а стерти треба вміти й
        // відсутнього. Клік по рядку кладе його uid у це саме поле.
        if (nm == "BtnPlrWipe")
        {
            // Trim() ПОВЕРТАЄ рядок, а не чистить на місці (TrimInPlace --
            // окремий метод): пробіл із країв поля інакше зробив би з
            // сімнадцяти цифр вісімнадцять символів, і ядро відмовило б
            // «нема такого» на цілком правильний Steam64.
            string wuid = GetEdit("PlrUid").Trim();

            if (wuid == "")
            {
                Hint("type or pick a Steam64 first");
                return true;
            }

            // Курок зводить ЛИШЕ відповідь player_peek: цілі без файла гравця
            // не буває (ядро відмовить), а сімнадцять цифр з одруківкою
            // виглядають рівно як правильні.
            // The peek's answer names the person and asks on the confirm strip
            // (design §3.4); the strip's "yes" sends the wipe (OnConfirmed).
            m_PlrUid = wuid;
            Ask(OZ_AdminSect.PLAYERS, OZ_PlayerOp.PEEK + ":" + wuid, "{}");
            Hint("checking " + wuid + "...");
            return true;
        }

        if (nm == "BtnNwWho")
        {
            m_NwPick++;
            if (m_NwPick > m_NwVoices.Count())
                m_NwPick = 0;
            PaintNewsWho();
            return true;
        }

        if (nm == "BtnNwPost")
        {
            string title = GetEdit("NwTitle");
            string text  = "";
            MultilineEditBoxWidget nb = MultilineEditBoxWidget.Cast(M_SUB_WIDGET.FindAnyWidget("NwBody"));
            if (nb)
                nb.GetText(text);

            // Порожнє відхиляємо ТУТ, до мосту: він відмовив би тими ж
            // словами, але за круг через сервер, і адмін чекав би на відповідь
            // про те, що бачить сам.
            if (title.Trim() == "")
            {
                Hint("a title first");
                return true;
            }
            if (text.Trim() == "")
            {
                Hint("the body is empty");
                return true;
            }

            // Довге тіло відхиляємо ТУТ, з числом: сервер розбирає зібраний
            // JSON через JsonFileLoader, а той ріже строкове значення на
            // 1023 байтах мовчки -- тобто без цієї перевірки адмін бачив би
            // «готово» на новину, яка поїхала в гільдію обрубком.
            //
            // СТРОГО БІЛЬШЕ: рівно тисяча тут відхилялась, а на сторінці КПК
            // проходила (розбіжність 96). Останнє слово однаково за мостом --
            // він відповідає body_too_long і возить власну стелю.
            if (text.Length() > OZ_Const.NEWS_BODY_MAX)
            {
                Hint("the body is " + text.Length().ToString() + " b, the limit is " + OZ_Const.NEWS_BODY_MAX.ToString());
                return true;
            }

            OZ_NewsAdminAsk ask = new OZ_NewsAdminAsk();
            ask.Who   = PickedVoice();
            ask.Title = title;
            ask.Body  = text;

            string letter;
            string lerr;
            if (!JsonFileLoader<OZ_NewsAdminAsk>.MakeData(ask, letter, lerr, false))
            {
                Hint("cannot build the post: " + lerr);
                return true;
            }

            Hint("posting...");
            Ask(OZ_AdminSect.NEWS, OZ_NewsOp.POST, letter);
            return true;
        }

        // ДЗЕРКАЛА -- ПО ВІДЖЕТУ, А НЕ ПО ІМЕНІ. Рядки згенеровані, і всі
        // їхні кнопки звуться однаково (MirrorRowBtn): рід везе сам рядок.
        int mrow = m_MirrorBtns.Find(w);
        if (mrow != -1)
        {
            MirrorClick(mrow);
            return true;
        }

        if (nm == "BtnRawReload")
        {
            if (m_RawPicked != "")
                AskCfg(m_RawPicked);
            return true;
        }

        if (nm == "BtnRawApply")
        {
            if (m_RawPicked == "")
            {
                Hint("pick a config first");
                return true;
            }
            string applyAsk = Widget.TranslateString("#STR_OZ_VPP_APPLY_ASK") + " " + m_RawPicked;
            Confirm(applyAsk, "#STR_OZ_VPP_YES_APPLY", "cfg:" + m_RawPicked);
            return true;
        }

        // The clipboard by script, not the engine's paste: that one stops at 512 bytes
        // in an edit box (dayz-modding gui-layouts §22; design §4.2).
        if (nm == "BtnRawPaste")
        {
            string clip;
            GetGame().CopyFromClipboard(clip);
            MultilineEditBoxWidget pasteTo = MultilineEditBoxWidget.Cast(M_SUB_WIDGET.FindAnyWidget("RawEdit"));
            if (pasteTo && clip != "")
            {
                pasteTo.SetText(clip);
                Hint("pasted " + clip.Length().ToString() + " b from the clipboard");
            }
            return true;
        }

        if (nm == "BtnRawCopy")
        {
            MultilineEditBoxWidget copyFrom = MultilineEditBoxWidget.Cast(M_SUB_WIDGET.FindAnyWidget("RawEdit"));
            if (copyFrom)
            {
                string whole;
                copyFrom.GetText(whole);
                GetGame().CopyToClipboard(whole);
                Hint("copied " + whole.Length().ToString() + " b");
            }
            return true;
        }

        return super.OnClick(w, x, y, button);
    }

    // Порожня смуга з розмітки панелі -- дім згенерованих рядків. Немає її
    // (стара .layout поруч із новим скриптом) -- рядки лишаються дітьми
    // панелі: гірше на вигляд, але не порожньо. Той самий прийом, що й у
    // TabStrip().
    protected Widget MirrorStrip()
    {
        if (!M_SUB_WIDGET)
            return null;

        Widget strip = M_SUB_WIDGET.FindAnyWidget("MirrorStrip");
        if (strip)
            return strip;

        return M_SUB_WIDGET;
    }

    // Рядки будуються З ВІДПОВІДІ СЕРВЕРА, а не з розмітки: скільки родів
    // зареєстрували моди, стільки й кнопок.
    protected void BuildMirrorRows(OZ_MirrorState st)
    {
        Widget strip = MirrorStrip();
        if (!strip || !st || !st.Mirrors)
            return;

        // Старі рядки знімаємо руками: Unlink() на кожному, бо смуга живе
        // далі й наступний mirror_list інакше домалював би другий комплект.
        for (int d = 0; d < m_MirrorBtns.Count(); d++)
        {
            if (m_MirrorBtns[d])
                m_MirrorBtns[d].Unlink();
        }
        m_MirrorBtns.Clear();
        m_MirrorKinds.Clear();
        m_MirrorOn.Clear();
        m_MirrorNotes.Clear();
        m_MirrorArmed = -1;

        for (int i = 0; i < st.Mirrors.Count(); i++)
        {
            OZ_KindMirror km = st.Mirrors[i];
            if (!km || km.Kind == "")
                continue;

            Widget row = GetGame().GetWorkspace().CreateWidgets("OpenZone_VPP/gui/layouts/oz_vpp_mirror_row.layout", strip);
            if (!row)
            {
                OZ_Log.Error("vpp mirror: layout failed to load: " + km.Kind);
                continue;
            }

            if (m_MirrorW <= 0)
            {
                float rowH;
                row.GetSize(m_MirrorW, rowH);
            }

            m_MirrorKinds.Insert(km.Kind);
            if (km.Mirror)
                m_MirrorOn.Insert(1);
            else
                m_MirrorOn.Insert(0);

            if (st.Notes && i < st.Notes.Count())
                m_MirrorNotes.Insert(st.Notes[i]);
            else
                m_MirrorNotes.Insert("");

            m_MirrorBtns.Insert(row);
        }

        LayoutMirror();
        PaintMirror();
    }

    // A button and its children (edge, ground, caption) narrower -- or wider -- by one
    // number, in PIXELS: SetSize on an exact-size widget writes screen pixels (measured
    // 2026-09-03, gui-layouts §24). The tab strip had it first; the mirror rows keep it.
    private void ShrinkTab(Widget b, float width)
    {
        float w, h;
        b.GetScreenSize(w, h);
        float d = w - width;
        b.SetSize(width, h);
        Widget c = b.GetChildren();
        while (c)
        {
            c.GetScreenSize(w, h);
            c.SetSize(w - d, h);
            c = c.GetSibling();
        }
    }

    // The rows along the strip, narrower when they no longer fit: the arithmetic the
    // tab strip used, without its first-frame guard -- the strip is on screen when the
    // mirror_list answer comes.
    protected void LayoutMirror()
    {
        int n = m_MirrorBtns.Count();
        Widget strip = MirrorStrip();
        if (n == 0 || !strip)
            return;

        float room, striph;
        strip.GetScreenSize(room, striph);

        int scrW, scrH;
        GetScreenSize(scrW, scrH);
        float scrHf = scrH;

        if (room <= 0 || m_MirrorW <= 0 || scrHf <= 0)
            return;

        float s    = scrHf / 1080.0;
        float gap  = TAB_GAP * s;
        float want = m_MirrorW;
        if (want * n + gap * (n - 1) > room)
            want = (room - gap * (n - 1)) / n;

        for (int i = 0; i < n; i++)
        {
            Widget b = m_MirrorBtns[i];
            b.SetPos(i * (want + gap), 0);

            float bw, bh;
            b.GetScreenSize(bw, bh);
            if (bw > want)
                ShrinkTab(b, want);
        }
    }

    // Напис кнопки несе ВСЕ: рід, стан і зведеність. Мітки поруч більше
    // немає -- при п'яти родах на неї просто немає ширини, а «CHAT: ON»
    // читається так само, як «chat mirror: ON» читалось раніше.
    protected void PaintMirror()
    {
        for (int i = 0; i < m_MirrorBtns.Count(); i++)
        {
            Widget row = m_MirrorBtns[i];
            if (!row)
                continue;

            TextWidget t = TextWidget.Cast(row.FindAnyWidget("MirrorRowBtnText"));
            if (!t)
                continue;

            if (i == m_MirrorArmed)
            {
                t.SetText("PRESS AGAIN");
                continue;
            }

            string cap = m_MirrorKinds[i];
            cap.ToUpper();
            if (!m_MirrorKnown)
                t.SetText(cap + ": ?");
            else if (m_MirrorOn[i] == 1)
                t.SetText(cap + ": ON");
            else
                t.SetText(cap + ": OFF");
        }
    }

    // Один перемикач на рід. Перше натискання каже, що саме станеться,
    // друге робить (R5.2). Речення про наслідок складає СЕРВЕР -- панель у
    // ядрі не знає, чим чат відрізняється від ролей (дизайн платформи §4).
    protected void MirrorClick(int row)
    {
        if (row < 0 || row >= m_MirrorKinds.Count())
            return;

        string kind = m_MirrorKinds[row];
        bool   on   = m_MirrorOn[row] == 1;

        if (!m_MirrorKnown)
        {
            Hint("the mirror state has not arrived yet");
            Ask(OZ_AdminSect.CONFIG, "mirror_list", "{}");
            return;
        }

        string what = m_MirrorNotes[row];
        if (what == "")
            what = "switching the " + kind + " mirror";

        string verb = "Turn the " + kind + " mirror ON? ";
        if (on)
            verb = "Turn the " + kind + " mirror OFF? ";
        Confirm(verb + what, "#STR_OZ_VPP_YES", "mirror:" + row.ToString());
    }

    // The confirm strip's "yes" for a mirror row.
    protected void MirrorSet(int row)
    {
        if (row < 0 || row >= m_MirrorKinds.Count())
            return;

        string kind = m_MirrorKinds[row];
        bool on = m_MirrorOn[row] == 1;
        Hint("switching the " + kind + " mirror...");
        if (on)
            Ask(OZ_AdminSect.CONFIG, "mirror_set:" + kind + ":off", "{}");
        else
            Ask(OZ_AdminSect.CONFIG, "mirror_set:" + kind + ":on", "{}");
    }

    // ---------------------------------------------------------- дрібне

    protected void SetEdit(string name, string val)
    {
        EditBoxWidget e = EditBoxWidget.Cast(M_SUB_WIDGET.FindAnyWidget(name));
        if (e)
            e.SetText(val);
    }

    protected string GetEdit(string name)
    {
        EditBoxWidget e = EditBoxWidget.Cast(M_SUB_WIDGET.FindAnyWidget(name));
        if (!e)
            return "";
        return e.GetText();
    }
}

// The frame tick while the window is open: the journal's waits and the status bar
// (OZ_VppAdminMenu.Tick). It used to finish the tab strip's layout; the rail needs none.
//
// Другої роботи -- ховати вікно, коли VPP ховає панель кнопок, -- з
// 2026-11-09 немає: вікно тепер дитина кореня VPP, і ховається та
// вертається разом із ним, як вікна самого VPP. Доти воно висіло на корені
// робочої області, і сироту доводилось прибирати звідси.
modded class MissionGameplay
{
    override void OnUpdate(float timeslice)
    {
        super.OnUpdate(timeslice);

        if (!OZ_VppAdminMenu.s_Inst || !OZ_VppAdminMenu.s_Inst.IsOpen())
            return;

        OZ_VppAdminMenu.s_Inst.Tick();
    }
}

#endif
#endif
