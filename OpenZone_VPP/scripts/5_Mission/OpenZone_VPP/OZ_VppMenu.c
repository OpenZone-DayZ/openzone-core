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

// A JSON root with no fields (Ядро › Файли конфігів): reading a text into it judges the
// text's syntax alone, as the serializer skips every key a class does not have.
class OZ_VppJsonProbe
{
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

    // Ядро › Огляд: the last overview the server gave, as a copy made by script, and
    // how many module cards the page holds.
    protected ref OZ_AdminOverview m_Ov;
    static const int OV_CARDS = 9;

    // Ім'я віджета підказки для кожної панелі -- називає сама панель.
    protected ref map<string, string> m_PaneHints;

    // Перелік редагованих конфігів з сервера: ім'я + власник (core/pda).
    protected ref array<string> m_CfgNames;
    protected ref array<string> m_CfgOwners;
    protected ref array<string> m_CfgPaths;

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

    // ------------------------------------------------- Ядро › Точки появи
    protected ref OZ_SpawnsConfig m_SpawnsCfg;

    // Рядок списку -> що це таке i чим воно зветься: "head" (a group's caption),
    // "none", "zone" (key: the slug, * staging, - fallback) or "uid" (key: Steam ID).
    protected ref array<string> m_SpawnRowKind;
    protected ref array<string> m_SpawnRowKey;

    // The picked point ("zone:<slug>" or "uid:<Steam ID>"), the cycler's slug ("" is
    // its first choice) and when the scheme was last drawn (GetTime, ms).
    protected string m_SpPick;
    protected string m_SpSlugPick;
    protected int    m_SpDrawnAt;

    // What the source of factions said (identity, a read): its name and the faction
    // ids it knows, with their names side by side.
    protected string m_IdSource;
    protected ref array<string> m_IdFactions;
    protected ref array<string> m_IdNames;

    // ------------------------------------------------- raw
    protected string m_RawPicked = "";
    protected ref array<string> m_RawRows;

    // Ядро › Файли конфігів: the editor's text as last seen and when it changed (the
    // size follows at once, the JSON check once typing pauses), the text last judged
    // and the verdict, the page's own tick, the editor.
    protected string m_RawSeen;
    protected int    m_RawSeenAt;
    protected string m_RawChecked;
    protected bool   m_RawCheckedOk;
    protected string m_RawCheckedErr;
    protected int    m_RawTickAt;
    protected MultilineEditBoxWidget m_RawEditBox;

    // ------------------------------------------------- Ядро › Гравці
    //
    // The last players_here as copies made by script, the list's rows (row -> Steam ID)
    // and the card: whose it is and what the last player_peek said of that character.
    // The list holds who is in the game (the server lists them without the bridge); a
    // Steam ID typed whole in the search reaches an absent player too. The wipe asks on
    // the confirm strip and names what the peek found: seventeen digits with a typo look
    // exactly like the right ones. Fields are declared bare and set in OnCreate: every
    // glue derives from this class (dayz-modding enforce-script.md, field initialisers).
    protected ref array<string> m_PlrNames;
    protected ref array<string> m_PlrUids;
    protected ref array<string> m_PlrRowUid;
    protected int    m_PlrMax;
    protected string m_PlrPick;
    protected string m_PlrFindSeen;
    protected bool   m_PlrPeeked;
    protected bool   m_PlrFound;
    protected string m_PlrCharName;
    protected int    m_PlrCharGen;
    protected bool   m_PlrWantWipe;
    protected EditBoxWidget m_PlrFindBox;

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
        m_CfgPaths  = new array<string>();
        m_RawSeen = "";
        m_RawSeenAt = 0;
        m_RawChecked = "";
        m_RawCheckedOk = false;
        m_RawCheckedErr = "";
        m_RawTickAt = 0;
        m_CfgQ      = new array<string>();
        m_RawRows = new array<string>();
        m_PlrNames  = new array<string>();
        m_PlrUids   = new array<string>();
        m_PlrRowUid = new array<string>();
        m_PlrMax = 0;
        m_PlrPick = "";
        m_PlrFindSeen = "";
        m_PlrPeeked = false;
        m_PlrFound = false;
        m_PlrCharName = "";
        m_PlrCharGen = 0;
        m_PlrWantWipe = false;
        m_SpawnRowKind = new array<string>();
        m_SpawnRowKey  = new array<string>();
        m_SpPick = "";
        m_SpSlugPick = "";
        m_SpDrawnAt = 0;
        m_IdSource = "";
        m_IdFactions = new array<string>();
        m_IdNames = new array<string>();
        m_NwVoices = new array<string>();
        m_NwPick   = 0;
        m_NwSelf   = "";
        m_MirrorKinds = new array<string>();
        m_MirrorOn    = new array<int>();
        m_MirrorNotes = new array<string>();
        m_MirrorBtns  = new array<Widget>();

        // Ядро's pages (VPP rework 2026-11-12): the journal first, the config files last.
        AddOwnPane("overview", "#STR_OZ_VPP_PAGE_OVERVIEW", "JrHint");
        AddOwnPane("spawns",   "#STR_OZ_VPP_PAGE_SPAWNS",   "");
        AddOwnPane("news",     "#STR_OZ_VPP_PAGE_NEWS",     "");

        // PLAYERS -- ЯДРОВА, і саме тому вона тут (дизайн 2026-09-08).
        // Пермадес переїхав у ядро службою; без цієї панелі перенесення було
        // б наполовину -- операція є, а натиснути її нічим. Кнопка WIPE
        // лишається й у панелі фракцій: логіка одна, кнопки дві, і панель
        // фракцій уже водить ядрові операції спавнів зі свого ростера.
        // No hint label of its own: its lines go to the journal, and the status bar
        // shows the newest (design §3.2).
        AddOwnPane("players", "#STR_OZ_VPP_PAGE_PLAYERS", "");
        AddOwnPane("raw",     "#STR_OZ_VPP_PAGE_FILES",   "");
        m_PlrFindBox = EditBoxWidget.Cast(M_SUB_WIDGET.FindAnyWidget("PlrFind"));
        m_NwTitleBox = EditBoxWidget.Cast(M_SUB_WIDGET.FindAnyWidget("NwTitle"));
        m_RawEditBox = MultilineEditBoxWidget.Cast(M_SUB_WIDGET.FindAnyWidget("RawEdit"));
        m_NwBodyBox = MultilineEditBoxWidget.Cast(M_SUB_WIDGET.FindAnyWidget("NwBody"));
        m_NwSeenTitle = "";
        m_NwSeenBody = "";

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

    // ------------------------------------------------- Ядро › Новини
    //
    // The signatures the bridge lists for this admin (news_voices) and which one signs;
    // the title and text as the preview last showed them, and the two edit boxes.
    protected ref array<string> m_NwVoices;
    protected int               m_NwPick;
    protected string            m_NwSelf;
    protected string            m_NwSeenTitle;
    protected string            m_NwSeenBody;
    protected EditBoxWidget          m_NwTitleBox;
    protected MultilineEditBoxWidget m_NwBodyBox;

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
        {
            RebuildJournal();
            AskOverview();
        }
        if (id == "spawns")
        {
            // The file, the faction ids the source of factions knows (the cycler) and
            // who is online (names on the personal points): all three are reads.
            AskCfg("Spawns");
            Ask(OZ_AdminSect.CONFIG, "identity", "{}");
            PlrAskList();
            SpPaintSwitch();
            PaintSpawnCycler();
            SpDrawScheme();
        }
        if (id == "raw")
        {
            Ask(OZ_AdminSect.CONFIG, "cfg_list", "{}");
            RawPaintStatus(false);
            m_MirrorArmed = -1;
            PaintMirror();
            Ask(OZ_AdminSect.CONFIG, "mirror_list", "{}");
        }
        if (id == "news")
        {
            Ask(OZ_AdminSect.NEWS, OZ_NewsOp.VOICES, "{}");
            PaintNewsWho();
        }
        if (id == "players")
        {
            m_PlrWantWipe = false;
            PlrAskList();
            if (m_PlrPick != "")
                PlrPick(m_PlrPick);
            else
                PlrPaintCard();
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
        // The question takes the strip up to its buttons (16 in, 286 of buttons, 12 apart).
        FitChrome("ConfirmText", content - 314 * s, CONFIRM * s);

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
        // The open page's own frame work: the search follows typing, the scheme
        // follows the admin twice a second.
        if (m_Shown == "players")
            PlrFollowFind();
        if (m_Shown == "news")
            NwFollow();
        if (m_Shown == "raw")
            RawFollow();
        if (m_Shown == "spawns" && GetGame().GetTime() - m_SpDrawnAt >= 500)
            SpDrawScheme();

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

    // ---------------------------------------------------------- Ядро › Огляд

    // The server's state and a card per module (design §4.3). A read: not journaled.
    protected void AskOverview()
    {
        OvText("OvStat0Val", "#STR_OZ_VPP_OV_ASKING", ARGBF(1, 0.627, 0.624, 0.6));
        Ask(OZ_AdminSect.CONFIG, "overview", "{}");
    }

    protected void OnOverviewAnswer(bool ok, string json, string error)
    {
        if (!ok)
        {
            Hint("#" + error);
            return;
        }
        // Root made by the script, then read from a copy made by the script (the
        // core's OZ_ConfigBase header): nothing is read from what the serializer made.
        OZ_AdminOverview parsed = new OZ_AdminOverview();
        string err;
        if (!JsonFileLoader<OZ_AdminOverview>.LoadData(json, parsed, err) || !parsed)
        {
            Hint("#STR_OZ_ERR_INTERNAL");
            return;
        }
        m_Ov = parsed.Copy();
        PaintOverview();
    }

    protected void OnReloadAnswer(bool ok, string json, string error)
    {
        if (!ok)
        {
            Hint("#" + error);
            return;
        }
        OZ_AdminReloadReport parsed = new OZ_AdminReloadReport();
        string err;
        if (!JsonFileLoader<OZ_AdminReloadReport>.LoadData(json, parsed, err) || !parsed)
        {
            Hint("#STR_OZ_ERR_INTERNAL");
            return;
        }
        OZ_AdminReloadReport rep = parsed.Copy();

        // A journal line for each thing a module said, then the sum. Every translated
        // piece goes into a local before it meets another (two translations in one
        // expression lose one: dayz-modding mod-structure.md, strings from SCRIPT).
        for (int i = 0; i < rep.Lines.Count(); i++)
        {
            OZ_AdminReloadLine rl = rep.Lines[i];
            string modName = OvTr(rl.Name);
            for (int k = 0; k < rl.Said.Count(); k++)
            {
                string said = OvTr(rl.Said[k]);
                NoteHint(modName + ": " + said);
            }
        }
        string doneWord = OvTr("#STR_OZ_VPP_OV_RELOADED");
        string probWord = OvTr("#STR_OZ_VPP_OV_PROBLEMS_N");
        string sum = doneWord + " " + rep.Done.ToString();
        sum = sum + ", " + probWord;
        sum = sum + " " + rep.Problems.ToString();
        Hint(sum);
        AskOverview();
    }

    protected void PaintOverview()
    {
        if (!M_SUB_WIDGET || !m_Ov)
            return;

        int ink = ARGBF(1, 0.88, 0.875, 0.855);
        int inkOk = ARGBF(1, 0.4, 0.78, 0.36);
        int inkAlert = ARGBF(1, 0.96, 0.78, 0.25);
        int inkBad = ARGBF(1, 0.88, 0.32, 0.29);
        int inkMuted = ARGBF(1, 0.627, 0.624, 0.6);

        string players = m_Ov.Players.ToString();
        if (m_Ov.MaxPlayers > 0)
            players = players + " / " + m_Ov.MaxPlayers.ToString();
        OvText("OvStat0Val", players, ink);

        // The server ticks at 30; under 25 players feel it, under 15 it is unplayable
        // (dayz-modding toolchain.md, server performance).
        int fps = m_Ov.Fps;
        int fpsInk = inkOk;
        if (fps < 25)
            fpsInk = inkAlert;
        if (fps < 15)
            fpsInk = inkBad;
        OvText("OvStat1Val", fps.ToString(), fpsInk);

        string bridge = "#STR_OZ_VPP_OV_BRIDGE_OFF";
        int bridgeInk = inkMuted;
        if (m_Ov.Bridge == OZ_AdminBridge.UP)
        {
            bridge = "#STR_OZ_VPP_OV_BRIDGE_UP";
            bridgeInk = inkOk;
        }
        else if (m_Ov.Bridge == OZ_AdminBridge.DOWN)
        {
            bridge = "#STR_OZ_VPP_OV_BRIDGE_DOWN";
            bridgeInk = inkBad;
        }
        OvText("OvStat2Val", bridge, bridgeInk);

        int probInk = inkOk;
        if (m_Ov.Problems > 0)
            probInk = inkAlert;
        OvText("OvStat3Val", m_Ov.Problems.ToString(), probInk);

        if (m_Ov.Modules.Count() > OV_CARDS)
            OZ_Log.Warn("vpp overview: " + m_Ov.Modules.Count().ToString() + " modules, the page shows " + OV_CARDS.ToString());
        for (int k = 0; k < OV_CARDS; k++)
        {
            Widget card = M_SUB_WIDGET.FindAnyWidget("OvCard" + k.ToString());
            if (!card)
                continue;
            if (k >= m_Ov.Modules.Count())
            {
                card.Show(false);
                continue;
            }
            card.Show(true);
            PaintOvCard(k, m_Ov.Modules[k]);
        }
    }

    // One module's card: its name, whether it has pages here, its problems, its facts
    // on two lines (two facts on the first, the rest on the second).
    protected void PaintOvCard(int k, OZ_AdminModuleCard c)
    {
        string n = k.ToString();
        int ink = ARGBF(1, 0.88, 0.875, 0.855);
        int inkOk = ARGBF(1, 0.4, 0.78, 0.36);
        int inkAlert = ARGBF(1, 0.96, 0.78, 0.25);
        int inkMuted = ARGBF(1, 0.627, 0.624, 0.6);

        string shownName = OvTr(c.Name);
        OvText("OvCardName" + n, shownName, ink);

        if (c.Pages == 1)
            OvText("OvCardState" + n, "#STR_OZ_VPP_OV_WORKING", inkOk);
        else
            OvText("OvCardState" + n, "#STR_OZ_VPP_OV_NO_PAGES", inkMuted);

        string prob = "";
        int probInk = inkMuted;
        if (c.Problems >= 0)
        {
            string probWord = OvTr("#STR_OZ_VPP_OV_PROBLEMS_N");
            prob = probWord + " " + c.Problems.ToString();
            if (c.Problems > 0)
                probInk = inkAlert;
        }
        OvText("OvCardProb" + n, prob, probInk);

        string first = "";
        string second = "";
        for (int i = 0; i < c.Facts.Count(); i++)
        {
            OZ_AdminFact f = c.Facts[i];
            string lab = OvTr(f.Label);
            string val = OvTr(f.Value);
            string one = lab + ": ";
            one = one + val;
            if (i < 2)
            {
                if (first != "")
                    first = first + "  |  ";
                first = first + one;
            }
            else
            {
                if (second != "")
                    second = second + "  |  ";
                second = second + one;
            }
        }
        OvText("OvCardFacts" + n, first, inkMuted);
        OvText("OvCardMore" + n, second, inkMuted);
    }

    // A stringtable key translated; anything else as it is.
    protected string OvTr(string s)
    {
        if (s.IndexOf("#") != 0)
            return s;
        return Widget.TranslateString(s);
    }

    protected void OvText(string name, string text, int color)
    {
        if (!M_SUB_WIDGET)
            return;
        TextWidget t = TextWidget.Cast(M_SUB_WIDGET.FindAnyWidget(name));
        if (!t)
            return;
        t.SetText(text);
        t.SetColor(color);
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
        if (action == "reload_all")
        {
            Ask(OZ_AdminSect.CONFIG, "reload_all", "{}");
            return;
        }

        if (action.IndexOf("wipe:") == 0)
        {
            string wuid = action.Substring(5, action.Length() - 5);
            Ask(OZ_AdminSect.PLAYERS, OZ_PlayerOp.WIPE + ":" + wuid, "{}");
            Hint("#STR_OZ_VPP_PLR_WIPING");
            return;
        }

        // Ядро › Точки появи: a zone removed, a personal point removed, the switch.
        if (action.IndexOf("spclear:") == 0)
        {
            Ask(OZ_AdminSect.SPAWNS, OZ_SpawnOp.CLEAR, action.Substring(8, action.Length() - 8));
            return;
        }
        if (action.IndexOf("spuclear:") == 0)
        {
            Ask(OZ_AdminSect.SPAWNS, OZ_SpawnOp.UID_CLEAR, action.Substring(9, action.Length() - 9));
            return;
        }
        if (action.IndexOf("spon:") == 0)
        {
            Ask(OZ_AdminSect.SPAWNS, OZ_SpawnOp.ENABLE, action.Substring(5, action.Length() - 5));
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
            if (!m_RawEditBox)
                return;
            SendCfg(cname, RawText());
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
        array<string> reads = {"cfg_list", "mirror_list", "players_here", "news_voices", "roster", "list", "overview", "identity"};
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

        if (op == "overview")
        {
            OnOverviewAnswer(ok, json, error);
            return;
        }
        if (op == "reload_all")
        {
            OnReloadAnswer(ok, json, error);
            return;
        }
        if (op == "identity")
        {
            OnIdentityAnswer(ok, json);
            return;
        }

        if (!ok)
        {
            // Відмова на cfg_get мусить звільнити чергу, інакше вона стане. The
            // files page says it in place of the size line: the editor keeps what
            // it had, and APPLY would not send an empty text for a missing file.
            if (op.IndexOf("cfg_get:") == 0)
            {
                string refused = op.Substring(8, op.Length() - 8);
                CfgDone(refused);
                if (refused == m_RawPicked)
                    RawPaintHead(OZ_VppJournal.Words(error));
            }
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
                m_CfgPaths.Clear();
                for (int i = 0; i < l.Names.Count(); i++)
                {
                    m_CfgNames.Insert(l.Names[i]);
                    if (l.Owners && i < l.Owners.Count())
                        m_CfgOwners.Insert(l.Owners[i]);
                    else
                        m_CfgOwners.Insert("core");
                    if (l.Paths && i < l.Paths.Count())
                        m_CfgPaths.Insert(l.Paths[i]);
                    else
                        m_CfgPaths.Insert("");
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
            string applied = OvTr("#STR_OZ_VPP_FL_APPLIED");
            Hint(applied + " " + op.Substring(8, op.Length() - 8));
            OnCfgApplied();
            return;
        }

    }

    // Відповідь на спавнову операцію: сказати, що вийшло, i перечитати список.
    protected void OnSpawnAnswer(string op, bool ok, string error)
    {
        if (!IsOpen())
            return;

        // ВІДМОВУ ЛИШАЄМО НА ЕКРАНІ й списку не чіпаємо: a refusal changed
        // nothing, so nothing is read again, and its reason stays in the journal
        // (OZ_VppJournal.Answered closes the request with it). A line of our own
        // here would only repeat it. Success is "done" there too.
        if (!ok)
            return;

        AskCfg("Spawns");
    }

    // Відповідь розділу NEWS: список імен або результат публікації. A refusal is in the
    // journal by itself (OZ_VppJournal.Answered); the page adds only what the refusal
    // carries beyond its word.
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

                    string yours = OvTr("#STR_OZ_VPP_NW_YOURS");
                    Hint(yours + " " + names);
                }
                return;
            }

            // ЗАДОВГЕ ТІЛО -- ЧИСЛОМ МОСТА (розбіжність 96): якщо числа колись
            // розійдуться, адмін побачить межу, об яку насправді спіткнувся.
            if (error == "body_too_long" && json != "")
            {
                OZ_NewsAdminAnswer tl = new OZ_NewsAdminAnswer();
                string terr;
                if (JsonFileLoader<OZ_NewsAdminAnswer>.LoadData(json, tl, terr) && tl && tl.Max > 0)
                {
                    int ceiling = tl.Max;
                    string over = OvTr("#STR_OZ_VPP_NW_OVER");
                    string bytesWord = OvTr("#STR_OZ_VPP_NW_BYTES");
                    string line = over + " " + ceiling.ToString();
                    line = line + " " + bytesWord;
                    Hint(line);
                }
            }
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
                return;

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

            NwClear();
            string posted = OvTr("#STR_OZ_VPP_NW_POSTED");
            Hint(posted + " " + who);
            return;
        }
    }

    // ---------------------------------------------------------- Ядро › Гравці

    // Відповідь розділу PLAYERS: хто в Зоні, чий це персонаж, і сам пермадес.
    protected void OnPlayerAnswer(string op, bool ok, string json, string error)
    {
        bool wipeOp = op.IndexOf(OZ_PlayerOp.WIPE + ":") == 0;
        bool peekOp = op.IndexOf(OZ_PlayerOp.PEEK + ":") == 0;

        // ПІДКАЗКУ ПИШЕ ЛИШЕ ВІДКРИТА ПАНЕЛЬ (R-W4.5).
        //
        // OZ_ClientState.AdminWatch() -- розголос: цю саму відповідь чує й
        // панель фракцій, у якої кнопка WIPE теж є. A failure needs no line here:
        // the journal writes every failed answer by itself (OZ_VppJournal.Answered).
        bool mine = CurrentPane() == "players";

        if (peekOp)
        {
            // The answer of a peek names no player; its op does. An answer for a
            // player the card no longer shows is old news.
            int cut = OZ_PlayerOp.PEEK.Length() + 1;
            string peekUid = op.Substring(cut, op.Length() - cut);
            if (peekUid != m_PlrPick)
                return;
            PlrTakePeek(ok, json);
            return;
        }

        if (!ok)
        {
            if (wipeOp)
                m_PlrWantWipe = false;
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
                PlrTake(h);
            return;
        }

        if (wipeOp)
        {
            if (mine)
                Hint("#STR_OZ_VPP_PLR_WIPED");
            PlrAskList();

            // The card of the wiped player shows the new generation.
            int wcut = OZ_PlayerOp.WIPE.Length() + 1;
            string wiped = op.Substring(wcut, op.Length() - wcut);
            if (wiped == m_PlrPick)
                PlrPick(wiped);
            return;
        }
    }

    protected void PlrAskList()
    {
        Ask(OZ_AdminSect.PLAYERS, OZ_PlayerOp.HERE, "{}");
    }

    // The answer's arrays into the page's own, before the list is built (the core's
    // OZ_ConfigBase header): names, Steam IDs and the server's slot count.
    protected void PlrTake(OZ_PlayersHere h)
    {
        int max = h.Max;
        m_PlrNames.Clear();
        m_PlrUids.Clear();
        for (int i = 0; i < h.Uids.Count(); i++)
        {
            string uid = h.Uids[i];
            string nm = "";
            if (h.Names && i < h.Names.Count())
                nm = h.Names[i];
            m_PlrUids.Insert(uid);
            m_PlrNames.Insert(nm);
        }
        m_PlrMax = max;

        PlrRebuild();
        PlrPaintCard();

        // The spawns page names the personal points of those online.
        if (m_Shown == "spawns")
            RebuildSpawnList();
    }

    // What the peek found: the character's name and generation, or that the server
    // holds no such character. A wipe pressed before the peek came back asks now.
    protected void PlrTakePeek(bool ok, string json)
    {
        m_PlrPeeked = true;
        m_PlrFound = false;
        m_PlrCharName = "";
        m_PlrCharGen = 0;

        if (ok)
        {
            // Script-made root: Found, Name and Gen keep their initialisers when the
            // answer lacks them.
            OZ_PlayerPeeked p = new OZ_PlayerPeeked();
            string perr;
            if (JsonFileLoader<OZ_PlayerPeeked>.LoadData(json, p, perr) && p)
            {
                bool found = p.Found;
                string pname = p.Name;
                int pgen = p.Gen;
                m_PlrFound = found;
                m_PlrCharName = pname;
                m_PlrCharGen = pgen;
            }
        }

        PlrPaintCard();

        if (m_PlrWantWipe)
        {
            m_PlrWantWipe = false;
            PlrAskWipe();
        }
    }

    // The card follows this player: his peek is asked again (a read, not journaled).
    protected void PlrPick(string uid)
    {
        if (uid == "")
            return;

        m_PlrPick = uid;
        m_PlrPeeked = false;
        m_PlrFound = false;
        m_PlrCharName = "";
        m_PlrCharGen = 0;
        m_PlrWantWipe = false;

        PlrMarkRows();
        PlrPaintCard();
        Ask(OZ_AdminSect.PLAYERS, OZ_PlayerOp.PEEK + ":" + uid, "{}");
    }

    protected string PlrFindText()
    {
        if (!m_PlrFindBox)
            return "";
        return m_PlrFindBox.GetText().Trim();
    }

    // A Steam64: seventeen digits and nothing else.
    protected bool PlrIsSteamId(string s)
    {
        if (s.Length() != 17)
            return false;
        for (int i = 0; i < 17; i++)
        {
            if ("0123456789".IndexOf(s.Substring(i, 1)) == -1)
                return false;
        }
        return true;
    }

    protected bool PlrMatches(string name, string uid, string low)
    {
        string n = name;
        n.ToLower();
        bool hit = false;
        if (n.IndexOf(low) != -1)
            hit = true;
        if (uid.IndexOf(low) != -1)
            hit = true;
        return hit;
    }

    // Every frame on this page (Tick): the list follows what is typed, and seventeen
    // digits pick that Steam ID at once, in the game or not.
    protected void PlrFollowFind()
    {
        string find = PlrFindText();
        if (find == m_PlrFindSeen)
            return;
        m_PlrFindSeen = find;
        PlrRebuild();
        if (find != m_PlrPick && PlrIsSteamId(find))
            PlrPick(find);
    }

    // The list: the players the search keeps, name and Steam ID; a Steam ID typed whole
    // that is not in the game gets a row of its own on top.
    protected void PlrRebuild()
    {
        TextListboxWidget lb = TextListboxWidget.Cast(M_SUB_WIDGET.FindAnyWidget("PlrList"));
        if (!lb)
            return;

        string find = PlrFindText();
        string low = find;
        low.ToLower();
        bool away = PlrIsSteamId(find) && m_PlrUids.Find(find) == -1;

        m_Repaint = true;
        lb.ClearItems();
        m_PlrRowUid.Clear();

        if (away)
        {
            string awayWord = OvTr("#STR_OZ_VPP_PLR_OFFLINE");
            int arow = lb.AddItem(awayWord, NULL, 0);
            lb.SetItem(arow, find, NULL, 1);
            m_PlrRowUid.Insert(find);
        }

        for (int i = 0; i < m_PlrUids.Count(); i++)
        {
            string uid = m_PlrUids[i];
            string nm = m_PlrNames[i];
            if (low != "" && !PlrMatches(nm, uid, low))
                continue;
            string shown = nm;
            if (shown == "")
                shown = uid;
            int row = lb.AddItem(shown, NULL, 0);
            lb.SetItem(row, uid, NULL, 1);
            m_PlrRowUid.Insert(uid);
        }
        m_Repaint = false;

        PlrMarkRows();
        PlrPaintCount();
    }

    // A NoScrollBar list draws no highlight of its own (dayz-modding gui-layouts.md):
    // the picked row is the accent colour.
    protected void PlrMarkRows()
    {
        TextListboxWidget lb = TextListboxWidget.Cast(M_SUB_WIDGET.FindAnyWidget("PlrList"));
        if (!lb)
            return;

        int ink = ARGBF(1, 0.88, 0.875, 0.855);
        int muted = ARGBF(1, 0.627, 0.624, 0.6);
        int accent = ARGBF(1, 0.91, 0.47, 0.17);
        for (int r = 0; r < m_PlrRowUid.Count(); r++)
        {
            int c0 = ink;
            int c1 = muted;
            if (m_PlrRowUid[r] == m_PlrPick)
            {
                c0 = accent;
                c1 = accent;
            }
            lb.SetItemColor(r, 0, c0);
            lb.SetItemColor(r, 1, c1);
        }
    }

    protected void PlrPaintCount()
    {
        string head = OvTr("#STR_OZ_VPP_PLR_ONLINE");
        string line = head + " " + m_PlrUids.Count().ToString();
        if (m_PlrMax > 0)
        {
            string of = OvTr("#STR_OZ_VPP_PLR_OF");
            line = line + " " + of;
            line = line + " " + m_PlrMax.ToString();
        }
        OvText("PlrCount", line, ARGBF(1, 0.627, 0.624, 0.6));
    }

    // The card: who, whether he is in the game, and the character the server holds.
    protected void PlrPaintCard()
    {
        int ink = ARGBF(1, 0.88, 0.875, 0.855);
        int muted = ARGBF(1, 0.627, 0.624, 0.6);
        int okInk = ARGBF(1, 0.4, 0.78, 0.36);
        int badInk = ARGBF(1, 0.88, 0.32, 0.29);

        if (m_PlrPick == "")
        {
            OvText("PlrName", "#STR_OZ_VPP_PLR_NOBODY", muted);
            OvText("PlrUidShown", "", muted);
            OvText("PlrState", "", muted);
            OvText("PlrCharName", "-", muted);
            OvText("PlrGen", "-", muted);
            OvText("PlrSteam", "-", muted);
            return;
        }

        int at = m_PlrUids.Find(m_PlrPick);
        string shown = m_PlrPick;
        if (at != -1 && m_PlrNames[at] != "")
            shown = m_PlrNames[at];
        else if (m_PlrFound && m_PlrCharName != "")
            shown = m_PlrCharName;
        OvText("PlrName", shown, ink);
        OvText("PlrUidShown", m_PlrPick, muted);
        if (at != -1)
            OvText("PlrState", "#STR_OZ_VPP_PLR_ONLINE_STATE", okInk);
        else
            OvText("PlrState", "#STR_OZ_VPP_PLR_OFFLINE", muted);
        OvText("PlrSteam", m_PlrPick, ink);

        if (!m_PlrPeeked)
        {
            OvText("PlrCharName", "#STR_OZ_VPP_PLR_ASKING", muted);
            OvText("PlrGen", "-", muted);
            return;
        }
        if (!m_PlrFound)
        {
            OvText("PlrCharName", "#STR_OZ_VPP_PLR_NONE", badInk);
            OvText("PlrGen", "-", muted);
            return;
        }
        OvText("PlrCharName", m_PlrCharName, ink);
        OvText("PlrGen", m_PlrCharGen.ToString(), ink);
    }

    // The question on the confirm strip names the character the peek found; "yes"
    // sends the wipe (OnConfirmed "wipe:").
    protected void PlrAskWipe()
    {
        if (!m_PlrFound)
        {
            Hint("#STR_OZ_VPP_PLR_NO_CHAR");
            return;
        }

        string ask1 = OvTr("#STR_OZ_VPP_PLR_WIPE_ASK1");
        string genWord = OvTr("#STR_OZ_VPP_PLR_GEN_WORD");
        string ask2 = OvTr("#STR_OZ_VPP_PLR_WIPE_ASK2");
        string ask = ask1 + " " + m_PlrCharName;
        ask = ask + " (" + genWord;
        ask = ask + " " + m_PlrCharGen.ToString();
        ask = ask + ") " + ask2;
        Confirm(ask, "#STR_OZ_VPP_WIPE_YES", "wipe:" + m_PlrPick);
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

    // ---------------------------------------------------------- Ядро › Новини

    // Кнопка підпису показує поточний вибір: нуль -- своє ім'я.
    protected void PaintNewsWho()
    {
        TextWidget t = TextWidget.Cast(M_SUB_WIDGET.FindAnyWidget("BtnNwWhoText"));
        if (!t)
            return;

        if (m_NwPick <= 0 || m_NwPick > m_NwVoices.Count())
        {
            m_NwPick = 0;
            string me = OvTr("#STR_OZ_VPP_NW_ME");
            if (m_NwSelf != "")
                t.SetText(m_NwSelf + "  (" + me + ")");
            else
                t.SetText(me);
        }
        else
        {
            string persona = OvTr("#STR_OZ_VPP_NW_PERSONA");
            t.SetText(m_NwVoices[m_NwPick - 1] + "  (" + persona + ")");
        }
        NwPaint();
    }

    protected string PickedVoice()
    {
        if (m_NwPick <= 0 || m_NwPick > m_NwVoices.Count())
            return "";
        return m_NwVoices[m_NwPick - 1];
    }

    protected string NwTitleText()
    {
        if (!m_NwTitleBox)
            return "";
        return m_NwTitleBox.GetText();
    }

    protected string NwBodyText()
    {
        string text = "";
        if (m_NwBodyBox)
            m_NwBodyBox.GetText(text);
        return text;
    }

    // Every frame on this page (Tick): the preview and the byte counter follow typing.
    protected void NwFollow()
    {
        string title = NwTitleText();
        string body = NwBodyText();
        if (title == m_NwSeenTitle && body == m_NwSeenBody)
            return;
        m_NwSeenTitle = title;
        m_NwSeenBody = body;
        NwPaint();
    }

    // The byte counter against the ceiling (the bridge's, OZ_Const.NEWS_BODY_MAX: a
    // Cyrillic letter is two bytes) and the post as a PDA will show it.
    protected void NwPaint()
    {
        string title = NwTitleText();
        string body = NwBodyText();
        int bytes = body.Length();
        int max = OZ_Const.NEWS_BODY_MAX;

        int accent = ARGBF(1, 0.91, 0.47, 0.17);
        int alert = ARGBF(1, 0.96, 0.78, 0.25);
        int bad = ARGBF(1, 0.88, 0.32, 0.29);
        int ink = ARGBF(1, 0.88, 0.875, 0.855);
        int barInk = accent;
        int textInk = ink;
        if (bytes * 10 >= max * 9)
        {
            barInk = alert;
            textInk = alert;
        }
        if (bytes > max)
        {
            barInk = bad;
            textInk = bad;
        }

        string word = OvTr("#STR_OZ_VPP_NW_BYTES");
        string count = bytes.ToString() + " / " + max.ToString();
        count = count + " " + word;
        OvText("NwBytesText", count, textInk);

        Widget track = M_SUB_WIDGET.FindAnyWidget("NwBytes");
        Widget fill = M_SUB_WIDGET.FindAnyWidget("NwBytesFill");
        if (track && fill)
        {
            float tw;
            float th;
            track.GetSize(tw, th);
            float part = bytes;
            part = part / max;
            if (part > 1)
                part = 1;
            fill.SetSize(tw * part, th);
            fill.SetColor(barInk);
        }

        // The PDA's own blue (the vppPda* tokens of the layout); the texts only.
        int pdaText = ARGBF(1, 0.86, 0.9, 0.93);
        int pdaMuted = ARGBF(1, 0.55, 0.62, 0.68);
        if (title.Trim() == "")
            OvText("NwPrevTitle", "#STR_OZ_VPP_NW_PREV_TITLE", pdaMuted);
        else
            OvText("NwPrevTitle", title, pdaText);

        string who = PickedVoice();
        if (who == "")
            who = m_NwSelf;
        string now = OvTr("#STR_OZ_VPP_NW_JUST_NOW");
        string byline = who + "   " + now;
        OvText("NwPrevWho", byline, pdaMuted);

        if (body.Trim() == "")
            OvText("NwPrevBody", "#STR_OZ_VPP_NW_PREV_BODY", pdaMuted);
        else
            OvText("NwPrevBody", body, pdaText);
    }

    protected void NwClear()
    {
        if (m_NwTitleBox)
            m_NwTitleBox.SetText("");
        if (m_NwBodyBox)
            m_NwBodyBox.SetText("");
        NwPaint();
    }

    protected void NwPost()
    {
        string title = NwTitleText();
        string text = NwBodyText();

        // Порожнє відхиляємо ТУТ, до мосту: він відмовив би тими ж словами, але за
        // круг через сервер, і адмін чекав би на відповідь про те, що бачить сам.
        if (title.Trim() == "")
        {
            Hint("#STR_OZ_VPP_NW_NO_TITLE");
            return;
        }
        if (text.Trim() == "")
        {
            Hint("#STR_OZ_VPP_NW_NO_BODY");
            return;
        }

        // Довге тіло відхиляємо ТУТ, з числом: сервер розбирає зібраний JSON через
        // JsonFileLoader, а той ріже строкове значення на 1023 байтах мовчки.
        // СТРОГО БІЛЬШЕ: рівно тисяча проходить і тут, і в КПК (розбіжність 96).
        // Останнє слово однаково за мостом -- він відповідає body_too_long.
        if (text.Length() > OZ_Const.NEWS_BODY_MAX)
        {
            string over = OvTr("#STR_OZ_VPP_NW_OVER");
            string bytesWord = OvTr("#STR_OZ_VPP_NW_BYTES");
            string line = over + " " + OZ_Const.NEWS_BODY_MAX.ToString();
            line = line + " " + bytesWord;
            Hint(line);
            return;
        }

        OZ_NewsAdminAsk ask = new OZ_NewsAdminAsk();
        ask.Who   = PickedVoice();
        ask.Title = title;
        ask.Body  = text;

        string letter;
        string lerr;
        if (!JsonFileLoader<OZ_NewsAdminAsk>.MakeData(ask, letter, lerr, false))
        {
            OZ_Log.Error("vpp news: cannot build the post: " + lerr);
            return;
        }

        Ask(OZ_AdminSect.NEWS, OZ_NewsOp.POST, letter);
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
            SpTakeCfg(body);

        // Сирий редактор -- окремим питанням, а не «інакше»: той самий файл може бути
        // потрібен і сторінці, і редактору.
        if (name == m_RawPicked)
            RawTake(body);
    }

    // Після вдалого cfg_set перечитуємо те, що на екрані.
    protected void OnCfgApplied()
    {
        if (CurrentPane() == "spawns")
            AskCfg("Spawns");

        // The files page shows what the server saved.
        if (CurrentPane() == "raw" && m_RawPicked != "")
            AskCfg(m_RawPicked);
    }

    // OnCfgListChanged ТУТ БІЛЬШЕ НЕМАЄ: обгортка в один рядок навколо
    // RebuildRawList, з одним викликачем і без жодного override у КПК,
    // фракціях чи рації.

    // ---------------------------------------------------------- Ядро › Точки появи

    // СТВОРЮЄМО САМІ Й ОДРАЗУ ПЕРЕСАДЖУЄМО ВКЛАДЕНЕ.
    //
    // Панель тримає цей об'єкт, доки адмін дивиться на список зон, і читає з
    // нього Role та Center на кожен клік -- тобто хвилинами після розбору. Усе,
    // що виділив серіалізатор, стільки не живе (шапка OZ_ConfigBase); Validate
    // тут і є та пересадка.
    protected void SpTakeCfg(string body)
    {
        OZ_SpawnsConfig sc = new OZ_SpawnsConfig();
        string serr;
        if (!JsonFileLoader<OZ_SpawnsConfig>.LoadData(body, sc, serr) || !sc)
            return;

        int swarn;
        sc.Validate(swarn);
        m_SpawnsCfg = sc;

        RebuildSpawnList();
        SpPaintSwitch();
        SpDrawScheme();
    }

    // The source of factions and the ids it knows (identity, a read). The spawns
    // page offers the ids as the zones' owners.
    protected void OnIdentityAnswer(bool ok, string json)
    {
        if (!ok)
            return;

        // Root made by the script, read through a copy made by the script (the
        // core's OZ_ConfigBase header): the lists are kept.
        OZ_AdminIdentity parsed = new OZ_AdminIdentity();
        string err;
        if (!JsonFileLoader<OZ_AdminIdentity>.LoadData(json, parsed, err) || !parsed)
            return;
        OZ_AdminIdentity info = parsed.Copy();

        m_IdSource = info.Source;
        m_IdFactions.Clear();
        m_IdNames.Clear();
        for (int i = 0; i < info.Factions.Count(); i++)
        {
            string id = info.Factions[i];
            if (id == "" || m_IdFactions.Find(id) != -1)
                continue;
            string named = id;
            if (i < info.FactionNames.Count())
                named = info.FactionNames[i];
            m_IdFactions.Insert(id);
            m_IdNames.Insert(named);
        }

        PaintSpawnCycler();
        if (m_Shown == "spawns")
            RebuildSpawnList();
    }

    // The head's switch, as the last Spawns file said.
    protected void SpPaintSwitch()
    {
        TextWidget t = TextWidget.Cast(M_SUB_WIDGET.FindAnyWidget("BtnSpOnText"));
        if (!t)
            return;

        if (!m_SpawnsCfg)
        {
            t.SetText("#STR_OZ_VPP_SP_ON_UNKNOWN");
            t.SetColor(ARGBF(1, 0.627, 0.624, 0.6));
            return;
        }
        if (m_SpawnsCfg.Enabled)
        {
            t.SetText("#STR_OZ_VPP_SP_ON_YES");
            t.SetColor(ARGBF(1, 0.4, 0.78, 0.36));
            return;
        }
        t.SetText("#STR_OZ_VPP_SP_ON_NO");
        t.SetColor(ARGBF(1, 0.96, 0.78, 0.25));
    }

    // Switching the service on or off asks first: it moves where every new
    // character appears (OnConfirmed "spon:").
    protected void SpAskSwitch()
    {
        if (!m_SpawnsCfg)
        {
            AskCfg("Spawns");
            return;
        }
        if (m_SpawnsCfg.Enabled)
            Confirm("#STR_OZ_VPP_SP_OFF_ASK", "#STR_OZ_VPP_SP_YES_OFF", "spon:0");
        else
            Confirm("#STR_OZ_VPP_SP_ON_ASK", "#STR_OZ_VPP_SP_YES_ON", "spon:1");
    }

    // The cycler's choices: the faction ids the source of factions gave (identity),
    // those the factions glue put into OZ_VppFactionSlugs, then the fallback and the
    // staging zone. Without factions the fallback comes first, as it always did.
    protected void SpOptions(array<string> opts)
    {
        for (int i = 0; i < m_IdFactions.Count(); i++)
        {
            if (opts.Find(m_IdFactions[i]) == -1)
                opts.Insert(m_IdFactions[i]);
        }
        for (int k = 0; k < OZ_VppFactionSlugs.Slugs.Count(); k++)
        {
            string s = OZ_VppFactionSlugs.Slugs[k];
            if (s != "" && opts.Find(s) == -1)
                opts.Insert(s);
        }
        opts.Insert("-");
        opts.Insert("*");
    }

    // What a slug is called on this page: the faction's name beside its id, the
    // staging zone and the fallback in words.
    protected string SpWho(string slug)
    {
        if (slug == "*")
            return OvTr("#STR_OZ_VPP_SP_STAGING");
        if (slug == "-" || slug == "")
            return OvTr("#STR_OZ_VPP_SP_FALLBACK");

        int at = m_IdFactions.Find(slug);
        if (at != -1 && at < m_IdNames.Count())
        {
            string nm = m_IdNames[at];
            if (nm != "" && nm != slug)
                return slug + " - " + nm;
        }
        return slug;
    }

    // The cycler's slug: the one picked, or the first choice.
    protected string SpCyclerSlug()
    {
        if (m_SpSlugPick != "")
            return m_SpSlugPick;
        array<string> opts = new array<string>();
        SpOptions(opts);
        return opts[0];
    }

    // Кличе й склейка фракцій, коли її ростер приніс слаги (OZ_VppFactionSlugs).
    protected void PaintSpawnCycler()
    {
        TextWidget t = TextWidget.Cast(M_SUB_WIDGET.FindAnyWidget("BtnSpFacText"));
        if (!t)
            return;
        t.SetText(SpWho(SpCyclerSlug()));
    }

    protected void CycleSpawnFaction()
    {
        array<string> opts = new array<string>();
        SpOptions(opts);
        int at = opts.Find(SpCyclerSlug()) + 1;
        if (at >= opts.Count())
            at = 0;
        m_SpSlugPick = opts[at];
        SetEdit("SpSlug", "");
        PaintSpawnCycler();
    }

    // ЩО САМЕ поїде в операцію: поле сильніше за циклер. Поле приймає те, чого в
    // переліку немає: "*" (стейджинґ), "-" (запасна зона) i будь-який слаг, про
    // який ця збірка ще не чула. Trim(): « duty» інакше різалось би на порожній
    // слаг, тобто мовчки переносило б ЗАПАСНУ зону.
    protected string PickedSlug()
    {
        string typed = GetEdit("SpSlug").Trim();
        if (typed != "")
            return typed;
        return SpCyclerSlug();
    }

    // Whom the personal point is for: a Steam ID as typed, a name of someone online,
    // or the Steam ID of a point the list put into the field.
    protected string SpTargetUid()
    {
        string typed = GetEdit("SpUid").Trim();
        if (typed == "")
            return "";
        if (PlrIsSteamId(typed) || SpPersonalAt(typed) != -1)
            return typed;

        string low = typed;
        low.ToLower();
        for (int i = 0; i < m_PlrNames.Count(); i++)
        {
            string n = m_PlrNames[i];
            n.ToLower();
            if (n == low)
                return m_PlrUids[i];
        }
        return "";
    }

    protected int SpPersonalAt(string uid)
    {
        if (!m_SpawnsCfg || !m_SpawnsCfg.Personal)
            return -1;
        for (int i = 0; i < m_SpawnsCfg.Personal.Count(); i++)
        {
            OZ_SpawnPersonal p = m_SpawnsCfg.Personal[i];
            if (p && p.Uid == uid)
                return i;
        }
        return -1;
    }

    // The name of someone online, or his Steam ID.
    protected string SpPlayerName(string uid)
    {
        int at = m_PlrUids.Find(uid);
        if (at != -1 && m_PlrNames[at] != "")
            return m_PlrNames[at];
        return uid;
    }

    // "x y z" as whole metres; a centre the engine cannot read, as it is.
    protected string SpWhere(string center)
    {
        if (center == "")
            return "-";
        vector v = center.ToVector();
        if (v == vector.Zero)
            return center;
        int x = Math.Round(v[0]);
        int y = Math.Round(v[1]);
        int z = Math.Round(v[2]);
        string s = x.ToString() + " " + y.ToString();
        s = s + " " + z.ToString();
        return s;
    }

    protected string SpMetres(float r)
    {
        int m = Math.Round(r);
        return m.ToString();
    }

    // The list: a caption row per group, then its points (who, where, radius).
    protected void RebuildSpawnList()
    {
        TextListboxWidget lb = TextListboxWidget.Cast(M_SUB_WIDGET.FindAnyWidget("SpawnList"));
        if (!lb || !m_SpawnsCfg)
            return;

        m_Repaint = true;
        lb.ClearItems();
        m_SpawnRowKind.Clear();
        m_SpawnRowKey.Clear();

        // СТЕЙДЖИНҐ -- ПЕРШИМ, i навіть коли його немає: «не задано» відповідає на
        // питання, яке інакше вимагало б читати файл руками.
        SpRow(lb, "head", "", "#STR_OZ_VPP_SP_GROUP_STAGING", "", "");
        string stg = "";
        float stgR = 0;
        if (m_SpawnsCfg.Staging)
        {
            stg = m_SpawnsCfg.Staging.Center;
            stgR = m_SpawnsCfg.Staging.Radius;
        }
        if (stg != "")
        {
            string stgWho = SpWho("*");
            string stgWhere = SpWhere(stg);
            string stgRad = "r " + SpMetres(stgR);
            SpRow(lb, "zone", "*", stgWho, stgWhere, stgRad);
        }
        else
        {
            SpRow(lb, "none", "", "#STR_OZ_VPP_SP_NOT_SET", "", "");
        }

        SpRow(lb, "head", "", "#STR_OZ_VPP_SP_GROUP_ZONES", "", "");
        int zones = 0;
        if (m_SpawnsCfg.Zones)
        {
            for (int i = 0; i < m_SpawnsCfg.Zones.Count(); i++)
            {
                OZ_SpawnZone z = m_SpawnsCfg.Zones[i];
                if (!z)
                    continue;
                // Порожній слаг -- запасна зона; у полі й операції вона "-".
                string slug = z.Role;
                if (slug == "")
                    slug = "-";
                string zWho = SpWho(slug);
                string zWhere = SpWhere(z.Center);
                string zRad = "r " + SpMetres(z.Radius);
                SpRow(lb, "zone", slug, zWho, zWhere, zRad);
                zones++;
            }
        }
        if (zones == 0)
            SpRow(lb, "none", "", "#STR_OZ_VPP_SP_NONE", "", "");

        // Особисті точки -- у тому ж списку: адмін бачить УСЕ, що впливає на
        // спавни, на одному екрані.
        SpRow(lb, "head", "", "#STR_OZ_VPP_SP_GROUP_PERSONAL", "", "");
        int pers = 0;
        if (m_SpawnsCfg.Personal)
        {
            for (int k = 0; k < m_SpawnsCfg.Personal.Count(); k++)
            {
                OZ_SpawnPersonal p = m_SpawnsCfg.Personal[k];
                if (!p)
                    continue;
                string pWho = SpPlayerName(p.Uid);
                string pWhere = SpWhere(p.Center);
                string pRad = "r " + SpMetres(p.Radius);
                SpRow(lb, "uid", p.Uid, pWho, pWhere, pRad);
                pers++;
            }
        }
        if (pers == 0)
            SpRow(lb, "none", "", "#STR_OZ_VPP_SP_NONE", "", "");

        m_Repaint = false;
        SpMarkRows();
    }

    // One row; a caption's or an empty group's text is a stringtable key.
    protected void SpRow(TextListboxWidget lb, string kind, string key, string who, string where, string radius)
    {
        string first = OvTr(who);
        int row = lb.AddItem(first, NULL, 0);
        if (where != "")
            lb.SetItem(row, where, NULL, 1);
        if (radius != "")
            lb.SetItem(row, radius, NULL, 2);
        m_SpawnRowKind.Insert(kind);
        m_SpawnRowKey.Insert(key);
    }

    // Captions muted, the picked point in the accent colour (a NoScrollBar list draws
    // no highlight of its own).
    protected void SpMarkRows()
    {
        TextListboxWidget lb = TextListboxWidget.Cast(M_SUB_WIDGET.FindAnyWidget("SpawnList"));
        if (!lb)
            return;

        int ink = ARGBF(1, 0.88, 0.875, 0.855);
        int muted = ARGBF(1, 0.627, 0.624, 0.6);
        int faint = ARGBF(1, 0.36, 0.353, 0.345);
        int accent = ARGBF(1, 0.91, 0.47, 0.17);
        for (int r = 0; r < m_SpawnRowKind.Count(); r++)
        {
            string kind = m_SpawnRowKind[r];
            string pick = kind + ":" + m_SpawnRowKey[r];
            int c0 = ink;
            int c1 = muted;
            if (kind == "head")
                c0 = muted;
            else if (kind == "none")
                c0 = faint;
            else if (pick == m_SpPick)
            {
                c0 = accent;
                c1 = accent;
            }
            lb.SetItemColor(r, 0, c0);
            lb.SetItemColor(r, 1, c1);
            lb.SetItemColor(r, 2, c1);
        }
    }

    // Рядок списку КЛАДЕ СЕБЕ В ФОРМУ: a zone into the cycler, a personal point into
    // the player field, its radius into the radius field; the scheme marks it.
    protected void SpPickRow(int row)
    {
        if (row < 0 || row >= m_SpawnRowKind.Count())
            return;
        string kind = m_SpawnRowKind[row];
        string key = m_SpawnRowKey[row];
        if (kind != "zone" && kind != "uid")
            return;

        m_SpPick = kind + ":" + key;
        float radius = 0;
        string center = "";
        SpPointOf(m_SpPick, center, radius);
        if (radius > 0)
            SetEdit("SpRadius", SpMetres(radius));

        if (kind == "uid")
        {
            SetEdit("SpUid", key);
        }
        else
        {
            m_SpSlugPick = key;
            SetEdit("SpSlug", "");
            PaintSpawnCycler();
        }

        SpMarkRows();
        SpDrawScheme();
    }

    // The centre and radius of a point named like m_SpPick; false: no such point.
    protected bool SpPointOf(string pick, out string center, out float radius)
    {
        center = "";
        radius = 0;
        if (!m_SpawnsCfg || pick == "")
            return false;

        if (pick == "zone:*")
        {
            if (!m_SpawnsCfg.Staging || m_SpawnsCfg.Staging.Center == "")
                return false;
            center = m_SpawnsCfg.Staging.Center;
            radius = m_SpawnsCfg.Staging.Radius;
            return true;
        }

        if (pick.IndexOf("zone:") == 0 && m_SpawnsCfg.Zones)
        {
            string slug = pick.Substring(5, pick.Length() - 5);
            if (slug == "-")
                slug = "";
            for (int i = 0; i < m_SpawnsCfg.Zones.Count(); i++)
            {
                OZ_SpawnZone z = m_SpawnsCfg.Zones[i];
                if (!z || z.Role != slug)
                    continue;
                center = z.Center;
                radius = z.Radius;
                return true;
            }
            return false;
        }

        if (pick.IndexOf("uid:") == 0)
        {
            int at = SpPersonalAt(pick.Substring(4, pick.Length() - 4));
            if (at == -1)
                return false;
            center = m_SpawnsCfg.Personal[at].Center;
            radius = m_SpawnsCfg.Personal[at].Radius;
            return true;
        }
        return false;
    }

    // ------------------------------------------------- the scheme around the admin
    //
    // No map: the admin is the square in the middle, north is up, every point is a
    // circle of its radius (the staging zone in the alert colour, faction zones in the
    // text colour, the fallback muted, personal points green, the picked one in the
    // accent colour). The scale fits the farthest edge of any point. Drawn on the page's
    // CanvasWidget (dayz-modding gui-layouts.md §7: Clear, then every line again), on
    // each change and twice a second while the page is shown (Tick).
    protected void SpDrawScheme()
    {
        m_SpDrawnAt = GetGame().GetTime();

        CanvasWidget cv = CanvasWidget.Cast(M_SUB_WIDGET.FindAnyWidget("SpScheme"));
        if (!cv)
            return;
        cv.Clear();

        float w;
        float h;
        cv.GetScreenSize(w, h);
        DayZPlayer me = GetGame().GetPlayer();
        if (w <= 0 || h <= 0 || !me)
            return;
        vector at = me.GetPosition();

        // How far the scheme reaches: the farthest edge of any point, 100 m at least.
        float far = 100;
        if (m_SpawnsCfg)
        {
            if (m_SpawnsCfg.Staging)
            {
                float sReach = SpReach(at, m_SpawnsCfg.Staging.Center, m_SpawnsCfg.Staging.Radius);
                if (sReach > far)
                    far = sReach;
            }
            for (int i = 0; i < m_SpawnsCfg.Zones.Count(); i++)
            {
                OZ_SpawnZone fz = m_SpawnsCfg.Zones[i];
                if (!fz)
                    continue;
                float zReach = SpReach(at, fz.Center, fz.Radius);
                if (zReach > far)
                    far = zReach;
            }
            for (int k = 0; k < m_SpawnsCfg.Personal.Count(); k++)
            {
                OZ_SpawnPersonal fp = m_SpawnsCfg.Personal[k];
                if (!fp)
                    continue;
                float pReach = SpReach(at, fp.Center, fp.Radius);
                if (pReach > far)
                    far = pReach;
            }
        }

        float view = far * 1.1;
        float half = Math.Min(w, h) / 2 - 8;
        if (half < 10)
            return;
        float scale = half / view;
        float cx = w / 2;
        float cy = h / 2;

        // Rings at the edge and at half of it, and the cross through the admin.
        int faint = ARGBF(1, 0.36, 0.353, 0.345);
        SpCircle(cv, cx, cy, half, faint, 1);
        SpCircle(cv, cx, cy, half / 2, faint, 1);
        cv.DrawLine(cx - half, cy, cx + half, cy, 1, faint);
        cv.DrawLine(cx, cy - half, cx, cy + half, 1, faint);

        int ink = ARGBF(1, 0.88, 0.875, 0.855);
        int muted = ARGBF(1, 0.627, 0.624, 0.6);
        int alert = ARGBF(1, 0.96, 0.78, 0.25);
        int okInk = ARGBF(1, 0.4, 0.78, 0.36);
        int accent = ARGBF(1, 0.91, 0.47, 0.17);

        if (m_SpawnsCfg)
        {
            if (m_SpawnsCfg.Staging)
                SpDot(cv, at, scale, cx, cy, m_SpawnsCfg.Staging.Center, m_SpawnsCfg.Staging.Radius, alert, 1);
            for (int zi = 0; zi < m_SpawnsCfg.Zones.Count(); zi++)
            {
                OZ_SpawnZone z = m_SpawnsCfg.Zones[zi];
                if (!z)
                    continue;
                int zc = ink;
                if (z.Role == "")
                    zc = muted;
                SpDot(cv, at, scale, cx, cy, z.Center, z.Radius, zc, 1);
            }
            for (int pk = 0; pk < m_SpawnsCfg.Personal.Count(); pk++)
            {
                OZ_SpawnPersonal p = m_SpawnsCfg.Personal[pk];
                if (p)
                    SpDot(cv, at, scale, cx, cy, p.Center, p.Radius, okInk, 1);
            }

            // The picked point last, over the rest.
            string pc;
            float pr;
            if (SpPointOf(m_SpPick, pc, pr))
                SpDot(cv, at, scale, cx, cy, pc, pr, accent, 2);
        }

        // The admin: a square in the middle.
        cv.DrawLine(cx - 5, cy, cx + 5, cy, 10, accent);

        SpPaintSchemeText(at, view);
    }

    // How far a point's far edge is from the admin, on the ground (x and z).
    protected float SpReach(vector at, string center, float radius)
    {
        if (center == "")
            return 0;
        vector c = center.ToVector();
        if (c == vector.Zero)
            return 0;
        float dx = c[0] - at[0];
        float dz = c[2] - at[2];
        float d = Math.Sqrt(dx * dx + dz * dz);
        return d + radius;
    }

    protected void SpDot(CanvasWidget cv, vector at, float scale, float cx, float cy, string center, float radius, int color, float width)
    {
        if (center == "")
            return;
        vector c = center.ToVector();
        if (c == vector.Zero)
            return;
        float px = cx + (c[0] - at[0]) * scale;
        float py = cy - (c[2] - at[2]) * scale;
        float pr = radius * scale;
        if (pr < 4)
            pr = 4;
        SpCircle(cv, px, py, pr, color, width);
        cv.DrawLine(px - 2, py, px + 2, py, 4, color);
    }

    // A circle as a ring of short lines (CanvasWidget draws lines only).
    protected void SpCircle(CanvasWidget cv, float x, float y, float r, int color, float width)
    {
        int segs = 40;
        if (r < 20)
            segs = 16;
        float step = Math.PI2 / segs;
        float px = x + r;
        float py = y;
        for (int i = 1; i <= segs; i++)
        {
            float a = step * i;
            float nx = x + r * Math.Cos(a);
            float ny = y + r * Math.Sin(a);
            cv.DrawLine(px, py, nx, ny, width, color);
            px = nx;
            py = ny;
        }
    }

    // Under the scheme: how far its edge reaches, and where the picked point is from
    // the admin, in metres and a direction.
    protected void SpPaintSchemeText(vector at, float view)
    {
        int muted = ARGBF(1, 0.627, 0.624, 0.6);
        int ink = ARGBF(1, 0.88, 0.875, 0.855);

        string scaleWord = OvTr("#STR_OZ_VPP_SP_SCALE");
        string mWord = OvTr("#STR_OZ_VPP_SP_M");
        string edge = scaleWord + " " + SpMetres(view);
        edge = edge + " " + mWord;
        OvText("SpSchemeScale", edge, muted);

        string pc;
        float pr;
        if (!SpPointOf(m_SpPick, pc, pr))
        {
            OvText("SpSchemePick", "#STR_OZ_VPP_SP_PICK_HINT", muted);
            return;
        }

        vector c = pc.ToVector();
        float dx = c[0] - at[0];
        float dz = c[2] - at[2];
        float d = Math.Sqrt(dx * dx + dz * dz);
        string who = SpPickWho();
        string dir = SpDirection(dx, dz);
        string rWord = OvTr("#STR_OZ_VPP_SP_RADIUS_WORD");
        string line = who + ": " + SpMetres(d);
        line = line + " " + mWord;
        line = line + " " + dir;
        line = line + ", " + rWord;
        line = line + " " + SpMetres(pr);
        line = line + " " + mWord;
        OvText("SpSchemePick", line, ink);
    }

    protected string SpPickWho()
    {
        if (m_SpPick.IndexOf("uid:") == 0)
            return SpPlayerName(m_SpPick.Substring(4, m_SpPick.Length() - 4));
        if (m_SpPick.IndexOf("zone:") == 0)
            return SpWho(m_SpPick.Substring(5, m_SpPick.Length() - 5));
        return "";
    }

    // One of eight directions from the admin, north up: east is +x, north is +z.
    protected string SpDirection(float dx, float dz)
    {
        float deg = Math.Atan2(dx, dz) * Math.RAD2DEG;
        if (deg < 0)
            deg = deg + 360;
        int idx = Math.Round(deg / 45);
        idx = idx % 8;

        string all = OvTr("#STR_OZ_VPP_SP_DIRS");
        array<string> dirs = new array<string>();
        all.Split(",", dirs);
        if (idx < 0 || idx >= dirs.Count())
            return "";
        return dirs[idx];
    }

    // The page's buttons; false: not one of them.
    protected bool SpClick(string nm)
    {
        if (nm == "BtnSpOn")
        {
            SpAskSwitch();
            return true;
        }

        if (nm == "BtnSpFac")
        {
            CycleSpawnFaction();
            return true;
        }

        // Позицію сервер бере з тіла адміна, не з клієнта (OZ_SpawnSection).
        if (nm == "BtnSpHere")
        {
            string zarg = PickedSlug();
            string zrad = GetEdit("SpRadius").Trim();
            if (zrad != "")
                zarg = zarg + " " + zrad;
            Ask(OZ_AdminSect.SPAWNS, OZ_SpawnOp.HERE, zarg);
            return true;
        }

        // Removing asks first (design §3.4), naming the zone.
        if (nm == "BtnSpClear")
        {
            string slug = PickedSlug();
            string ask1 = OvTr("#STR_OZ_VPP_SP_CLEAR_ASK1");
            string ask2 = OvTr("#STR_OZ_VPP_SP_CLEAR_ASK2");
            string who = SpWho(slug);
            string ask = ask1 + " " + who;
            ask = ask + "? " + ask2;
            Confirm(ask, "#STR_OZ_VPP_SP_YES_CLEAR", "spclear:" + slug);
            return true;
        }

        // ОСОБИСТА ТОЧКА -- звідси, а не з панелі фракцій: вона про спавн, а не про
        // фракцію, i на сервері без мода фракцій її інакше не було де поставити.
        if (nm == "BtnSpUidHere" || nm == "BtnSpUidClear")
        {
            string uid = SpTargetUid();
            if (uid == "")
            {
                Hint("#STR_OZ_VPP_SP_NO_PLAYER");
                return true;
            }

            if (nm == "BtnSpUidHere")
            {
                string uarg = uid;
                string urad = GetEdit("SpRadius").Trim();
                if (urad != "")
                    uarg = uarg + " " + urad;
                Ask(OZ_AdminSect.SPAWNS, OZ_SpawnOp.UID_HERE, uarg);
                return true;
            }

            string pask = OvTr("#STR_OZ_VPP_SP_PCLEAR_ASK");
            string pwho = SpPlayerName(uid);
            string pline = pask + " " + pwho;
            pline = pline + "?";
            Confirm(pline, "#STR_OZ_VPP_SP_YES_CLEAR", "spuclear:" + uid);
            return true;
        }

        return false;
    }

    // ---------------------------------------------------------- raw

    // ---------------------------------------------------------- Ядро › Файли конфігів

    // The module a file's owner names, as the rail calls it; an owner with no page
    // in this window, by its own id.
    protected string RawOwnerLabel(string owner)
    {
        int m = m_ModIds.Find(owner);
        if (m != -1)
            return OvTr(m_ModLabels[m]);
        string up = owner;
        up.ToUpper();
        return up;
    }

    // The files grouped by the module that owns them, in the rail's order; owners
    // with no page here after them. A caption row per module (its row names no file).
    protected void RebuildRawList()
    {
        TextListboxWidget lb = TextListboxWidget.Cast(M_SUB_WIDGET.FindAnyWidget("RawList"));
        if (!lb)
            return;

        array<string> owners = new array<string>();
        for (int m = 0; m < m_ModIds.Count(); m++)
        {
            if (m_CfgOwners.Find(m_ModIds[m]) != -1)
                owners.Insert(m_ModIds[m]);
        }
        for (int o = 0; o < m_CfgOwners.Count(); o++)
        {
            if (owners.Find(m_CfgOwners[o]) == -1)
                owners.Insert(m_CfgOwners[o]);
        }

        m_Repaint = true;
        lb.ClearItems();
        m_RawRows.Clear();
        for (int g = 0; g < owners.Count(); g++)
        {
            string caption = RawOwnerLabel(owners[g]);
            lb.AddItem(caption, NULL, 0);
            m_RawRows.Insert("");
            for (int i = 0; i < m_CfgNames.Count(); i++)
            {
                if (m_CfgOwners[i] != owners[g])
                    continue;
                lb.AddItem("   " + m_CfgNames[i], NULL, 0);
                m_RawRows.Insert(m_CfgNames[i]);
            }
        }
        m_Repaint = false;

        RawMarkRows();

        string filesWord = OvTr("#STR_OZ_VPP_FL_FILES");
        string modsWord = OvTr("#STR_OZ_VPP_FL_MODULES");
        string count = filesWord + " " + m_CfgNames.Count().ToString();
        count = count + ", " + modsWord;
        count = count + " " + owners.Count().ToString();
        OvText("RawCount", count, ARGBF(1, 0.627, 0.624, 0.6));
    }

    // Captions muted, the picked file in the accent colour.
    protected void RawMarkRows()
    {
        TextListboxWidget lb = TextListboxWidget.Cast(M_SUB_WIDGET.FindAnyWidget("RawList"));
        if (!lb)
            return;
        int ink = ARGBF(1, 0.88, 0.875, 0.855);
        int muted = ARGBF(1, 0.627, 0.624, 0.6);
        int accent = ARGBF(1, 0.91, 0.47, 0.17);
        for (int r = 0; r < m_RawRows.Count(); r++)
        {
            int c = ink;
            if (m_RawRows[r] == "")
                c = muted;
            else if (m_RawRows[r] == m_RawPicked)
                c = accent;
            lb.SetItemColor(r, 0, c);
        }
    }

    // The editor is emptied before the file is asked: a file that does not come (not
    // written yet) must not leave the last file's text under its name, ready to apply.
    protected void RawPick(string name)
    {
        if (name == "")
            return;
        m_RawPicked = name;
        if (m_RawEditBox)
            m_RawEditBox.SetText("");
        m_RawSeen = "";
        m_RawChecked = "";
        m_RawCheckedOk = false;
        m_RawCheckedErr = "";
        RawMarkRows();
        RawPaintHead("#STR_OZ_VPP_FL_ASKING");
        RawPaintStatus(false);
        AskCfg(name);
    }

    protected string RawText()
    {
        string text = "";
        if (m_RawEditBox)
            m_RawEditBox.GetText(text);
        return text;
    }

    // The head of the editor: where the file lives and how big its text is now.
    protected void RawPaintHead(string sizeLine)
    {
        int ink = ARGBF(1, 0.88, 0.875, 0.855);
        int muted = ARGBF(1, 0.627, 0.624, 0.6);
        if (m_RawPicked == "")
        {
            OvText("RawPath", "#STR_OZ_VPP_FL_PICK", muted);
            OvText("RawSize", "", muted);
            return;
        }

        string path = m_RawPicked;
        int at = m_CfgNames.Find(m_RawPicked);
        if (at != -1 && at < m_CfgPaths.Count() && m_CfgPaths[at] != "")
            path = m_CfgPaths[at];
        OvText("RawPath", path, ink);
        OvText("RawSize", sizeLine, muted);
    }

    // "4.2 KB" or "312 bytes" of the text as it is in the editor now, and what the
    // module of the file does when it is applied.
    protected string RawSizeLine(int bytes)
    {
        string amount;
        if (bytes < 1024)
        {
            string bWord = OvTr("#STR_OZ_VPP_NW_BYTES");
            amount = bytes.ToString() + " " + bWord;
        }
        else
        {
            int tenths = Math.Round(bytes * 10.0 / 1024.0);
            int whole = tenths / 10;
            int part = tenths % 10;
            string kbWord = OvTr("#STR_OZ_VPP_FL_KB");
            amount = whole.ToString() + "." + part.ToString();
            amount = amount + " " + kbWord;
        }
        string note = OvTr("#STR_OZ_VPP_FL_SIZE_NOTE");
        return amount + " -- " + note;
    }

    // Does the text parse as JSON at all? Read into an object with no fields: the
    // serializer still walks the whole text and skips every key it does not know, so
    // only the syntax is judged (what the module's applier then checks is its own).
    protected void RawCheck(string text)
    {
        m_RawChecked = text;
        m_RawCheckedErr = "";
        if (text.Trim() == "")
        {
            m_RawCheckedOk = false;
            return;
        }
        OZ_VppJsonProbe probe = new OZ_VppJsonProbe();
        JsonSerializer js = new JsonSerializer();
        string jerr;
        bool parsed = js.ReadFromString(probe, text, jerr);
        m_RawCheckedOk = parsed;
        if (!parsed)
            m_RawCheckedErr = jerr;
    }

    protected void RawPaintStatus(bool pending)
    {
        int okInk = ARGBF(1, 0.4, 0.78, 0.36);
        int badInk = ARGBF(1, 0.88, 0.32, 0.29);
        int muted = ARGBF(1, 0.627, 0.624, 0.6);

        if (m_RawPicked == "")
        {
            OvText("RawJson", "", muted);
            OvText("RawWhere", "", muted);
            return;
        }

        if (pending)
            OvText("RawJson", "#STR_OZ_VPP_FL_JSON_WAIT", muted);
        else if (m_RawCheckedOk)
            OvText("RawJson", "#STR_OZ_VPP_FL_JSON_OK", okInk);
        else
        {
            string bad = OvTr("#STR_OZ_VPP_FL_JSON_BAD");
            if (m_RawCheckedErr != "")
                bad = bad + ": " + m_RawCheckedErr;
            OvText("RawJson", bad, badInk);
        }

        if (!m_RawEditBox)
            return;
        int line = m_RawEditBox.GetCarriageLine() + 1;
        int lines = m_RawEditBox.GetLinesCount();
        string lineWord = OvTr("#STR_OZ_VPP_FL_LINE");
        string ofWord = OvTr("#STR_OZ_VPP_PLR_OF");
        string where = lineWord + " " + line.ToString();
        where = where + " " + ofWord;
        where = where + " " + lines.ToString();
        OvText("RawWhere", where, muted);
    }

    // Every 300 ms on this page (Tick): the size follows the text at once, the JSON
    // check once typing has paused (a broken text is judged once, not per key).
    protected void RawFollow()
    {
        int now = GetGame().GetTime();
        if (now - m_RawTickAt < 300)
            return;
        m_RawTickAt = now;
        if (m_RawPicked == "" || !m_RawEditBox)
            return;

        string text = RawText();
        if (text != m_RawSeen)
        {
            m_RawSeen = text;
            m_RawSeenAt = now;
            RawPaintHead(RawSizeLine(text.Length()));
        }

        bool pending = m_RawChecked != m_RawSeen;
        if (pending && now - m_RawSeenAt >= 800)
        {
            RawCheck(m_RawSeen);
            pending = false;
        }
        RawPaintStatus(pending);
    }

    // The text of the picked file came from the server (cfg_get).
    protected void RawTake(string body)
    {
        if (m_RawEditBox)
            m_RawEditBox.SetText(body);
        m_RawSeen = body;
        m_RawSeenAt = GetGame().GetTime();
        RawCheck(body);
        RawPaintHead(RawSizeLine(body.Length()));
        RawPaintStatus(false);
    }

    // The page's buttons; false: not one of them.
    protected bool RawClick(string nm)
    {
        if (nm == "BtnRawReload")
        {
            if (m_RawPicked != "")
                RawPick(m_RawPicked);
            return true;
        }

        if (nm == "BtnRawApply")
        {
            if (m_RawPicked == "")
            {
                Hint("#STR_OZ_VPP_FL_PICK_FIRST");
                return true;
            }

            // A text that does not parse never leaves the client: the server would
            // refuse it with the same verdict, a round trip later.
            string current = RawText();
            RawCheck(current);
            RawPaintStatus(false);
            if (!m_RawCheckedOk)
            {
                Hint("#STR_OZ_VPP_FL_NOT_SENT");
                return true;
            }

            string applyAsk = OvTr("#STR_OZ_VPP_APPLY_ASK");
            string whose = RawOwnerLabel(RawOwnerOf(m_RawPicked));
            string ask = applyAsk + " " + m_RawPicked;
            ask = ask + " (" + whose;
            ask = ask + ")?";
            Confirm(ask, "#STR_OZ_VPP_YES_APPLY", "cfg:" + m_RawPicked);
            return true;
        }

        // The clipboard by script, not the engine's paste: that one stops at 512 bytes
        // in an edit box (dayz-modding gui-layouts §22; design §4.2).
        if (nm == "BtnRawPaste")
        {
            string clip;
            GetGame().CopyFromClipboard(clip);
            if (!m_RawEditBox || clip == "")
                return true;
            m_RawEditBox.SetText(clip);
            m_RawSeen = clip;
            m_RawSeenAt = GetGame().GetTime();
            RawCheck(clip);
            RawPaintHead(RawSizeLine(clip.Length()));
            RawPaintStatus(false);
            string pasted = OvTr("#STR_OZ_VPP_FL_PASTED");
            string sizeNow = RawSizeLine(clip.Length());
            Hint(pasted + " " + sizeNow);
            return true;
        }

        if (nm == "BtnRawCopy")
        {
            string whole = RawText();
            GetGame().CopyToClipboard(whole);
            string copied = OvTr("#STR_OZ_VPP_FL_COPIED");
            Hint(copied + " " + whole.Length().ToString());
            return true;
        }

        return false;
    }

    protected string RawOwnerOf(string name)
    {
        int at = m_CfgNames.Find(name);
        if (at == -1 || at >= m_CfgOwners.Count())
            return "core";
        return m_CfgOwners[at];
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
                RawPick(m_RawRows[row]);
            return true;
        }

        if (nm == "SpawnList")
        {
            SpPickRow(row);
            return true;
        }

        // A player's row opens his card; the card asks the server for his character.
        // A question already on the confirm strip was about someone else: it goes.
        if (nm == "PlrList")
        {
            if (row >= 0 && row < m_PlrRowUid.Count())
            {
                if (m_ConfirmAction.IndexOf("wipe:") == 0)
                    CloseConfirm();
                PlrPick(m_PlrRowUid[row]);
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

        if (nm == "BtnOvRefresh")
        {
            AskOverview();
            return true;
        }

        if (nm == "BtnOvReload")
        {
            Confirm("#STR_OZ_VPP_OV_ASK", "#STR_OZ_VPP_OV_YES", "reload_all");
            return true;
        }

        // Ядро › Точки появи.
        if (SpClick(nm))
            return true;

        if (nm == "BtnPlrRefresh")
        {
            PlrAskList();
            if (m_PlrPick != "")
                PlrPick(m_PlrPick);
            return true;
        }

        // ПЕРМАДЕС НАЗИВАЄ ЛЮДИНУ: the question on the confirm strip names the
        // character the peek found. A wipe pressed before the peek came back waits
        // for it (PlrTakePeek) and asks it once more, in case the first was lost.
        if (nm == "BtnPlrWipe")
        {
            if (m_PlrPick == "")
            {
                Hint("#STR_OZ_VPP_PLR_PICK_FIRST");
                return true;
            }
            if (m_PlrPeeked)
            {
                PlrAskWipe();
                return true;
            }
            m_PlrWantWipe = true;
            Ask(OZ_AdminSect.PLAYERS, OZ_PlayerOp.PEEK + ":" + m_PlrPick, "{}");
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
            NwPost();
            return true;
        }

        if (nm == "BtnNwClear")
        {
            NwClear();
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

        // Ядро › Файли конфігів.
        if (RawClick(nm))
            return true;

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
