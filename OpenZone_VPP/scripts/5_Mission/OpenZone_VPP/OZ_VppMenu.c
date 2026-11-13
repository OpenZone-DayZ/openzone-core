// Вкладка «OpenZone» у VPP Admin Tools: вікно з рейкою модулів. Цей pbo
// реєструє сторінки ядра -- Огляд і журнал, Точки появи, Новини, Гравці,
// Дзеркало Discord, Файли конфігів -- і «Групи FFR» модуля фракцій, коли
// фракції сервера дає склейка OpenZone Identity FFR (серверний мод, тож
// клієнт дізнається про неї лише з відповіді identity). Сторінки іншого мода
// додає СКЛЕЙКА -- окремий pbo, що чіпляється сюди через modded class, де
// точки розширення позначені словом protected. Сторінки фракцій, наприклад,
// приїжджають з OpenZone_Factions_VPP, з репозиторію фракцій.
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

// Ядро › Новини: a page of the bridge's feed (v1/news/list, handed on by the core's
// news_list), newest first. The PDA mod reads the same answer into classes of its own
// (OZ_NewsList); the window works without that mod, so these twins of the same shape
// read the same JSON -- JSON carries field names, not class names.
class OZ_VppNewsItem
{
    string Id;
    string Title;
    string Who;
    string At;
    int    Replies;

    OZ_VppNewsItem Copy()
    {
        OZ_VppNewsItem c = new OZ_VppNewsItem();
        c.Id = Id;
        c.Title = Title;
        c.Who = Who;
        c.At = At;
        c.Replies = Replies;
        return c;
    }
}

class OZ_VppNewsList
{
    ref array<ref OZ_VppNewsItem> Items;
    // Where the feed goes on; empty: this page reaches its end.
    string Next;

    void OZ_VppNewsList()
    {
        Items = new array<ref OZ_VppNewsItem>();
        Next = "";
    }

    // Read through a copy made by script (the core's OZ_ConfigBase header).
    OZ_VppNewsList Copy()
    {
        OZ_VppNewsList c = new OZ_VppNewsList();
        c.Next = Next;
        if (!Items)
            return c;
        for (int i = 0; i < Items.Count(); i++)
        {
            if (Items[i])
                c.Items.Insert(Items[i].Copy());
        }
        return c;
    }
}

// Фракції › Групи FFR: the answer of the FFR glue's ffr_groups and its settings file, as
// the window reads them. The glue is a server mod (-serverMod): a client never loads it,
// so its classes (OZI_FfrGroupsAnswer, OZI_FfrSettings in OpenZone_Identity_FFR) are not
// here, and these twins of the same shape read the same JSON -- JSON carries field
// names, not class names. Change a field there, change it here.
class OZ_VppFfrGroupRow
{
    string Name;
    int    Members;
    string FactionId;

    OZ_VppFfrGroupRow Copy()
    {
        OZ_VppFfrGroupRow c = new OZ_VppFfrGroupRow();
        c.Name = Name;
        c.Members = Members;
        c.FactionId = FactionId;
        return c;
    }
}

class OZ_VppFfrGroups
{
    bool   FileRead;
    int    ReadAgo;
    string FfrFile;
    int    Players;
    ref array<ref OZ_VppFfrGroupRow> Groups;
    string BaseFaction;
    bool   AnyMemberSpends;
    int    ReloadSeconds;

    void OZ_VppFfrGroups()
    {
        Groups = new array<ref OZ_VppFfrGroupRow>();
    }

    // Read through a copy made by script (the core's OZ_ConfigBase header).
    OZ_VppFfrGroups Copy()
    {
        OZ_VppFfrGroups c = new OZ_VppFfrGroups();
        c.FileRead = FileRead;
        c.ReadAgo = ReadAgo;
        c.FfrFile = FfrFile;
        c.Players = Players;
        c.BaseFaction = BaseFaction;
        c.AnyMemberSpends = AnyMemberSpends;
        c.ReloadSeconds = ReloadSeconds;
        if (!Groups)
            return c;
        for (int i = 0; i < Groups.Count(); i++)
        {
            if (Groups[i])
                c.Groups.Insert(Groups[i].Copy());
        }
        return c;
    }
}

class OZ_VppFfrMap
{
    string FfrGroup;
    string FactionId;

    OZ_VppFfrMap Copy()
    {
        OZ_VppFfrMap c = new OZ_VppFfrMap();
        c.FfrGroup = FfrGroup;
        c.FactionId = FactionId;
        return c;
    }
}

// OZ_Identity_FFR.json: what the page edits and sends back whole through cfg_set.
class OZ_VppFfrSettings
{
    int Version;
    ref array<ref OZ_VppFfrMap> GroupFactions;
    string BaseFaction;
    bool AnyMemberSpends;
    int ReloadSeconds;

    void OZ_VppFfrSettings()
    {
        Version = 1;
        GroupFactions = new array<ref OZ_VppFfrMap>();
        BaseFaction = "";
        AnyMemberSpends = true;
        ReloadSeconds = 30;
    }

    OZ_VppFfrSettings Copy()
    {
        OZ_VppFfrSettings c = new OZ_VppFfrSettings();
        c.Version = Version;
        c.BaseFaction = BaseFaction;
        c.AnyMemberSpends = AnyMemberSpends;
        c.ReloadSeconds = ReloadSeconds;
        if (!GroupFactions)
            return c;
        for (int i = 0; i < GroupFactions.Count(); i++)
        {
            if (GroupFactions[i])
                c.GroupFactions.Insert(GroupFactions[i].Copy());
        }
        return c;
    }

    // The faction a group maps to here; empty: none.
    string FactionOf(string gname)
    {
        for (int i = 0; i < GroupFactions.Count(); i++)
        {
            if (GroupFactions[i].FfrGroup == gname)
                return GroupFactions[i].FactionId;
        }
        return "";
    }

    // Map a group to a faction, or drop its row when the id is empty.
    void SetFaction(string gname, string id)
    {
        for (int i = 0; i < GroupFactions.Count(); i++)
        {
            if (GroupFactions[i].FfrGroup != gname)
                continue;
            if (id == "")
                GroupFactions.RemoveOrdered(i);
            else
                GroupFactions[i].FactionId = id;
            return;
        }
        if (id == "")
            return;
        OZ_VppFfrMap m = new OZ_VppFfrMap();
        m.FfrGroup = gname;
        m.FactionId = id;
        GroupFactions.Insert(m);
    }

    string ToJsonText()
    {
        string text;
        string err;
        if (!JsonFileLoader<OZ_VppFfrSettings>.MakeData(this, text, err, true))
            return "";
        return text;
    }
}


modded class VPPAdminHud
{
    override void DefineButtons()
    {
        super.DefineButtons();
        // The series' mark, the OZ-COM one the radios carry (source/icons/make_icons.py).
        InsertButton("OZ_VppAdminMenu", "OpenZone", "set:ozv_ui image:oz_mark", "OpenZone: spawns, configs, news");
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

    // The window's layouts are converted from the mockup (css2layout) and carry how each
    // widget follows a resize; m_Flex applies it. The window's size in pixels as last
    // set, the content area's frame the pages live in, the confirm strip (a layout of its
    // own), the search box of the title band and what it last held, when the title band's
    // chips were last asked, and the edit boxes with a placeholder label (parallel lists).
    protected ref OZ_VppFlex m_Flex;
    protected float  m_WinW;
    protected float  m_WinH;
    protected Widget m_Content;
    protected Widget m_ConfirmBar;
    protected EditBoxWidget m_SearchBox;
    protected string m_SearchSeen;
    protected int    m_ChipsAt;
    protected int    m_PhAt;
    protected string m_StatusSaid;
    protected ref array<EditBoxWidget> m_PhEdits;
    protected ref array<Widget> m_PhLabels;

    // The confirm strip (design §3.4): the action its "yes" sends to OnConfirmed.
    protected string m_ConfirmAction = "";

    // The journal as the status bar last showed it, and the filter of Ядро › Огляд.
    protected int  m_JournalSeen = -1;
    protected bool m_JournalErrors = false;
    // "Only mine": the entries of other admins left out.
    protected bool m_JournalMine = false;
    protected bool m_RepaintQueued = false;

    // Ядро › Огляд: the last overview the server gave, as a copy made by script, and
    // how many module cards the page holds.
    protected ref OZ_AdminOverview m_Ov;
    static const int OV_CARDS = 7;

    // The journal's rows on Ядро › Огляд (a row template, one per entry shown), and the
    // order the module cards stand in (indices into m_Ov.Modules).
    protected ref OZ_VppFlexRows m_JrRows;
    protected ref array<int> m_OvOrder;

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

    // The list of the page (board CoreSpawns): caption rows and point rows stand in one
    // list, a row template each. A point's key is "zone:<slug>" (* staging, - fallback)
    // or "uid:<Steam ID>"; the row of an empty group has the key "none".
    protected ref OZ_VppFlexRows m_SpHeads;
    protected ref OZ_VppFlexRows m_SpRows;

    // The picked point (a row's key), the cycler's slug ("" is its first choice), when
    // the scheme was last drawn (GetTime, ms), and the labels that stand over the scheme:
    // made as the points need them, kept and placed again by every redraw.
    protected string m_SpPick;
    protected string m_SpSlugPick;
    protected int    m_SpDrawnAt;
    protected ref array<Widget> m_SpLabels;
    protected int    m_SpLabelsUsed;

    // The window's size (pixels) the open page of the core was last fitted to: Tick fits
    // the page's lists and flowed words again when the window has another (CoreRefit),
    // and for m_CoreFitDue frames more -- a text measured on the frame its page was
    // shown or resized may not know its width yet.
    protected float  m_CoreFitW;
    protected float  m_CoreFitH;
    protected int    m_CoreFitDue;

    // What the source of factions said (identity, a read): its name and the faction
    // ids it knows, with their names side by side.
    protected string m_IdSource;
    protected ref array<string> m_IdFactions;
    protected ref array<string> m_IdNames;
    protected bool   m_IdAsked;

    // ------------------------------------------------- Фракції › Групи FFR
    //
    // The FFR glue's names on the wire: its Source(), its admin section and ops, its
    // config. The glue is a server mod and the client never sees its class (OZI_Ffr), so
    // the strings live twice; change one, change both.
    static const string FFR_SOURCE    = "OpenZone Identity FFR";
    static const string FFR_SECTION   = "ffr";
    static const string FFR_OP_GROUPS = "ffr_groups";
    static const string FFR_OP_REREAD = "ffr_reread";
    static const string FFR_CFG       = "IdentityFFR";

    // Whether the page is registered, the last ffr_groups, the settings file as saved
    // and as edited, the list's rows (a row's key is the group's name), the picked
    // group, the faction the picked group's list stands on ("" is no faction; an id typed
    // into the field under it wins), whether this page's own apply is on its way back,
    // the page's tick and its three fields.
    protected bool m_FfrOn;
    protected ref OZ_VppFfrGroups   m_FfrGroups;
    protected ref OZ_VppFfrSettings m_FfrSaved;
    protected ref OZ_VppFfrSettings m_FfrDraft;
    protected ref OZ_VppFlexRows m_FfrRows;
    protected string m_FfrPick;
    protected string m_FfrFacPick;
    protected bool   m_FfrApplied;
    protected int    m_FfrTickAt;
    protected EditBoxWidget m_FfrFacBox;
    protected EditBoxWidget m_FfrBaseBox;
    protected EditBoxWidget m_FfrReloadBox;

    // ------------------------------------------------- raw
    protected string m_RawPicked = "";

    // The list of files (board CoreFiles): a caption row per module, a row per file (its
    // key is the file's name), and a "no files" row for a module of the rail that owns
    // none -- three row templates in one list.
    protected ref OZ_VppFlexRows m_RawHeads;
    protected ref OZ_VppFlexRows m_RawFiles;
    protected ref OZ_VppFlexRows m_RawNones;

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
    // The last players_here as copies made by script, the list's rows (a row's key is
    // the Steam ID) and the card: whose it is and what the last player_peek said of that
    // character. The list holds who is in the game (the server lists them without the
    // bridge); a Steam ID typed whole in the search reaches an absent player too. The
    // wipe asks on the confirm strip and names what the peek found: seventeen digits with
    // a typo look exactly like the right ones. Fields are declared bare and set in
    // OnCreate: every glue derives from this class (dayz-modding enforce-script.md, field
    // initialisers).
    protected ref array<string> m_PlrNames;
    protected ref array<string> m_PlrUids;
    protected ref OZ_VppFlexRows m_PlrRows;
    protected int    m_PlrMax;

    // "Інше про гравця": the ways from a player's card into other pages about him, as
    // AddPlayerLink gathered them (title, what is there, the page it opens); the card has
    // PLR_LINKS places. And for how many more frames the card's head is laid out again
    // (a text knows its width for certain only once it was drawn).
    static const int PLR_LINKS = 3;
    protected ref array<string> m_PlrLinkTitle;
    protected ref array<string> m_PlrLinkWhat;
    protected ref array<string> m_PlrLinkPage;
    protected int    m_PlrFlowDue;
    protected string m_PlrPick;
    protected string m_PlrFindSeen;
    protected bool   m_PlrPeeked;
    protected bool   m_PlrFound;
    protected string m_PlrCharName;
    protected int    m_PlrCharGen;
    protected bool   m_PlrWantWipe;
    protected EditBoxWidget m_PlrFindBox;

    // The live search of the pages' class fields (OZ_ClassPick, design
    // docs/specs/2026-11-13-openzone-vpp-class-search-design.md).
    protected ref OZ_ClassPick m_ClassPick;

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

        m_Flex = new OZ_VppFlex();
        m_OvOrder = new array<int>();
        m_WinW = DEF_W * UnitScale();
        m_WinH = DEF_H * UnitScale();
        m_Content = M_SUB_WIDGET.FindAnyWidget("VppContent");
        if (m_Content)
            m_Content.SetFlags(WidgetFlags.CLIPCHILDREN);
        // The confirm strip lives in the content frame beside the pages and over them: a
        // page's root has priority 100, a module's own strip (a draft bar) takes 150, this
        // one 200. As a child of the window's root with a sort of 20 it was shown and drawn
        // under the page (measured 2026-11-13).
        m_ConfirmBar = OZ_VppFlex.Create("OpenZone_VPP/gui/layouts/oz_vpp_confirm.layout", M_SUB_WIDGET, "OZ_VppConfirm");
        if (m_ConfirmBar)
        {
            if (m_Content)
                m_Content.AddChild(m_ConfirmBar);
            m_ConfirmBar.Show(false);
            m_ConfirmBar.SetSort(200);
        }
        if (m_Grip)
            m_Grip.SetSort(400);
        m_SearchBox = EditBoxWidget.Cast(M_SUB_WIDGET.FindAnyWidget("VppSearch"));
        m_SearchSeen = "";
        m_ChipsAt = 0;
        m_PhAt = 0;
        m_StatusSaid = "";
        m_PhEdits = new array<EditBoxWidget>();
        m_PhLabels = new array<Widget>();
        AdoptPlaceholders(M_SUB_WIDGET.FindAnyWidget("VppHeader"));

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
        m_PlrNames  = new array<string>();
        m_PlrUids   = new array<string>();
        m_PlrLinkTitle = new array<string>();
        m_PlrLinkWhat  = new array<string>();
        m_PlrLinkPage  = new array<string>();
        m_PlrFlowDue = 0;
        m_PlrMax = 0;
        m_PlrPick = "";
        m_PlrFindSeen = "";
        m_PlrPeeked = false;
        m_PlrFound = false;
        m_PlrCharName = "";
        m_PlrCharGen = 0;
        m_PlrWantWipe = false;
        m_SpPick = "";
        m_SpSlugPick = "";
        m_SpDrawnAt = 0;
        m_SpLabels = new array<Widget>();
        m_SpLabelsUsed = 0;
        m_CoreFitW = 0;
        m_CoreFitH = 0;
        m_CoreFitDue = 0;
        m_IdSource = "";
        m_IdFactions = new array<string>();
        m_IdNames = new array<string>();
        m_IdAsked = false;
        m_FfrOn = false;
        m_FfrPick = "";
        m_FfrFacPick = "";
        m_FfrApplied = false;
        m_FfrTickAt = 0;
        m_NwVoices = new array<string>();
        m_NwPick   = 0;
        m_NwSelf   = "";
        m_NwFitDue = 0;
        m_MirrorKinds = new array<string>();
        m_MirrorOn    = new array<int>();
        m_MirrorNotes = new array<string>();

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
        AddOwnPane("mirror",  "#STR_OZ_VPP_PAGE_MIRROR",  "");
        AddOwnPane("raw",     "#STR_OZ_VPP_PAGE_FILES",   "");
        // The spawns page's radius starts at the server's own default, as the board shows.
        SetEdit("SpRadius", "20");
        // The core's own way out of a player's card: his personal spawn point.
        AddPlayerLink("#STR_OZ_VPP_PAGE_SPAWNS", "#STR_OZ_VPP_PLR_LINK_SPAWNS", "spawns");
        m_PlrFindBox = EditBoxWidget.Cast(M_SUB_WIDGET.FindAnyWidget("PlrFind"));
        m_NwTitleBox = EditBoxWidget.Cast(M_SUB_WIDGET.FindAnyWidget("NwTitle"));
        m_RawEditBox = MultilineEditBoxWidget.Cast(M_SUB_WIDGET.FindAnyWidget("RawEdit"));
        m_NwBodyBox = MultilineEditBoxWidget.Cast(M_SUB_WIDGET.FindAnyWidget("NwBody"));
        m_NwSeenTitle = "";
        m_NwSeenBody = "";

        // The pages' class fields find classes as they are typed: the glues name their
        // fields in ClassFields, the list follows the focused one from Tick.
        m_ClassPick = new OZ_ClassPick(this, M_SUB_WIDGET);
        ClassFields(m_ClassPick);

        if (!m_Ears)
        {
            m_Ears = true;
            OZ_ClientState.AdminWatch().Insert(this.OnAdminResponse);
            OZ_ClientState.AdminEchoWatch().Insert(this.OnAdminEcho);
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
            OZ_ClientState.AdminEchoWatch().Remove(this.OnAdminEcho);
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
    // Ядро › Дзеркало Discord (ТЗ-2 §8): стан приходить mirror_list, перемикання --
    // mirror_set через смугу підтвердження, бо вмикання заливає в Discord усю
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

    // Паралельні списки, по одному запису на рядок: рід, його стан і речення «що
    // буде, як натиснути». Клік шукається по рядку, тобто рід їде разом із рядком,
    // а не зашитий в ім'я віджета.
    protected ref array<string> m_MirrorKinds;
    protected ref array<int>    m_MirrorOn;
    protected ref array<string> m_MirrorNotes;

    // The rows themselves, a row template each (oz_vpp_mirror_row): made again by every
    // mirror_list.
    protected ref OZ_VppFlexRows m_MrRows;

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

    // The newest posts as the bridge listed them (news_list), a row each; and for how
    // many more frames the preview's card is fitted to its text (a wrapped text may know
    // its height only after it was drawn once).
    protected ref OZ_VppFlexRows m_NwRows;
    protected int                m_NwFitDue;

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

    // Where the window first stands (fractions of VPP's panel) and its smallest size, in
    // layout units. A page follows its window by the rule measured as the mockup grew;
    // the same rule is trusted a little way down (lists and stretching columns give up
    // 260 x 172 units), and the window does not get smaller than that: below it a page's
    // fixed parts would meet.
    static const float WIN_X  = 0.008;
    static const float WIN_Y  = 0.06;
    static const float MIN_W  = 1340;
    static const float MIN_H  = 728;

    // The window as its layout declares it (oz_vpp_admin.layout, converted from the
    // mockup's Main board), in layout units; times UnitScale() for pixels. The engine does
    // not read the mockup, so the numbers live twice: convert again, check these. DEF: the
    // window; CONTENT: the frame the pages live in; PAGE: a page as the mockup draws it
    // (2 x 4 units more than the room the window's own frame leaves: a page is fitted);
    // CONFIRM: the confirm strip's height; HEAD_W and BAR_W: the title band and the status
    // bar; SEARCH_X, CHIP_Y, CHIP_H, CRUMB_X, CRUMB_Y, CRUMB_H: parts of the title band.
    static const float DEF_W     = 1600;
    static const float DEF_H     = 900;
    static const float CONTENT_X = 241;
    static const float CONTENT_Y = 54;
    static const float CONTENT_W = 1358;
    static const float CONTENT_H = 808;
    static const float PAGE_W    = 1360;
    static const float PAGE_H    = 812;
    static const float CONFIRM   = 60;
    static const float BAR_W     = 1598;
    static const float SEARCH_X  = 1288;
    static const float CHIP_Y    = 15;
    static const float CHIP_H    = 23;
    static const float CRUMB_X   = 246;
    static const float CRUMB_Y   = 18;
    static const float CRUMB_H   = 17;


    // Своя панель -- окремою розміткою, точно як у склейок: створити під
    // коренем і зареєструвати. Ім'я файлу росте з того самого id, тож
    // розійтися їм нема де.
    protected void AddOwnPane(string id, string label, string hintName)
    {
        string layout = "OpenZone_VPP/gui/layouts/oz_vpp_pane_" + id + ".layout";

        // The root's name is "Pane" and the id with a capital: checked, because the engine
        // has handed one path another file's widgets (OZ_VppFlex.Create).
        string first = id.Substring(0, 1);
        first.ToUpper();
        string rootName = "Pane" + first + id.Substring(1, id.Length() - 1);
        Widget pane = OZ_VppFlex.Create(layout, M_SUB_WIDGET, rootName);
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
            // The page lives in the content area's frame: cut at its edge, moved with it.
            if (m_Content)
                m_Content.AddChild(pane);
            pane.SetFlags(WidgetFlags.CLIPCHILDREN);
            // A page converted from the mockup carries its own colours; a pane from before
            // is still repainted from the series palette to the window's.
            if (!IsFlexPane(pane))
                OZ_VppPaint.Repaint(pane);
            AdoptPlaceholders(pane);
            PlacePane(pane);
        }
        LayoutRail();
    }

    // Whether a pane was laid out by css2layout: its root says so (the flag in its userID).
    protected bool IsFlexPane(Widget pane)
    {
        if (!pane)
            return false;
        return (pane.GetUserID() & OZ_VppFlex.FLAG) != 0;
    }

    // An edit box named X with a label named XPh beside it: the label is the box's
    // placeholder, shown while the box is empty (Tick keeps it so).
    protected void AdoptPlaceholders(Widget root)
    {
        if (!root)
            return;
        Widget c = root.GetChildren();
        string nm;
        EditBoxWidget box;
        while (c)
        {
            nm = c.GetName();
            if (nm.Length() > 2 && nm.Substring(nm.Length() - 2, 2) == "Ph")
            {
                box = EditBoxWidget.Cast(M_SUB_WIDGET.FindAnyWidget(nm.Substring(0, nm.Length() - 2)));
                if (box && m_PhEdits.Find(box) == -1)
                {
                    m_PhEdits.Insert(box);
                    m_PhLabels.Insert(c);
                }
            }
            AdoptPlaceholders(c);
            c = c.GetSibling();
        }
    }

    protected void FollowPlaceholders()
    {
        int i;
        for (i = 0; i < m_PhEdits.Count(); i++)
        {
            if (m_PhEdits[i] && m_PhLabels[i])
                m_PhLabels[i].Show(m_PhEdits[i].GetText() == "");
        }
    }

    // A page in the content area. A converted page follows the area by the rule its layout
    // carries, up from its design size and down to the window's smallest (the mockup draws
    // its pages 2 x 4 units larger than the room the window's frame leaves, so a page is
    // a little under its design size even in the window as drawn).
    protected void PlacePane(Widget pane)
    {
        float s = UnitScale();
        float cw = m_WinW - (DEF_W - CONTENT_W) * s;
        float ch = m_WinH - (DEF_H - CONTENT_H) * s;
        pane.SetPos(0, 0);
        if (!IsFlexPane(pane))
        {
            pane.SetSize(cw, ch);
            return;
        }
        m_Flex.Resize(pane, cw, ch, PAGE_W * s, PAGE_H * s);
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
    // The mockup's rail: 12 units of room on top, a module's head 40 high, its pages 32
    // high a unit apart under it (4 above them, 6 below), 4 between modules.
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
        float y = 12 * s;
        for (int m = 0; m < m_ModIds.Count(); m++)
        {
            string mod = m_ModIds[m];
            int pages = PagesOf(mod);
            bool open = m_ModOpen[m] == 1;
            AddRailRow(rail, "mod", mod, m_ModLabels[m], pages, open, y, s);
            y += 40 * s;
            if (pages > 1 && open)
            {
                y += 4 * s;
                // the module's pages in the order they registered, its config files last
                // (that page registers when cfg_list answers; a glue's page may come later)
                for (int pass = 0; pass < 2; pass++)
                {
                    for (int p = 0; p < m_TabIds.Count(); p++)
                    {
                        if (m_PageModule[p] != mod)
                            continue;
                        if (IsFilesPage(m_TabIds[p]) != (pass == 1))
                            continue;
                        AddRailRow(rail, "page", m_TabIds[p], m_PageLabel[p], 0, false, y, s);
                        y += 33 * s;
                    }
                }
                y += 5 * s;
            }
            y += 4 * s;
        }
    }

    protected void AddRailRow(Widget rail, string kind, string key, string label, int pages, bool open, float y, float s)
    {
        string layout = "OpenZone_VPP/gui/layouts/oz_vpp_rail_page.layout";
        string rootName = "OZ_VppRailPage";
        if (kind == "mod")
        {
            layout = "OpenZone_VPP/gui/layouts/oz_vpp_rail_mod.layout";
            rootName = "OZ_VppRailMod";
        }
        Widget row = OZ_VppFlex.Create(layout, rail, rootName);
        if (!row)
        {
            OZ_Log.Error("vpp rail: layout failed to load: " + layout);
            return;
        }

        int n = m_RailRows.Count();
        row.SetName("rail:" + n.ToString());
        row.SetPos(10 * s, y);
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

        int cInk = ARGB(255, 224, 223, 218);
        int cMuted = ARGB(255, 160, 159, 153);

        if (kind == "mod")
        {
            TextWidget modName = TextWidget.Cast(row.FindAnyWidget("RailModName"));
            if (modName)
            {
                modName.SetText(label);
                if (holds)
                    modName.SetColor(cInk);
                else
                    modName.SetColor(cMuted);
            }
            TextWidget mark = TextWidget.Cast(row.FindAnyWidget("RailModMark"));
            if (mark)
                mark.SetText(ModuleMark(key, label));
            Widget modBg = row.FindAnyWidget("OZ_VppRailModBg");
            if (modBg)
                modBg.Show(holds);
            ImageWidget chev = ImageWidget.Cast(row.FindAnyWidget("RailModChevron"));
            if (chev)
            {
                chev.Show(pages > 1);
                if (open)
                    chev.LoadImageFile(0, "set:ozv_ui image:tri_down");
                else
                    chev.LoadImageFile(0, "set:ozv_ui image:tri_right");
                chev.SetImage(0);
            }
            return;
        }

        TextWidget pageName = TextWidget.Cast(row.FindAnyWidget("RailPageName"));
        if (pageName)
        {
            pageName.SetText(label);
            if (current)
                pageName.SetColor(cInk);
            else
                pageName.SetColor(cMuted);
        }
        Widget pageBg = row.FindAnyWidget("OZ_VppRailPageBg");
        if (pageBg)
            pageBg.Show(current);
        Widget pageBar = row.FindAnyWidget("OZ_VppRailPageBar");
        if (pageBar)
            pageBar.Show(current);
    }

    // The two letters on a module's badge: the stringtable's STR_OZ_VPP_MARK_<ID> (each
    // mod names its own), else the first two letters of the module's name.
    protected string ModuleMark(string module, string label)
    {
        string k = "STR_OZ_VPP_MARK_" + module;
        k.ToUpper();
        string got = Widget.TranslateString("#" + k);
        if (got != "" && got.IndexOf("STR_") == -1)
            return got;
        string nm = Widget.TranslateString(label);
        if (nm.LengthUtf8() <= 2)
            return nm;
        return nm.SubstringUtf8(0, 2);
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

        // One module open at a time: four open modules pushed research's pages under the
        // window's edge on the stand (2026-11-12), and the rail does not scroll.
        int m = m_ModIds.Find(ModuleOf(id));
        if (m != -1)
        {
            for (int k = 0; k < m_ModOpen.Count(); k++)
                m_ModOpen[k] = 0;
            m_ModOpen[m] = 1;
        }

        PaintCrumb();
        LayoutRail();
        OnPaneShown(id);
        RepaintSoon();
    }

    // The title band names the open page: its module, and the page when it has several.
    protected void PaintCrumb()
    {
        TextWidget mod = TextWidget.Cast(M_SUB_WIDGET.FindAnyWidget("VppCrumbMod"));
        TextWidget sep = TextWidget.Cast(M_SUB_WIDGET.FindAnyWidget("VppCrumbSep"));
        TextWidget page = TextWidget.Cast(M_SUB_WIDGET.FindAnyWidget("VppCrumbPage"));
        if (!mod || !sep || !page)
            return;

        int i = m_TabIds.Find(m_Shown);
        if (i < 0)
        {
            mod.SetText("");
            sep.SetText("");
            page.SetText("");
            return;
        }

        mod.SetText(Widget.TranslateString(ModuleLabelOf(m_Shown)));
        if (PagesOf(m_PageModule[i]) > 1)
        {
            sep.SetText("›");
            page.SetText(Widget.TranslateString(m_PageLabel[i]));
        }
        else
        {
            sep.SetText("");
            page.SetText("");
        }
        FlowCrumb();
    }

    // The crumb's words stand one after another, whatever their length.
    protected void FlowCrumb()
    {
        TextWidget mod = TextWidget.Cast(M_SUB_WIDGET.FindAnyWidget("VppCrumbMod"));
        TextWidget sep = TextWidget.Cast(M_SUB_WIDGET.FindAnyWidget("VppCrumbSep"));
        TextWidget page = TextWidget.Cast(M_SUB_WIDGET.FindAnyWidget("VppCrumbPage"));
        if (!mod || !sep || !page)
            return;

        float s = UnitScale();
        float x = CRUMB_X * s;
        float y = CRUMB_Y * s;
        float hgt = CRUMB_H * s;
        x = FlowWord(mod, x, y, hgt, 8 * s);
        x = FlowWord(sep, x, y, hgt, 8 * s);
        FlowWord(page, x, y, hgt, 0);
    }

    // One word of a line: put at x, made as wide as its text; returns where the next goes.
    protected float FlowWord(TextWidget t, float x, float y, float hgt, float gap)
    {
        int tw;
        int th;
        t.Update();
        t.GetTextSize(tw, th);
        t.SetPos(x, y);
        t.SetSize(tw + 2, hgt);
        if (tw <= 0)
            return x;
        return x + tw + gap;
    }

    // ---------------------------------------------------------- the title band's chips

    // The server's name, who is in and whether the Discord bridge answers: from the game
    // and from the last overview (asked when the window opens and every quarter minute).
    protected void PaintChips()
    {
        int cMuted = ARGB(255, 160, 159, 153);
        int cOk = ARGB(255, 102, 199, 92);
        int cBad = ARGB(255, 224, 82, 74);

        string host = g_Game.GetHostName();
        Widget chip = M_SUB_WIDGET.FindAnyWidget("VppChipServer");
        if (chip)
            chip.Show(host != "");
        OvText("VppChipServerText", Widget.TranslateString("#STR_OZ_VPP_CHIP_SERVER") + " " + host, cMuted);

        Widget players = M_SUB_WIDGET.FindAnyWidget("VppChipPlayers");
        Widget bridge = M_SUB_WIDGET.FindAnyWidget("VppChipBridge");
        if (players)
            players.Show(m_Ov != null);
        if (bridge)
            bridge.Show(m_Ov != null);
        if (m_Ov)
        {
            OvText("VppChipPlayersText", Widget.TranslateString("#STR_OZ_VPP_CHIP_PLAYERS") + " " + m_Ov.Players.ToString(), cOk);
            ChipDot("VppChipPlayersDot", cOk);
            if (m_Ov.Bridge == OZ_AdminBridge.UP)
            {
                OvText("VppChipBridgeText", "#STR_OZ_VPP_CHIP_BRIDGE", cOk);
                ChipDot("VppChipBridgeDot", cOk);
            }
            else if (m_Ov.Bridge == OZ_AdminBridge.DOWN)
            {
                OvText("VppChipBridgeText", "#STR_OZ_VPP_CHIP_BRIDGE_DOWN", cBad);
                ChipDot("VppChipBridgeDot", cBad);
            }
            else
            {
                OvText("VppChipBridgeText", "#STR_OZ_VPP_CHIP_BRIDGE_OFF", cMuted);
                ChipDot("VppChipBridgeDot", cMuted);
            }
        }
        FitChips();
    }

    protected void ChipDot(string name, int color)
    {
        Widget dot = M_SUB_WIDGET.FindAnyWidget(name);
        if (dot)
            dot.SetColor(color);
    }

    // The chips stand right to left before the search box, each as wide as its words.
    protected void FitChips()
    {
        float s = UnitScale();
        float right = SEARCH_X * s + (m_WinW - DEF_W * s) - 16 * s;
        right = FitChip("VppChipBridge", true, right, s);
        right = FitChip("VppChipPlayers", true, right, s);
        FitChip("VppChipServer", false, right, s);
    }

    protected float FitChip(string name, bool dot, float right, float s)
    {
        Widget chip = M_SUB_WIDGET.FindAnyWidget(name);
        TextWidget t = TextWidget.Cast(M_SUB_WIDGET.FindAnyWidget(name + "Text"));
        Widget bg = M_SUB_WIDGET.FindAnyWidget(name + "Bg");
        if (!chip || !t || !chip.IsVisible())
            return right;

        int tw;
        int th;
        t.Update();
        t.GetTextSize(tw, th);
        float lead = 10 * s;
        if (dot)
            lead = 21 * s;
        float w = lead + tw + 10 * s;
        chip.SetPos(right - w, CHIP_Y * s);
        chip.SetSize(w, CHIP_H * s);
        if (bg)
            bg.SetSize(w, CHIP_H * s);
        t.SetPos(lead, 4 * s);
        t.SetSize(tw + 2, 15 * s);
        return right - w - 8 * s;
    }

    // The search box of the title band finds a page by its name or its module's name and
    // opens the first that fits (two letters or more).
    protected void FollowSearch()
    {
        if (!m_SearchBox)
            return;
        string text = m_SearchBox.GetText();
        if (text == m_SearchSeen)
            return;
        m_SearchSeen = text;

        // OZ_VppFold, not ToLower: the engine's ToLower makes every Cyrillic byte a space.
        string want = text;
        want.TrimInPlace();
        want = OZ_VppFold.Lower(want);
        if (want.LengthUtf8() < 2)
            return;

        int i;
        string have;
        for (i = 0; i < m_TabIds.Count(); i++)
        {
            have = OZ_VppFold.Lower(Widget.TranslateString(m_PageLabel[i]));
            if (have.IndexOf(want) != -1 && m_TabIds[i] != m_Shown)
            {
                ShowPane(m_TabIds[i]);
                return;
            }
        }
        for (i = 0; i < m_TabIds.Count(); i++)
        {
            have = OZ_VppFold.Lower(Widget.TranslateString(ModuleLabelOf(m_TabIds[i])));
            if (have.IndexOf(want) != -1)
            {
                if (ModuleOf(m_Shown) != m_PageModule[i])
                    ShowPane(m_TabIds[i]);
                return;
            }
        }
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
        if (IsFilesPage(id))
        {
            // The list as it is known already, with this module's files first; the
            // answer of cfg_list builds it again.
            if (m_CfgNames.Count() > 0)
                RebuildRawList();
            Ask(OZ_AdminSect.CONFIG, "cfg_list", "{}");
            RawPaintStatus(false);
        }
        if (id == "ffr")
            FfrAsk();
        if (id == "mirror")
        {
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

        // What the core's pages lay out by script is laid out for the page now shown: it
        // may have been built, or the window resized, while it was hidden. Now, and on
        // the next frames (Tick), when its texts have been drawn and know their widths.
        CoreRefit(id);
        m_CoreFitDue = 2;
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
        PaintChips();
        PaintFoot();
        m_ChipsAt = GetGame().GetTime();
        OZ_Rpc.AdminRequest(OZ_AdminSect.CONFIG, "overview", "{}");

        Ask(OZ_AdminSect.CONFIG, "cfg_list", "{}");

        // The source of factions, once per window (it cannot change while the server
        // runs): the spawns page offers its faction ids, and the factions module shows
        // FFR's groups when the FFR glue is that source (FfrRegister).
        if (!m_IdAsked)
        {
            m_IdAsked = true;
            Ask(OZ_AdminSect.CONFIG, "identity", "{}");
        }

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

    // The window's new size in pixels. Its own parts follow by the rule its layout carries
    // (m_Flex); every page gets the content area; what is fitted to its text (the crumb,
    // the chips, the status bar's right half) is fitted again; glues hear OnWindowResized.
    protected void ApplyWindowSize(float w, float h)
    {
        float s = UnitScale();
        if (w < MIN_W * s)
            w = MIN_W * s;
        if (h < MIN_H * s)
            h = MIN_H * s;

        m_WinW = w;
        m_WinH = h;
        m_Sized = true;
        m_Flex.Resize(M_SUB_WIDGET, w, h, DEF_W * s, DEF_H * s);

        float cw = w - (DEF_W - CONTENT_W) * s;
        float ch = h - (DEF_H - CONTENT_H) * s;
        Widget pane;
        Widget before;
        for (int i = 0; i < m_TabIds.Count(); i++)
        {
            pane = m_Panes.Get(m_TabIds[i]);
            if (!pane || pane == before)
                continue;
            before = pane;
            PlacePane(pane);
        }

        if (m_ConfirmBar && m_ConfirmBar.IsVisible())
            PlaceConfirm();
        FlowCrumb();
        FitChips();
        FlowStatus();

        // The journal's rows and its box follow the page (a glue's OnWindowResized may
        // not call super, so this is not left to that hook).
        if (m_JrRows)
        {
            m_JrRows.Fit();
            PlaceJournalSigns();
            FitJournalBox();
        }

        OnWindowResized(cw, ch);
    }

    // How far above the page's bottom the confirm strip stands, in layout units: a page
    // with a bar of its own at its bottom (the research editor's draft bar) lifts it.
    protected float ConfirmLift()
    {
        return 0;
    }

    protected void PlaceConfirm()
    {
        if (!m_ConfirmBar)
            return;
        float s = UnitScale();
        float cw = m_WinW - (DEF_W - CONTENT_W) * s;
        float ch = m_WinH - (DEF_H - CONTENT_H) * s;
        m_Flex.Resize(m_ConfirmBar, cw, CONFIRM * s, PAGE_W * s, CONFIRM * s);
        m_ConfirmBar.SetPos(0, ch - (CONFIRM + ConfirmLift()) * s);
    }

    // The rail's foot: where this window lives and who is looking.
    protected void PaintFoot()
    {
        int cMuted = ARGB(255, 160, 159, 153);
        OvText("RailFootA", "#STR_OZ_VPP_FOOT_A", cMuted);
        string who = "";
        PlayerBase me = PlayerBase.Cast(GetGame().GetPlayer());
        if (me && me.GetIdentity())
            who = me.GetIdentity().GetName();
        OvText("RailFootB", Widget.TranslateString("#STR_OZ_VPP_FOOT_B") + " " + who, cMuted);
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

    // The class fields of the window's pages for the live search (OZ_ClassPick): a glue
    // overrides this, calls super and binds its own edit boxes by name, with the kinds of
    // classes each takes (OZ_ClassIndex.ITEM, OBJECT, VEHICLE) and whether it holds a list.
    void ClassFields(OZ_ClassPick pick)
    {
    }

    bool IsOpen()
    {
        return M_SUB_WIDGET && M_SUB_WIDGET.IsVisible();
    }

    // Every frame while the window is open (MissionGameplay.OnUpdate below): a request
    // that waited too long is closed, and the status bar and the journal follow it.
    void Tick()
    {
        // The live class search: the class index a slice a frame, the list after the
        // focused class field.
        if (m_ClassPick)
            m_ClassPick.Tick();

        // The open page's own frame work: the search follows typing, the scheme
        // follows the admin twice a second.
        if (m_Shown == "players")
            PlrFollowFind();
        if (m_Shown == "news")
            NwFollow();
        if (IsFilesPage(m_Shown))
            RawFollow();
        if (m_Shown == "ffr")
            FfrFollow();
        if (m_Shown == "spawns" && GetGame().GetTime() - m_SpDrawnAt >= 500)
            SpDrawScheme();

        // The core's own pages follow the window: when its size changed since the open
        // page was last fitted, the page's lists and flowed words are fitted again, on
        // this frame and the two after it.
        if (m_WinW != m_CoreFitW || m_WinH != m_CoreFitH)
        {
            m_CoreFitW = m_WinW;
            m_CoreFitH = m_WinH;
            m_CoreFitDue = 3;
        }
        if (m_CoreFitDue > 0)
        {
            m_CoreFitDue--;
            CoreRefit(m_Shown);
        }

        // The title band: the search follows typing, placeholders their boxes (seven
        // times a second), the chips ask the server every quarter minute.
        FollowSearch();
        int now = GetGame().GetTime();
        if (now - m_PhAt >= 150)
        {
            m_PhAt = now;
            FollowPlaceholders();
        }
        if (now - m_ChipsAt >= 15000)
        {
            m_ChipsAt = now;
            OZ_Rpc.AdminRequest(OZ_AdminSect.CONFIG, "overview", "{}");
        }

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

    // Another admin's request, told by the server (design §3.2, phase 6). Reads stay out,
    // as this window's own do.
    void OnAdminEcho(string who, string sectionId, string op, bool ok, string error)
    {
        if (IsReadOp(op))
            return;
        OZ_VppJournal.Other(who, sectionId, op, ok, error);
    }

    protected string JournalLine(OZ_VppJournalEntry e)
    {
        string line = e.Time;
        if (e.Who != "")
            line += "   [" + e.Who + "]";
        line += "   " + OZ_VppJournal.Text(e.Module);
        line += "   " + OZ_VppJournal.Text(e.What);
        if (e.Result != "")
            line += "   ->   " + OZ_VppJournal.Text(e.Result);
        return line;
    }

    // The status bar: how the last request stands, what it said, the config problems the
    // last overview counted and the last thing done.
    protected void PaintStatus()
    {
        if (!M_SUB_WIDGET)
            return;
        int cMuted = ARGB(255, 160, 159, 153);
        int cOk = ARGB(255, 102, 199, 92);
        int cAlert = ARGB(255, 245, 199, 64);

        OZ_VppJournalEntry e = OZ_VppJournal.Last();
        string state = "#STR_OZ_VPP_ST_READY";
        int c = cOk;
        string said = "";
        string last = "";
        if (e)
        {
            c = LevelColor(e.Level);
            if (e.Level == OZ_VppJournal.LV_WAIT)
                state = "#STR_OZ_VPP_ST_WAIT";
            else if (e.Level == OZ_VppJournal.LV_ERR)
                state = "#STR_OZ_VPP_ST_ERR";
            else if (e.Level == OZ_VppJournal.LV_WARN)
                state = "#STR_OZ_VPP_ST_WARN";
            else
                c = cOk;
            said = OZ_VppJournal.Text(e.Module) + ": " + OZ_VppJournal.Text(e.What);
            if (e.Result != "")
                said = said + " -> " + OZ_VppJournal.Text(e.Result);
            last = Widget.TranslateString("#STR_OZ_VPP_ST_LAST") + " " + e.Time;
        }
        OvText("StatusState", state, c);
        ChipDot("StatusDot", c);
        m_StatusSaid = said;
        OvText("StatusText", said, cMuted);
        OvText("StatusLast", last, cMuted);

        string problems = Widget.TranslateString("#STR_OZ_VPP_ST_PROBLEMS") + " ";
        if (m_Ov)
        {
            problems = problems + m_Ov.Problems.ToString() + " ›";
            if (m_Ov.Problems > 0)
                OvText("StatusCount", problems, cAlert);
            else
                OvText("StatusCount", problems, cMuted);
        }
        else
            OvText("StatusCount", problems + "…", cMuted);
        FlowStatus();
    }

    // The bar's words stand by their length: the state and what was said from the left,
    // the last action and the problems from the right; what was said is cut where the
    // right half begins.
    protected void FlowStatus()
    {
        TextWidget state = TextWidget.Cast(M_SUB_WIDGET.FindAnyWidget("StatusState"));
        TextWidget said = TextWidget.Cast(M_SUB_WIDGET.FindAnyWidget("StatusText"));
        TextWidget last = TextWidget.Cast(M_SUB_WIDGET.FindAnyWidget("StatusLast"));
        TextWidget count = TextWidget.Cast(M_SUB_WIDGET.FindAnyWidget("StatusCount"));
        Widget btn = M_SUB_WIDGET.FindAnyWidget("BtnVppProblems");
        if (!state || !said || !last || !count || !btn)
            return;

        float s = UnitScale();
        float y = 12 * s;
        float hgt = 15 * s;
        float barW = BAR_W * s + (m_WinW - DEF_W * s);
        float left = FlowWord(state, 27 * s, y, hgt, 16 * s);

        int tw;
        int th;
        last.Update();
        last.GetTextSize(tw, th);
        float right = barW - 16 * s - tw;
        last.SetPos(right, y);
        last.SetSize(tw + 2, hgt);
        if (tw > 0)
            right = right - 16 * s;

        count.Update();
        count.GetTextSize(tw, th);
        right = right - tw;
        btn.SetPos(right, y);
        btn.SetSize(tw + 2, hgt);
        count.SetPos(0, 0);
        count.SetSize(tw + 2, hgt);
        right = right - 16 * s;

        // What was said: as much of it as fits.
        float room = right - left;
        if (room < 0)
            room = 0;
        said.SetPos(left, y);
        said.SetSize(room, hgt);
        said.SetText(m_StatusSaid);
        said.Update();
        said.GetTextSize(tw, th);
        if (tw > room && tw > 0)
        {
            string full = m_StatusSaid;
            int n = full.LengthUtf8();
            float f = n;
            int keep = f * room / tw;
            keep = keep - 2;
            if (keep < 1)
                keep = 1;
            if (keep < n)
                said.SetText(full.SubstringUtf8(0, keep) + "…");
        }
    }

    // Ядро › Огляд: the journal, newest first; "errors" keeps the failed ones only, "only
    // mine" leaves the other admins' out. A row per entry (oz_vpp_journal_row.layout); the
    // journal's box is as tall as its rows and the page scrolls.
    protected void RebuildJournal()
    {
        Widget host = M_SUB_WIDGET.FindAnyWidget("OvJournal");
        if (!host)
            return;
        if (!m_JrRows)
            m_JrRows = new OZ_VppFlexRows(m_Flex, host, "OpenZone_VPP/gui/layouts/oz_vpp_journal_row.layout", "OZ_VppJournalRow", 1310, 33);

        m_JrRows.Clear();
        int errors = 0;
        OZ_VppJournalEntry e;
        Widget row;
        for (int i = OZ_VppJournal.Count() - 1; i >= 0; i--)
        {
            e = OZ_VppJournal.At(i);
            if (e.Level == OZ_VppJournal.LV_ERR)
                errors++;
            if (m_JournalErrors && e.Level != OZ_VppJournal.LV_ERR)
                continue;
            if (m_JournalMine && e.Who != "")
                continue;
            if (m_JrRows.Count() >= 120)
                continue;
            row = m_JrRows.Add(i.ToString());
            if (row)
                PaintJournalRow(row, e);
        }
        m_JrRows.Fit();
        PlaceJournalSigns();

        string errWord = Widget.TranslateString("#STR_OZ_VPP_JR_ERRORS");
        OvText("BtnJrErrText", errWord + " · " + errors.ToString(), ARGB(255, 160, 159, 153));
        PaintJournalFilter();
        FitJournalBox();
    }

    protected void RowText(Widget row, string name, string text, int color)
    {
        TextWidget t = TextWidget.Cast(row.FindAnyWidget(name));
        if (!t)
            return;
        t.SetText(text);
        t.SetColor(color);
    }

    protected void PaintJournalRow(Widget row, OZ_VppJournalEntry e)
    {
        int cInk = ARGB(255, 224, 223, 218);
        int cMuted = ARGB(255, 160, 159, 153);
        int c = LevelColor(e.Level);

        string who = e.Who;
        if (who == "")
        {
            PlayerBase me = PlayerBase.Cast(GetGame().GetPlayer());
            if (me && me.GetIdentity())
                who = me.GetIdentity().GetName();
        }
        RowText(row, "JrTime", e.Time, cMuted);
        RowText(row, "JrModule", OZ_VppJournal.Text(e.Module), cMuted);
        RowText(row, "JrWho", who, cMuted);
        RowText(row, "JrWhat", OZ_VppJournal.Text(e.What), cInk);
        RowText(row, "JrResult", OZ_VppJournal.Text(e.Result), c);

        // The sign before the result: done, failed, waiting; nothing for a plain note.
        ImageWidget sign = ImageWidget.Cast(row.FindAnyWidget("JrResultIcon"));
        if (!sign)
            return;
        string image = "";
        if (e.Level == OZ_VppJournal.LV_OK)
            image = "check";
        else if (e.Level == OZ_VppJournal.LV_ERR)
            image = "cross";
        else if (e.Level == OZ_VppJournal.LV_WAIT)
            image = "ring";
        sign.Show(image != "" && e.Result != "");
        if (image != "")
        {
            sign.LoadImageFile(0, "set:ozv_ui image:" + image);
            sign.SetImage(0);
            sign.SetColor(c);
        }
    }

    // A result is written from the right; its sign stands before its first letter.
    protected void PlaceJournalSigns()
    {
        if (!m_JrRows)
            return;
        float s = UnitScale();
        float right = m_JrRows.Width() - 14 * s;
        Widget row;
        Widget sign;
        TextWidget res;
        int tw;
        for (int i = 0; i < m_JrRows.Count(); i++)
        {
            row = m_JrRows.Row(i);
            sign = row.FindAnyWidget("JrResultIcon");
            res = TextWidget.Cast(row.FindAnyWidget("JrResult"));
            if (!sign || !res)
                continue;
            tw = OZ_VppFlex.TextWidth(res);
            sign.SetPos(right - tw - 16 * s, 11 * s);
        }
    }

    // The three filters: the picked one lit.
    protected void PaintJournalFilter()
    {
        bool all = !m_JournalErrors && !m_JournalMine;
        PaintPill("BtnJrAll", all);
        PaintPill("BtnJrErr", m_JournalErrors);
        PaintPill("BtnJrMine", m_JournalMine);
    }

    // A pill of a filter row: lit (the accent's soft ground, bright words) or not (a
    // frame, muted words). Its parts are <name>Edge, <name>Bg and <name>Text.
    protected void PaintPill(string name, bool lit)
    {
        Widget t = M_SUB_WIDGET.FindAnyWidget(name + "Text");
        if (lit)
        {
            OZ_VppFlex.Paint(M_SUB_WIDGET, name + "Edge", ARGB(255, 53, 35, 22));
            OZ_VppFlex.Paint(M_SUB_WIDGET, name + "Bg", ARGB(255, 53, 35, 22));
            if (t)
                t.SetColor(ARGB(255, 224, 223, 218));
            return;
        }
        OZ_VppFlex.Paint(M_SUB_WIDGET, name + "Edge", ARGB(255, 60, 60, 58));
        OZ_VppFlex.Paint(M_SUB_WIDGET, name + "Bg", ARGB(255, 19, 19, 18));
        if (t)
            t.SetColor(ARGB(255, 160, 159, 153));
    }

    // The journal's box as tall as its rows, and the page's scrolled part as tall as what
    // it holds. Called after the rows change and after a resize (the resize rule puts the
    // box back to its drawn height).
    protected void FitJournalBox()
    {
        Widget box = M_SUB_WIDGET.FindAnyWidget("OvJournalBox");
        Widget edge = M_SUB_WIDGET.FindAnyWidget("OvJournalBoxEdge");
        Widget bg = M_SUB_WIDGET.FindAnyWidget("OvJournalBoxBg");
        Widget body = M_SUB_WIDGET.FindAnyWidget("OvBodyBody");
        Widget scroll = M_SUB_WIDGET.FindAnyWidget("OvBodyScroll");
        if (!box || !m_JrRows || !body || !scroll)
            return;

        float s = UnitScale();
        int n = m_JrRows.Count();
        float w = m_JrRows.Width() + 2 * s;
        float h = (51 + n * 33) * s;
        box.SetSize(w, h);
        if (edge)
            edge.SetSize(w, h);
        if (bg)
            bg.SetSize(w - 2 * s, h - 2 * s);

        // The box stands 361 units down the page; 18 under it.
        float sw;
        float sh;
        scroll.GetSize(sw, sh);
        float need = (361 + 18) * s + h;
        if (need < sh)
            need = sh;
        if (sw > 0)
            body.SetSize(sw, need);
        scroll.Update();
    }

    // ---------------------------------------------------------- Ядро › Огляд
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
        PaintChips();
        PaintStatus();
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
            players = players + " " + Widget.TranslateString("#STR_OZ_VPP_OV_OF") + " " + m_Ov.MaxPlayers.ToString();
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
        // The cards in the rail's order (the server lists modules as they started), the
        // modules without pages here after them.
        m_OvOrder.Clear();
        int mi;
        int ci;
        for (mi = 0; mi < m_ModIds.Count(); mi++)
        {
            for (ci = 0; ci < m_Ov.Modules.Count(); ci++)
            {
                if (m_Ov.Modules[ci].Id == m_ModIds[mi] && m_OvOrder.Find(ci) == -1)
                    m_OvOrder.Insert(ci);
            }
        }
        for (ci = 0; ci < m_Ov.Modules.Count(); ci++)
        {
            if (m_OvOrder.Find(ci) == -1)
                m_OvOrder.Insert(ci);
        }
        for (int k = 0; k < OV_CARDS; k++)
        {
            Widget card = M_SUB_WIDGET.FindAnyWidget("OvCard" + k.ToString());
            if (!card)
                continue;
            if (k >= m_OvOrder.Count())
            {
                card.Show(false);
                continue;
            }
            card.Show(true);
            PaintOvCard(k, m_Ov.Modules[m_OvOrder[k]]);
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

        // The chip hugs its word and stands 8 units after the module's name.
        float s = UnitScale();
        TextWidget nameW = TextWidget.Cast(M_SUB_WIDGET.FindAnyWidget("OvCardName" + n));
        Widget chip = M_SUB_WIDGET.FindAnyWidget("OvCardChip" + n);
        Widget chipBg = M_SUB_WIDGET.FindAnyWidget("OvCardChip" + n + "Bg");
        TextWidget stateW = TextWidget.Cast(M_SUB_WIDGET.FindAnyWidget("OvCardState" + n));
        if (nameW && chip && stateW)
        {
            int nameTw = OZ_VppFlex.TextWidth(nameW);
            nameW.SetSize(nameTw + 2, 21 * s);
            OZ_VppFlex.Hug(chip, chipBg, stateW, 8 * s);
            chip.SetPos(15 * s + nameTw + 8 * s, 16 * s);
        }

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

        // The facts on one line, a dot between them.
        string facts = "";
        for (int i = 0; i < c.Facts.Count(); i++)
        {
            OZ_AdminFact f = c.Facts[i];
            string lab = OvTr(f.Label);
            string val = OvTr(f.Value);
            string one = lab + ": ";
            one = one + val;
            if (facts != "")
                facts = facts + " · ";
            facts = facts + one;
        }
        OvText("OvCardFacts" + n, facts, inkMuted);
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
    // The strip is red, as the mockup draws it for what removes or wipes.
    protected void Confirm(string text, string yesLabel, string action)
    {
        AskStrip(text, yesLabel, action, false);
    }

    // The same strip for what applies rather than removes (the TraderPlus import): in
    // the accent, as the mockup draws that one.
    protected void ConfirmApply(string text, string yesLabel, string action)
    {
        AskStrip(text, yesLabel, action, true);
    }

    protected void AskStrip(string text, string yesLabel, string action, bool apply)
    {
        if (!m_ConfirmBar)
        {
            OZ_Log.Error("vpp confirm: the confirm strip's layout did not load; nothing is done");
            return;
        }

        int ground = ARGB(255, 37, 20, 19);
        int mark = ARGB(255, 224, 82, 74);
        if (apply)
        {
            ground = ARGB(255, 47, 29, 17);
            mark = ARGB(255, 232, 120, 43);
        }
        OZ_VppFlex.Paint(m_ConfirmBar, "OZ_VppConfirmBg", ground);
        OZ_VppFlex.Paint(m_ConfirmBar, "OZ_VppConfirmLine", mark);
        OZ_VppFlex.Paint(m_ConfirmBar, "BtnConfirmYesBg", mark);

        m_ConfirmAction = action;
        TextWidget t = TextWidget.Cast(m_ConfirmBar.FindAnyWidget("ConfirmText"));
        if (t)
            t.SetText(text);
        TextWidget yes = TextWidget.Cast(m_ConfirmBar.FindAnyWidget("BtnConfirmYesText"));
        if (yes)
            yes.SetText(yesLabel);
        m_ConfirmBar.Show(true);
        PlaceConfirm();
    }

    protected void CloseConfirm()
    {
        m_ConfirmAction = "";
        if (m_ConfirmBar)
            m_ConfirmBar.Show(false);
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

        // Фракції › Групи FFR.
        if (action == "ffr:apply")
        {
            FfrApply();
            return;
        }
        if (action == "ffr:discard")
        {
            FfrDiscard();
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
        if (p && !IsFlexPane(p))
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
        array<string> reads = {"cfg_list", "mirror_list", "players_here", "news_voices", "roster", "list", "overview", "identity", "ffr_groups", "ffr_reread"};
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

        if (sectionId == FFR_SECTION)
        {
            OnFfrAnswer(op, ok, json, error);
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
                PaintMirror();
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
            if (JsonFileLoader<OZ_MirrorReport>.LoadData(json, mrep, rerr) && mrep)
            {
                // ЗНІМАЄМО ВСЕ ВІДРАЗУ, а вже тоді складаємо рядок: складання
                // рядка -- це виділення пам'яті, а конверт виділив серіалізатор.
                string kind    = mrep.Kind;
                bool   on      = mrep.On;
                int    pushed  = mrep.Pushed;
                int    skipped = mrep.Skipped;
                int    failed  = mrep.Failed;
                string note    = mrep.Note;

                // Рядок цього роду -- по імені: рід приходить з відповіді, і
                // сторінка не мусить знати жодного імені наперед.
                int row = m_MirrorKinds.Find(kind);
                if (row != -1)
                {
                    m_MirrorKnown = true;
                    if (on)
                        m_MirrorOn.Set(row, 1);
                    else
                        m_MirrorOn.Set(row, 0);
                }
                MrReport(kind, on, pushed, skipped, failed, note);
            }
            PaintMirror();

            // Речення «що буде, як натиснути» складає СЕРВЕР і воно щойно
            // застаріло -- стан протилежний. Перепитуємо.
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
                RegisterFilesPages();
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
            string setName = op.Substring(8, op.Length() - 8);
            if (setName == FFR_CFG)
                m_FfrApplied = true;
            string applied = OvTr("#STR_OZ_VPP_FL_APPLIED");
            Hint(applied + " " + setName);
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
            // No answer about the signatures or about the feed: the place of the newest
            // posts says so (the reason is in the journal), and the feed is not asked
            // from a bridge that has just refused.
            if (op == OZ_NewsOp.VOICES || op == OZ_NewsOp.LIST)
            {
                NwRecentSay("#STR_OZ_VPP_NW_RECENT_DOWN");
                return;
            }

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

            // The bridge answers: now the feed (asked only then, so a silent bridge
            // costs the journal one line, not two).
            NwAskRecent();
            return;
        }

        if (op == OZ_NewsOp.LIST)
        {
            // Root made by the script, read through a copy made by the script (the
            // core's OZ_ConfigBase header): the rows are built from the copy.
            OZ_VppNewsList got = new OZ_VppNewsList();
            string gerr;
            if (!JsonFileLoader<OZ_VppNewsList>.LoadData(json, got, gerr) || !got)
            {
                NwRecentSay("#STR_OZ_VPP_NW_RECENT_DOWN");
                return;
            }
            NwTakeRecent(got.Copy());
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

            // The feed has a post more.
            NwAskRecent();
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
        string n = OZ_VppFold.Lower(name);
        bool hit = false;
        if (n.IndexOf(low) != -1)
            hit = true;
        if (uid.IndexOf(low) != -1)
            hit = true;
        return hit;
    }

    // Every frame on this page (Tick): the list follows what is typed, and seventeen
    // digits pick that Steam ID at once, in the game or not. The card's head is laid out
    // again for a few frames after it was painted.
    protected void PlrFollowFind()
    {
        if (m_PlrFlowDue > 0)
        {
            m_PlrFlowDue--;
            PlrFlowHead();
        }

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
        if (!m_PlrRows)
        {
            Widget host = M_SUB_WIDGET.FindAnyWidget("PlrListRows");
            if (!host)
                return;
            m_PlrRows = new OZ_VppFlexRows(m_Flex, host, "OpenZone_VPP/gui/layouts/oz_vpp_player_row.layout", "OZ_VppPlayerRow", 387, 40);
        }

        string find = PlrFindText();
        string low = OZ_VppFold.Lower(find);
        bool away = PlrIsSteamId(find) && m_PlrUids.Find(find) == -1;

        m_PlrRows.Clear();

        if (away)
        {
            string awayWord = OvTr("#STR_OZ_VPP_PLR_OFFLINE");
            PlrRow(find, awayWord, false);
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
            PlrRow(uid, shown, true);
        }
        m_PlrRows.Fit();

        PlrMarkRows();
        PlrPaintCount();
    }

    // One row: the sign of being in the game (green; faint for a Steam ID that is not),
    // the name, the Steam ID. The row's key is the Steam ID.
    protected void PlrRow(string uid, string name, bool here)
    {
        Widget row = m_PlrRows.Add(uid);
        if (!row)
            return;
        int ink = ARGB(255, 224, 223, 218);
        int muted = ARGB(255, 160, 159, 153);
        int nameInk = ink;
        int dotInk = ARGB(255, 102, 199, 92);
        if (!here)
        {
            nameInk = muted;
            dotInk = ARGB(255, 92, 90, 88);
        }
        RowText(row, "PlrRowName", name, nameInk);
        RowText(row, "PlrRowUid", uid, muted);
        Widget dot = row.FindAnyWidget("PlrRowDot");
        if (dot)
            dot.SetColor(dotInk);
    }

    // The picked player's row is lit: the accent's frame on its soft ground, as the
    // board draws it; the rest lie on the head's colour.
    protected void PlrMarkRows()
    {
        if (!m_PlrRows)
            return;

        Widget row;
        for (int r = 0; r < m_PlrRows.Count(); r++)
        {
            row = m_PlrRows.Row(r);
            if (!row)
                continue;
            if (m_PlrRows.Key(r) == m_PlrPick)
            {
                OZ_VppFlex.Paint(row, "OZ_VppPlayerRowEdge", ARGB(255, 232, 120, 43));
                OZ_VppFlex.Paint(row, "OZ_VppPlayerRowBg", ARGB(255, 47, 29, 17));
            }
            else
            {
                OZ_VppFlex.Paint(row, "OZ_VppPlayerRowEdge", ARGB(255, 19, 19, 18));
                OZ_VppFlex.Paint(row, "OZ_VppPlayerRowBg", ARGB(255, 19, 19, 18));
            }
        }
    }

    // A click somewhere in the list; false: not this list. A player's row opens his
    // card, and the card asks the server for his character; a question already on the
    // confirm strip was about someone else: it goes.
    protected bool PlrRowClick(Widget w)
    {
        if (!m_PlrRows)
            return false;
        int i = m_PlrRows.IndexOf(w);
        if (i == -1)
            return false;
        if (m_ConfirmAction.IndexOf("wipe:") == 0)
            CloseConfirm();
        PlrPick(m_PlrRows.Key(i));
        return true;
    }

    // "ONLINE · 1 OF 60".
    protected void PlrPaintCount()
    {
        string head = OvTr("#STR_OZ_VPP_PLR_ONLINE_CAP");
        string line = head + " · " + m_PlrUids.Count().ToString();
        if (m_PlrMax > 0)
        {
            string of = OvTr("#STR_OZ_VPP_PLR_OF");
            line = line + " " + of;
            line = line + " " + m_PlrMax.ToString();
        }
        OvText("PlrCount", line, ARGB(255, 160, 159, 153));
    }

    // The card: who, whether he is in the game, and the character the server holds.
    protected void PlrPaintCard()
    {
        int ink = ARGB(255, 224, 223, 218);
        int muted = ARGB(255, 160, 159, 153);
        int okInk = ARGB(255, 102, 199, 92);
        int badInk = ARGB(255, 224, 82, 74);

        Widget chip = M_SUB_WIDGET.FindAnyWidget("PlrUidChip");
        Widget dot = M_SUB_WIDGET.FindAnyWidget("PlrStateDot");
        PlrPaintLinks();
        m_PlrFlowDue = 3;

        if (m_PlrPick == "")
        {
            OvText("PlrName", "#STR_OZ_VPP_PLR_NOBODY", muted);
            OvText("PlrUidShown", "", muted);
            OvText("PlrState", "", muted);
            OvText("PlrCharName", "-", muted);
            OvText("PlrGen", "-", muted);
            OvText("PlrSteam", "-", muted);
            if (chip)
                chip.Show(false);
            if (dot)
                dot.Show(false);
            PlrFlowHead();
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
        if (chip)
            chip.Show(true);
        if (dot)
            dot.Show(true);
        if (at != -1)
        {
            OvText("PlrState", "#STR_OZ_VPP_PLR_ONLINE_STATE", okInk);
            if (dot)
                dot.SetColor(okInk);
        }
        else
        {
            OvText("PlrState", "#STR_OZ_VPP_PLR_OFFLINE", muted);
            if (dot)
                dot.SetColor(muted);
        }
        OvText("PlrSteam", m_PlrPick, ink);
        PlrFlowHead();

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

    // The card's head as the board lays it: the name, 12 units after it the Steam ID on
    // a chip as wide as its digits, 12 after that the sign and the word of being in the
    // game. Every part keeps the height the layout gave it.
    protected void PlrFlowHead()
    {
        TextWidget nameW = TextWidget.Cast(M_SUB_WIDGET.FindAnyWidget("PlrName"));
        Widget chip = M_SUB_WIDGET.FindAnyWidget("PlrUidChip");
        Widget chipBg = M_SUB_WIDGET.FindAnyWidget("PlrUidChipBg");
        TextWidget uidW = TextWidget.Cast(M_SUB_WIDGET.FindAnyWidget("PlrUidShown"));
        Widget dot = M_SUB_WIDGET.FindAnyWidget("PlrStateDot");
        TextWidget stateW = TextWidget.Cast(M_SUB_WIDGET.FindAnyWidget("PlrState"));
        if (!nameW || !chip || !uidW || !dot || !stateW)
            return;

        // A text that has not been drawn yet may answer no width: the head keeps the
        // places it has until it does.
        int tw = OZ_VppFlex.TextWidth(nameW);
        if (tw <= 0)
            return;

        float s = UnitScale();
        float x;
        float y;
        float w;
        float h;
        nameW.GetPos(x, y);
        nameW.GetSize(w, h);
        nameW.SetSize(tw + 2, h);
        float at = x + tw + 12 * s;

        if (chip.IsVisible())
        {
            float cw = OZ_VppFlex.Hug(chip, chipBg, uidW, 8 * s);
            chip.GetPos(x, y);
            chip.SetPos(at, y);
            at = at + cw + 12 * s;
        }

        dot.GetPos(x, y);
        dot.SetPos(at, y);
        stateW.GetPos(x, y);
        stateW.SetPos(at + 12 * s, y);
    }

    // A way from a player's card into another page about that player: a title, a line
    // on what is there (both stringtable keys) and the page it opens. A module's glue
    // adds its own after super.OnCreate; the card has PLR_LINKS places (PlrLinkAt).
    protected void AddPlayerLink(string title, string what, string page)
    {
        if (page == "" || m_PlrLinkPage.Find(page) != -1)
            return;
        m_PlrLinkTitle.Insert(title);
        m_PlrLinkWhat.Insert(what);
        m_PlrLinkPage.Insert(page);
    }

    // A link opened its page for this player (Steam ID). The core's own leads to the
    // spawns page and puts the player into its personal point's field; a glue overrides
    // this for its pages and calls super.
    protected void OnPlayerLink(string page, string uid)
    {
        if (page == "spawns")
            SetEdit("SpUid", uid);
    }

    // The link that stands in a place of the card; -1: none. The modules' links come
    // first, in the order their glues added them, and the core's own (the first one
    // added) last, as the board has them; a link whose page is not in this window takes
    // no place.
    protected int PlrLinkAt(int place)
    {
        int n = m_PlrLinkPage.Count();
        int shown = 0;
        int at;
        for (int i = 0; i < n; i++)
        {
            at = i + 1;
            if (at >= n)
                at = 0;
            if (!m_Panes.Contains(m_PlrLinkPage[at]))
                continue;
            if (shown == place)
                return at;
            shown++;
        }
        return -1;
    }

    // The link places of the card: a place per link, the rest hidden; the whole box
    // hidden when there is none.
    protected void PlrPaintLinks()
    {
        int ink = ARGB(255, 224, 223, 218);
        int muted = ARGB(255, 160, 159, 153);
        int shown = 0;
        Widget card;
        string n;
        int at;
        for (int k = 0; k < PLR_LINKS; k++)
        {
            n = k.ToString();
            at = PlrLinkAt(k);
            card = M_SUB_WIDGET.FindAnyWidget("PlrLink" + n);
            if (card)
                card.Show(at != -1);
            if (at == -1)
                continue;
            OvText("PlrLinkTitle" + n, m_PlrLinkTitle[at], ink);
            OvText("PlrLinkWhat" + n, m_PlrLinkWhat[at], muted);
            shown++;
        }

        Widget box = M_SUB_WIDGET.FindAnyWidget("PlrLinks");
        Widget tag = M_SUB_WIDGET.FindAnyWidget("PlrLinksTag");
        if (box)
            box.Show(shown > 0);
        if (tag)
            tag.Show(shown > 0);
    }

    // A link place was clicked: the n-th link whose page is in this window opens its
    // page for the picked player.
    protected void PlrOpenLink(int place)
    {
        if (m_PlrPick == "")
        {
            Hint("#STR_OZ_VPP_PLR_PICK_FIRST");
            return;
        }

        int at = PlrLinkAt(place);
        if (at == -1)
            return;
        string page = m_PlrLinkPage[at];
        string uid = m_PlrPick;
        ShowPane(page);
        OnPlayerLink(page, uid);
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

    // Every frame on this page (Tick): the preview and the byte counter follow typing,
    // and the card is fitted to its text for a few frames after the text changed.
    protected void NwFollow()
    {
        if (m_NwFitDue > 0)
        {
            m_NwFitDue--;
            NwFitCard();
        }

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

        int accent = ARGB(255, 232, 120, 43);
        int alert = ARGB(255, 245, 199, 64);
        int bad = ARGB(255, 224, 82, 74);
        int ink = ARGB(255, 224, 223, 218);
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

        // The bar: its filled part (a frame and its ground) as wide as the text's share
        // of the ceiling.
        Widget track = M_SUB_WIDGET.FindAnyWidget("NwBytes");
        Widget fill = M_SUB_WIDGET.FindAnyWidget("NwBytesFill");
        Widget fillBg = M_SUB_WIDGET.FindAnyWidget("NwBytesFillBg");
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
            if (fillBg)
                fillBg.SetSize(tw * part, th);
            OZ_VppFlex.Paint(M_SUB_WIDGET, "NwBytesFillBg", barInk);
        }

        // The PDA's own blue, as the board's card has it; the texts only.
        int pdaText = ARGB(255, 230, 237, 242);
        int pdaMuted = ARGB(255, 173, 180, 186);
        if (title.Trim() == "")
            OvText("NwPrevTitle", "#STR_OZ_VPP_NW_PREV_TITLE", pdaMuted);
        else
            OvText("NwPrevTitle", title, pdaText);

        string who = PickedVoice();
        if (who == "")
            who = m_NwSelf;
        string now = OvTr("#STR_OZ_VPP_NW_JUST_NOW");
        string byline = now;
        if (who != "")
            byline = who + " · " + now;
        OvText("NwPrevWho", byline, pdaMuted);

        if (body.Trim() == "")
            OvText("NwPrevBody", "#STR_OZ_VPP_NW_PREV_BODY", pdaMuted);
        else
            OvText("NwPrevBody", body, pdaText);

        // The card follows its text: now and on the next frames (NwFollow).
        m_NwFitDue = 3;
        NwFitCard();
    }

    // The preview's card is as tall as its text, as the board draws it: the text's box
    // takes the height its lines need (its four designed lines at least, and no more
    // than the column has room for), the card ends 19 units under the text and the note
    // stands 13 under the card. The numbers are those of oz_vpp_pane_news.layout
    // (converted from the board CoreNews): convert again, check these.
    protected void NwFitCard()
    {
        Widget mid = M_SUB_WIDGET.FindAnyWidget("NwMid");
        Widget card = M_SUB_WIDGET.FindAnyWidget("NwCard");
        Widget edge = M_SUB_WIDGET.FindAnyWidget("NwCardEdge");
        Widget bg = M_SUB_WIDGET.FindAnyWidget("NwCardBg");
        Widget note = M_SUB_WIDGET.FindAnyWidget("NwPrevNote");
        RichTextWidget body = RichTextWidget.Cast(M_SUB_WIDGET.FindAnyWidget("NwPrevBody"));
        if (!mid || !card || !body)
            return;

        float s = UnitScale();
        float mw;
        float mh;
        mid.GetSize(mw, mh);

        // The card stands 42 units down the column and the text 112 down the card;
        // under the card: 13, the note's 36 and 16 of the column's own room.
        float designed = 86 * s;
        float room = mh - (42 + 112 + 19 + 13 + 36 + 16) * s;
        body.Update();
        float need = body.GetContentHeight() + 4 * s;
        if (need < designed)
            need = designed;
        if (need > room)
            need = room;
        if (need < designed)
            need = designed;

        // Widths stay as the layout has them; only the heights follow the text.
        float bw;
        float bh;
        float cardW;
        float cardH;
        body.GetSize(bw, bh);
        card.GetSize(cardW, cardH);
        if (bw <= 0 || cardW <= 0)
            return;
        cardH = (112 + 19) * s + need;
        body.SetSize(bw, need);
        card.SetSize(cardW, cardH);
        if (edge)
            edge.SetSize(cardW, cardH);
        if (bg)
            bg.SetSize(cardW - 2 * s, cardH - 2 * s);
        if (note)
            note.SetPos(20 * s, (42 + 13) * s + cardH);
    }

    // The newest posts are asked from the bridge. A read asked all the time: it goes
    // past Ask(), which would write every one of them into the journal as an action
    // (a failed answer reaches the journal all the same, OZ_VppJournal.Answered).
    protected void NwAskRecent()
    {
        if (!m_NwRows || m_NwRows.Count() == 0)
            NwRecentSay("#STR_OZ_VPP_NW_RECENT_ASKING");
        OZ_Rpc.AdminRequest(OZ_AdminSect.NEWS, OZ_NewsOp.LIST, "{}");
    }

    // The words beside the "newest" heading: where the list stands.
    protected void NwRecentSay(string key)
    {
        OvText("NwRecentNote", key, ARGB(255, 160, 159, 153));
    }

    // "2026-11-12 13:05:44" (the bridge's stamp, UTC) as "12.11 13:05"; anything else
    // as it came.
    protected string NwWhen(string at)
    {
        if (at.Length() < 16)
            return at;
        string day = at.Substring(8, 2) + "." + at.Substring(5, 2);
        return day + " " + at.Substring(11, 5);
    }

    // The feed's first page came: a row per post (its title; who signed it and when),
    // and in the page's head how many there are and when the newest was written.
    protected void NwTakeRecent(OZ_VppNewsList list)
    {
        if (!list)
            return;
        if (!m_NwRows)
        {
            Widget host = M_SUB_WIDGET.FindAnyWidget("NwRecentRows");
            if (!host)
                return;
            m_NwRows = new OZ_VppFlexRows(m_Flex, host, "OpenZone_VPP/gui/layouts/oz_vpp_news_row.layout", "OZ_VppNewsRow", 400, 57);
        }

        int ink = ARGB(255, 224, 223, 218);
        int muted = ARGB(255, 160, 159, 153);
        m_NwRows.Clear();
        OZ_VppNewsItem it;
        Widget row;
        string meta;
        for (int i = 0; i < list.Items.Count(); i++)
        {
            it = list.Items[i];
            row = m_NwRows.Add(it.Id);
            if (!row)
                continue;
            meta = NwWhen(it.At);
            if (it.Who != "")
                meta = it.Who + "   " + meta;
            RowText(row, "NwRowTitle", it.Title, ink);
            RowText(row, "NwRowMeta", meta, muted);
        }
        m_NwRows.Fit();

        int n = list.Items.Count();
        if (n == 0)
        {
            NwRecentSay("#STR_OZ_VPP_NW_RECENT_NONE");
            OvText("NwCount", "", muted);
            return;
        }
        NwRecentSay("#STR_OZ_VPP_NW_RECENT_FROM");

        // "Posts: 12 · the newest 12.11 13:05"; a plus when the feed goes on past this page.
        string countWord = OvTr("#STR_OZ_VPP_NW_COUNT");
        string lastWord = OvTr("#STR_OZ_VPP_NW_LAST");
        string head = countWord + " " + n.ToString();
        if (list.Next != "")
            head = head + "+";
        head = head + " · " + lastWord;
        head = head + " " + NwWhen(list.Items[0].At);
        OvText("NwCount", head, muted);
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
        if (name == FFR_CFG)
            FfrTakeSettings(body);

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

        // The files page shows what the server saved; the FFR page reads its file and
        // FFR's groups again.
        if (IsFilesPage(CurrentPane()) && m_RawPicked != "")
            AskCfg(m_RawPicked);
        if (CurrentPane() == "ffr")
            FfrAsk();
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

        if (m_IdSource == FFR_SOURCE)
            FfrRegister();
    }

    // The head's switch, as the last Spawns file said: a tick box beside its caption
    // ("the spawn points are on"). Ticked and bright while the service is on, empty and
    // muted while it is off, empty and faint until the file has come.
    protected void SpPaintSwitch()
    {
        bool known = false;
        bool on = false;
        if (m_SpawnsCfg)
        {
            known = true;
            on = m_SpawnsCfg.Enabled;
        }

        int ink = ARGB(255, 92, 90, 88);
        if (known)
            ink = ARGB(255, 160, 159, 153);
        if (on)
            ink = ARGB(255, 224, 223, 218);
        Widget t = M_SUB_WIDGET.FindAnyWidget("BtnSpOnText");
        if (t)
            t.SetColor(ink);
        CorePaintTick("SpOnBox", on);
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
                return slug + " — " + nm;
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

        string low = OZ_VppFold.Lower(typed);
        for (int i = 0; i < m_PlrNames.Count(); i++)
        {
            string n = OZ_VppFold.Lower(m_PlrNames[i]);
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

    // The two kinds of rows of the list, made once: both go into the list's one spacer
    // (SpListRows), so they stand in the order they are added.
    protected bool SpMakeRows()
    {
        if (m_SpRows)
            return true;
        Widget host = M_SUB_WIDGET.FindAnyWidget("SpListRows");
        if (!host)
            return false;
        m_SpHeads = new OZ_VppFlexRows(m_Flex, host, "OpenZone_VPP/gui/layouts/oz_vpp_spawn_head.layout", "OZ_VppSpawnHead", 420, 22);
        m_SpRows = new OZ_VppFlexRows(m_Flex, host, "OpenZone_VPP/gui/layouts/oz_vpp_spawn_row.layout", "OZ_VppSpawnRow", 420, 38);
        return true;
    }

    // The list: a caption row per group, then its points (who, where, radius).
    protected void RebuildSpawnList()
    {
        if (!m_SpawnsCfg || !SpMakeRows())
            return;

        m_SpHeads.Clear();
        m_SpRows.Clear();

        // СТЕЙДЖИНҐ -- ПЕРШИМ, i навіть коли його немає: «не задано» відповідає на
        // питання, яке інакше вимагало б читати файл руками.
        SpHead("#STR_OZ_VPP_SP_GROUP_STAGING");
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
            SpRow("zone:*", stgWho, stgWhere, stgRad);
        }
        else
        {
            SpRow("none", "#STR_OZ_VPP_SP_NOT_SET", "", "");
        }

        SpHead("#STR_OZ_VPP_SP_GROUP_ZONES");
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
                SpRow("zone:" + slug, zWho, zWhere, zRad);
                zones++;
            }
        }
        if (zones == 0)
            SpRow("none", "#STR_OZ_VPP_SP_NONE", "", "");

        // Особисті точки -- у тому ж списку: адмін бачить УСЕ, що впливає на
        // спавни, на одному екрані.
        // Two hundred rows of them at most (a row is seven widgets); the rest are counted
        // in a last row, and the player field still takes any of them by Steam ID.
        SpHead("#STR_OZ_VPP_SP_GROUP_PERSONAL");
        int pers = 0;
        int hidden = 0;
        if (m_SpawnsCfg.Personal)
        {
            for (int k = 0; k < m_SpawnsCfg.Personal.Count(); k++)
            {
                OZ_SpawnPersonal p = m_SpawnsCfg.Personal[k];
                if (!p)
                    continue;
                if (pers >= 200)
                {
                    hidden++;
                    continue;
                }
                string pWho = SpPlayerName(p.Uid);
                string pWhere = SpWhere(p.Center);
                string pRad = "r " + SpMetres(p.Radius);
                SpRow("uid:" + p.Uid, pWho, pWhere, pRad);
                pers++;
            }
        }
        if (pers == 0)
            SpRow("none", "#STR_OZ_VPP_SP_NONE", "", "");
        if (hidden > 0)
            SpRow("none", "… +" + hidden.ToString(), "", "");

        m_SpHeads.Fit();
        m_SpRows.Fit();
        SpMarkRows();
    }

    // A group's caption: a stringtable key.
    protected void SpHead(string caption)
    {
        Widget row = m_SpHeads.Add(caption);
        if (row)
            RowText(row, "SpHeadText", caption, ARGB(255, 160, 159, 153));
    }

    // One point, or the one row of an empty group (key "none": its text is a
    // stringtable key and it names no point).
    protected void SpRow(string key, string who, string where, string radius)
    {
        Widget row = m_SpRows.Add(key);
        if (!row)
            return;
        int muted = ARGB(255, 160, 159, 153);
        RowText(row, "SpRowWho", who, ARGB(255, 224, 223, 218));
        RowText(row, "SpRowWhere", where, muted);
        RowText(row, "SpRowR", radius, muted);
    }

    // The picked point's row is lit (the accent's frame on its soft ground, as the board
    // draws it); the row of an empty group is faint.
    protected void SpMarkRows()
    {
        if (!m_SpRows)
            return;

        int ink = ARGB(255, 224, 223, 218);
        int faint = ARGB(255, 92, 90, 88);
        Widget row;
        Widget who;
        string key;
        for (int r = 0; r < m_SpRows.Count(); r++)
        {
            row = m_SpRows.Row(r);
            key = m_SpRows.Key(r);
            if (!row)
                continue;

            if (key != "none" && key == m_SpPick)
            {
                OZ_VppFlex.Paint(row, "OZ_VppSpawnRowEdge", ARGB(255, 232, 120, 43));
                OZ_VppFlex.Paint(row, "OZ_VppSpawnRowBg", ARGB(255, 47, 29, 17));
            }
            else
            {
                OZ_VppFlex.Paint(row, "OZ_VppSpawnRowEdge", ARGB(255, 19, 19, 18));
                OZ_VppFlex.Paint(row, "OZ_VppSpawnRowBg", ARGB(255, 19, 19, 18));
            }

            who = row.FindAnyWidget("SpRowWho");
            if (!who)
                continue;
            if (key == "none")
                who.SetColor(faint);
            else
                who.SetColor(ink);
        }
    }

    // A click somewhere in the list; false: not this list.
    protected bool SpRowClick(Widget w)
    {
        if (!m_SpRows)
            return false;
        int i = m_SpRows.IndexOf(w);
        if (i == -1)
            return false;
        SpPickKey(m_SpRows.Key(i));
        return true;
    }

    // Рядок списку КЛАДЕ СЕБЕ В ФОРМУ: a zone into the cycler, a personal point into
    // the player field, its radius into the radius field; the scheme marks it.
    protected void SpPickKey(string key)
    {
        bool zone = key.IndexOf("zone:") == 0;
        bool uid = key.IndexOf("uid:") == 0;
        if (!zone && !uid)
            return;

        m_SpPick = key;
        float radius = 0;
        string center = "";
        SpPointOf(m_SpPick, center, radius);
        if (radius > 0)
            SetEdit("SpRadius", SpMetres(radius));

        if (uid)
        {
            SetEdit("SpUid", key.Substring(4, key.Length() - 4));
        }
        else
        {
            m_SpSlugPick = key.Substring(5, key.Length() - 5);
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
    // No map: the admin is the dot in the middle, north is up, every point is a circle
    // of its radius with its name over it. As the board CoreSpawns draws them: faction
    // zones muted, the fallback dashed, the picked point in the accent colour on a soft
    // ground; the staging zone is in the alert colour and personal points are green (the
    // board has neither). The scale fits the farthest edge of any point. Drawn on the
    // page's CanvasWidget (dayz-modding gui-layouts.md §7: Clear, then every line again),
    // on each change and twice a second while the page is shown (Tick); the names are
    // labels placed over the canvas (SpLabel).
    protected void SpDrawScheme()
    {
        m_SpDrawnAt = GetGame().GetTime();
        m_SpLabelsUsed = 0;

        CanvasWidget cv = CanvasWidget.Cast(M_SUB_WIDGET.FindAnyWidget("SpScheme"));
        if (!cv)
            return;
        cv.Clear();

        float w;
        float h;
        cv.GetScreenSize(w, h);
        DayZPlayer me = GetGame().GetPlayer();
        if (w <= 0 || h <= 0 || !me)
        {
            SpHideLabels();
            return;
        }
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

        // The scheme's edge stands a label's height inside the canvas: the name over the
        // farthest circle still has room.
        float s = UnitScale();
        float view = far * 1.1;
        float half = Math.Min(w, h) / 2 - 24 * s;
        if (half < 10)
        {
            SpHideLabels();
            return;
        }
        float scale = half / view;
        float cx = w / 2;
        float cy = h / 2;

        int muted = ARGB(255, 160, 159, 153);
        int alert = ARGB(255, 245, 199, 64);
        int okInk = ARGB(255, 102, 199, 92);
        int accent = ARGB(255, 232, 120, 43);

        // Under every line: the soft ground of the picked point (the accent colour at an
        // eighth over the scheme's ground, written as the colour it comes to).
        string pc;
        float pr;
        bool picked = SpPointOf(m_SpPick, pc, pr);
        if (picked && pc != "")
        {
            vector pv = pc.ToVector();
            if (pv != vector.Zero)
            {
                float fr = pr * scale;
                if (fr < 4)
                    fr = 4;
                float fx = cx + (pv[0] - at[0]) * scale;
                float fy = cy - (pv[2] - at[2]) * scale;
                int soft = ARGB(255, 34, 21, 11);
                SpDisc(cv, fx, fy, fr, soft);
            }
        }

        // The dashed cross through the admin.
        int cross = ARGB(255, 60, 60, 58);
        SpDashes(cv, cx - half, cy, cx + half, cy, cross, s);
        SpDashes(cv, cx, cy - half, cx, cy + half, cross, s);

        if (m_SpawnsCfg)
        {
            if (m_SpawnsCfg.Staging && m_SpPick != "zone:*")
                SpPoint(cv, at, scale, cx, cy, "zone:*", m_SpawnsCfg.Staging.Center, m_SpawnsCfg.Staging.Radius, alert, false, false);
            for (int zi = 0; zi < m_SpawnsCfg.Zones.Count(); zi++)
            {
                OZ_SpawnZone z = m_SpawnsCfg.Zones[zi];
                if (!z)
                    continue;
                string zslug = z.Role;
                if (zslug == "")
                    zslug = "-";
                string zkey = "zone:" + zslug;
                if (zkey == m_SpPick)
                    continue;
                bool fallback = z.Role == "";
                SpPoint(cv, at, scale, cx, cy, zkey, z.Center, z.Radius, muted, fallback, false);
            }
            for (int pk = 0; pk < m_SpawnsCfg.Personal.Count(); pk++)
            {
                OZ_SpawnPersonal p = m_SpawnsCfg.Personal[pk];
                if (!p)
                    continue;
                string pkey = "uid:" + p.Uid;
                if (pkey == m_SpPick)
                    continue;
                SpPoint(cv, at, scale, cx, cy, pkey, p.Center, p.Radius, okInk, false, false);
            }

            // The picked point last, over the rest.
            if (picked)
                SpPoint(cv, at, scale, cx, cy, m_SpPick, pc, pr, accent, false, true);
        }

        // The admin: a dot in the middle and the word beside it.
        SpDisc(cv, cx, cy, 6 * s, alert);
        string you = OvTr("#STR_OZ_VPP_SP_YOU");
        SpLabel(cv, cx + 20 * s, cy + 5 * s, you, alert);

        SpHideLabels();
        SpPaintSchemeText(at, view);
    }

    // What a point is called over its circle: a faction's id, the staging zone and the
    // fallback in the list's words, a player's name (or Steam ID).
    protected string SpLabelName(string key)
    {
        if (key.IndexOf("uid:") == 0)
            return SpPlayerName(key.Substring(4, key.Length() - 4));
        if (key == "zone:*")
            return OvTr("#STR_OZ_VPP_SP_STAGING");
        if (key == "zone:-")
            return OvTr("#STR_OZ_VPP_SP_FALLBACK");
        if (key.IndexOf("zone:") == 0)
            return key.Substring(5, key.Length() - 5);
        return "";
    }

    // A label over the scheme, its middle at x and its top at y (pixels of the canvas):
    // the next one of the kept labels, or a new one (oz_vpp_spawn_label, 200 x 18 units,
    // its text centred). Forty-eight at most: points beyond that go without a name.
    protected void SpLabel(CanvasWidget cv, float x, float y, string text, int color)
    {
        Widget box = M_SUB_WIDGET.FindAnyWidget("SpSchemeBox");
        if (!box || !cv)
            return;

        Widget lab;
        if (m_SpLabelsUsed < m_SpLabels.Count())
        {
            lab = m_SpLabels[m_SpLabelsUsed];
        }
        else
        {
            if (m_SpLabels.Count() >= 48)
                return;
            lab = OZ_VppFlex.Create("OpenZone_VPP/gui/layouts/oz_vpp_spawn_label.layout", box, "OZ_VppSpawnLabel");
            if (!lab)
                return;
            m_SpLabels.Insert(lab);
        }
        m_SpLabelsUsed++;
        if (!lab)
            return;

        // The canvas stands inside the box's frame: its own place is added.
        float ox;
        float oy;
        cv.GetPos(ox, oy);
        float s = UnitScale();
        lab.SetPos(ox + x - 100 * s, oy + y);
        lab.Show(true);
        RowText(lab, "SpLabelText", text, color);
    }

    // The labels the last drawing did not use are hidden.
    protected void SpHideLabels()
    {
        for (int i = m_SpLabelsUsed; i < m_SpLabels.Count(); i++)
        {
            if (m_SpLabels[i])
                m_SpLabels[i].Show(false);
        }
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

    // One point: its circle (dashed for the fallback zone, heavier for the picked point),
    // a mark in its middle and its name with its radius over it -- under it when the
    // circle reaches the canvas's top.
    protected void SpPoint(CanvasWidget cv, vector at, float scale, float cx, float cy, string key, string center, float radius, int color, bool dashed, bool lit)
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
        float width = 1;
        if (lit)
            width = 2;
        SpCircle(cv, px, py, pr, color, width, dashed);
        cv.DrawLine(px - 2, py, px + 2, py, 4, color);

        int ink = ARGB(255, 160, 159, 153);
        if (lit)
            ink = ARGB(255, 224, 223, 218);
        float s = UnitScale();
        float ly = py - pr - 22 * s;
        if (ly < 2 * s)
            ly = py + pr + 4 * s;
        string name = SpLabelName(key);
        name = name + " · r " + SpMetres(radius);
        SpLabel(cv, px, ly, name, ink);
    }

    // A circle as a ring of short lines (CanvasWidget draws lines only): a line for
    // about every six pixels of the ring, 16 at least and 96 at most; a dashed ring
    // draws every other one.
    protected void SpCircle(CanvasWidget cv, float x, float y, float r, int color, float width, bool dashed)
    {
        int segs = Math.Round(r * Math.PI2 / 6.0);
        if (segs < 16)
            segs = 16;
        if (segs > 96)
            segs = 96;
        float step = Math.PI2 / segs;
        float px = x + r;
        float py = y;
        int odd;
        for (int i = 1; i <= segs; i++)
        {
            float a = step * i;
            float nx = x + r * Math.Cos(a);
            float ny = y + r * Math.Sin(a);
            odd = i % 2;
            if (!dashed || odd == 1)
                cv.DrawLine(px, py, nx, ny, width, color);
            px = nx;
            py = ny;
        }
    }

    // A filled circle as horizontal bands, forty or so (a band is a pixel thick at
    // least); each band is as wide as the circle is at its far edge, so none reaches
    // outside the ring drawn over it.
    protected void SpDisc(CanvasWidget cv, float x, float y, float r, int color)
    {
        if (r < 1)
            return;
        float band = r / 20.0;
        if (band < 1)
            band = 1;
        float dy = -r;
        float edge;
        float hw;
        float mid;
        while (dy < r)
        {
            edge = dy;
            if (dy + band > 0 && dy < 0)
                edge = 0;
            else if (dy >= 0)
                edge = dy + band;
            if (edge > r)
                edge = r;
            if (edge < -r)
                edge = -r;
            hw = Math.Sqrt(r * r - edge * edge);
            mid = y + dy + band / 2;
            if (hw > 0.5)
                cv.DrawLine(x - hw, mid, x + hw, mid, band + 1, color);
            dy = dy + band;
        }
    }

    // A dashed line: three units drawn, six left out (the board's cross).
    protected void SpDashes(CanvasWidget cv, float x1, float y1, float x2, float y2, int color, float s)
    {
        float dx = x2 - x1;
        float dy = y2 - y1;
        float len = Math.Sqrt(dx * dx + dy * dy);
        if (len < 1 || s <= 0)
            return;
        float ux = dx / len;
        float uy = dy / len;
        float dash = 3 * s;
        float pitch = 9 * s;
        float a = 0;
        float b;
        float ax;
        float ay;
        while (a < len)
        {
            b = a + dash;
            if (b > len)
                b = len;
            ax = x1 + ux * a;
            ay = y1 + uy * a;
            cv.DrawLine(ax, ay, x1 + ux * b, y1 + uy * b, 1, color);
            a = a + pitch;
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
        // The switch is the whole label; its tick box is a button of its own inside it.
        if (nm == "BtnSpOn" || nm == "SpOnBox")
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

    // ---------------------------------------------------------- Ядро › Файли конфігів

    // The module a file's owner names, as the rail calls it; an owner with no page
    // in this window, by its own id.
    // "Файли конфігів" under every module that owns a file here (design 2.1): the one
    // page, registered once more per module; the core's own is "raw".
    protected void RegisterFilesPages()
    {
        Widget pane = m_Panes.Get("raw");
        if (!pane)
            return;
        string mod;
        string id;
        int n = m_ModIds.Count();
        for (int m = 0; m < n; m++)
        {
            mod = m_ModIds[m];
            id = mod + ".files";
            if (mod == "core" || m_TabIds.Find(id) != -1 || m_CfgOwners.Find(mod) == -1)
                continue;
            RegisterPage(mod, m_ModLabels[m], id, "#STR_OZ_VPP_PAGE_FILES", pane, "");
        }
    }

    protected bool IsFilesPage(string id)
    {
        if (id == "raw")
            return true;
        int n = id.Length();
        if (n < 7)
            return false;
        return id.Substring(n - 6, 6) == ".files";
    }

    protected string RawOwnerLabel(string owner)
    {
        int m = m_ModIds.Find(owner);
        if (m != -1)
            return OvTr(m_ModLabels[m]);
        string up = owner;
        up.ToUpper();
        return up;
    }

    // A name in capitals, for a group's caption (the board writes them so). The engine's
    // ToUpper() is trusted with Latin letters only: the Ukrainian ones go by a table, a
    // letter a place in both rows (the capital I is the Latin one, as the series' strings
    // write it).
    protected string RawUpper(string s)
    {
        string low = "абвгґдеєжзиіїйклмнопрстуфхцчшщьюя";
        string big = "АБВГҐДЕЄЖЗИIЇЙКЛМНОПРСТУФХЦЧШЩЬЮЯ";
        string res = "";
        string ch;
        int at;
        int n = s.LengthUtf8();
        for (int i = 0; i < n; i++)
        {
            ch = s.SubstringUtf8(i, 1);
            at = -1;
            if (ch.Length() == 2)
                at = low.IndexOf(ch);
            if (at != -1)
            {
                // Every letter of the lower row is two bytes: the byte index halves
                // into the letter's place.
                res = res + big.SubstringUtf8(at / 2, 1);
                continue;
            }
            // One byte is a Latin letter, a digit or a sign: the engine's own. Any
            // other letter stays as it is.
            if (ch.Length() == 1)
                ch.ToUpper();
            res = res + ch;
        }
        return res;
    }

    // The module whose files page is open: "core" for the core's own, the module of
    // "<module>.files" for the others; empty when the open page is not a files page.
    protected string RawPageModule()
    {
        if (m_Shown == "raw")
            return "core";
        if (!IsFilesPage(m_Shown))
            return "";
        return m_Shown.Substring(0, m_Shown.Length() - 6);
    }

    // The three kinds of rows of the list, made once: all go into the list's one spacer
    // (RawListRows), so they stand in the order they are added.
    protected bool RawMakeRows()
    {
        if (m_RawFiles)
            return true;
        Widget host = M_SUB_WIDGET.FindAnyWidget("RawListRows");
        if (!host)
            return false;
        m_RawHeads = new OZ_VppFlexRows(m_Flex, host, "OpenZone_VPP/gui/layouts/oz_vpp_file_head.layout", "OZ_VppFileHead", 275, 28);
        m_RawFiles = new OZ_VppFlexRows(m_Flex, host, "OpenZone_VPP/gui/layouts/oz_vpp_file_row.layout", "OZ_VppFileRow", 275, 28);
        m_RawNones = new OZ_VppFlexRows(m_Flex, host, "OpenZone_VPP/gui/layouts/oz_vpp_file_none.layout", "OZ_VppFileNone", 275, 22);
        return true;
    }

    // The files grouped by the module that owns them: the module whose files page is
    // open first, then the rest of the rail in its order (a module with no file says
    // so), then the owners with no page here. A caption row per group.
    protected void RebuildRawList()
    {
        if (!RawMakeRows())
            return;

        array<string> groups = new array<string>();
        string first = RawPageModule();
        if (first != "" && m_ModIds.Find(first) != -1)
            groups.Insert(first);
        for (int m = 0; m < m_ModIds.Count(); m++)
        {
            if (groups.Find(m_ModIds[m]) == -1)
                groups.Insert(m_ModIds[m]);
        }
        for (int o = 0; o < m_CfgOwners.Count(); o++)
        {
            if (groups.Find(m_CfgOwners[o]) == -1)
                groups.Insert(m_CfgOwners[o]);
        }

        int muted = ARGB(255, 160, 159, 153);
        m_RawHeads.Clear();
        m_RawFiles.Clear();
        m_RawNones.Clear();
        int owners = 0;
        Widget row;
        for (int g = 0; g < groups.Count(); g++)
        {
            string caption = RawUpper(RawOwnerLabel(groups[g]));
            row = m_RawHeads.Add(groups[g]);
            if (row)
                RowText(row, "RawHeadText", caption, muted);

            int files = 0;
            for (int i = 0; i < m_CfgNames.Count(); i++)
            {
                if (m_CfgOwners[i] != groups[g])
                    continue;
                row = m_RawFiles.Add(m_CfgNames[i]);
                if (row)
                    RowText(row, "RawRowName", m_CfgNames[i], muted);
                files++;
            }
            if (files > 0)
            {
                owners++;
                continue;
            }
            row = m_RawNones.Add(groups[g]);
            if (row)
                RowText(row, "RawNoneText", "#STR_OZ_VPP_FL_NO_FILES", muted);
        }
        m_RawHeads.Fit();
        m_RawFiles.Fit();
        m_RawNones.Fit();

        RawMarkRows();

        string filesWord = OvTr("#STR_OZ_VPP_FL_FILES");
        string modsWord = OvTr("#STR_OZ_VPP_FL_MODULES");
        string count = filesWord + " " + m_CfgNames.Count().ToString();
        count = count + ", " + modsWord;
        count = count + " " + owners.ToString();
        OvText("RawCount", count, muted);
    }

    // The picked file's row is lit: the accent's soft ground and its bar on the left,
    // bright words; the rest lie on the list's own ground in muted words.
    protected void RawMarkRows()
    {
        if (!m_RawFiles)
            return;
        Widget row;
        Widget bar;
        Widget name;
        bool lit;
        for (int r = 0; r < m_RawFiles.Count(); r++)
        {
            row = m_RawFiles.Row(r);
            if (!row)
                continue;
            lit = m_RawFiles.Key(r) == m_RawPicked;
            bar = row.FindAnyWidget("OZ_VppFileRowBar");
            if (bar)
                bar.Show(lit);
            name = row.FindAnyWidget("RawRowName");
            if (lit)
            {
                OZ_VppFlex.Paint(row, "OZ_VppFileRowBg", ARGB(255, 53, 35, 22));
                if (name)
                    name.SetColor(ARGB(255, 224, 223, 218));
            }
            else
            {
                OZ_VppFlex.Paint(row, "OZ_VppFileRowBg", ARGB(255, 19, 19, 18));
                if (name)
                    name.SetColor(ARGB(255, 160, 159, 153));
            }
        }
    }

    // A click somewhere in the list; false: not a file's row.
    protected bool RawRowClick(Widget w)
    {
        if (!m_RawFiles)
            return false;
        int i = m_RawFiles.IndexOf(w);
        if (i == -1)
            return false;
        RawPick(m_RawFiles.Key(i));
        return true;
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
        return amount + " — " + note;
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

    // The status line under the editor: whether the text reads as JSON (a sign and the
    // words), which module reads the file again once it is applied (shown while the text
    // reads), and where the caret is.
    protected void RawPaintStatus(bool pending)
    {
        int ink = ARGB(255, 224, 223, 218);
        int okInk = ARGB(255, 102, 199, 92);
        int badInk = ARGB(255, 224, 82, 74);
        int muted = ARGB(255, 160, 159, 153);

        ImageWidget sign = ImageWidget.Cast(M_SUB_WIDGET.FindAnyWidget("RawJsonIcon"));
        Widget sep = M_SUB_WIDGET.FindAnyWidget("RawSep");
        Widget after = M_SUB_WIDGET.FindAnyWidget("RawAfter");
        bool reads = false;

        if (m_RawPicked == "")
        {
            OvText("RawJson", "", muted);
            OvText("RawWhere", "", muted);
            OvText("RawAfter", "", muted);
            if (sign)
                sign.Show(false);
            if (sep)
                sep.Show(false);
            return;
        }

        if (pending)
        {
            OvText("RawJson", "#STR_OZ_VPP_FL_JSON_WAIT", muted);
            if (sign)
                sign.Show(false);
        }
        else if (m_RawCheckedOk)
        {
            reads = true;
            OvText("RawJson", "#STR_OZ_VPP_FL_JSON_OK", ink);
            RawSign(sign, "check", okInk);
        }
        else
        {
            string bad = OvTr("#STR_OZ_VPP_FL_JSON_BAD");
            if (m_RawCheckedErr != "")
                bad = bad + ": " + m_RawCheckedErr;
            OvText("RawJson", bad, badInk);
            RawSign(sign, "cross", badInk);
        }

        // "after applying, the file is read again by the module «Trader»".
        if (reads)
        {
            string afterWord = OvTr("#STR_OZ_VPP_FL_AFTER");
            string whose = RawOwnerLabel(RawOwnerOf(m_RawPicked));
            string said = afterWord + " «" + whose;
            said = said + "»";
            OvText("RawAfter", said, muted);
        }
        else
        {
            OvText("RawAfter", "", muted);
        }
        if (sep)
            sep.Show(reads);
        if (after)
            after.Show(reads);
        RawFlowStatus();

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

    // The sign before the status words: shown, of that picture (an icon of the window's
    // set) and that colour.
    protected void RawSign(ImageWidget sign, string image, int color)
    {
        if (!sign)
            return;
        sign.Show(true);
        sign.LoadImageFile(0, "set:ozv_ui image:" + image);
        sign.SetImage(0);
        sign.SetColor(color);
    }

    // The status words stand by their length: the JSON words get the width they need (a
    // text is cut at its widget's width, and the layout sized it for the board's two
    // words) up to the paste button, and the dot and the line about the module come 14
    // units after them, whatever language they are in, and end before that button.
    protected void RawFlowStatus()
    {
        TextWidget json = TextWidget.Cast(M_SUB_WIDGET.FindAnyWidget("RawJson"));
        Widget sep = M_SUB_WIDGET.FindAnyWidget("RawSep");
        Widget after = M_SUB_WIDGET.FindAnyWidget("RawAfter");
        Widget paste = M_SUB_WIDGET.FindAnyWidget("BtnRawPaste");
        if (!json || !sep || !after)
            return;
        int tw = OZ_VppFlex.TextWidth(json);
        if (tw <= 0)
            return;

        float s = UnitScale();
        float x;
        float y;
        float w;
        float h;
        float px = 0;
        float py = 0;
        json.GetPos(x, y);
        json.GetSize(w, h);
        if (paste)
            paste.GetPos(px, py);

        float wide = tw + 2;
        float most = px - 14 * s - x;
        if (paste && most > 40 * s && wide > most)
            wide = most;
        json.SetSize(wide, h);

        float at = x + wide + 14 * s;
        sep.GetPos(x, y);
        sep.SetPos(at, y);
        at = at + 17 * s;
        after.GetPos(x, y);
        after.GetSize(w, h);
        after.SetPos(at, y);
        if (paste)
        {
            // The line about the module ends before the paste button too.
            float room = px - 14 * s - at;
            if (room < 1)
                room = 1;
            after.SetSize(room, h);
        }
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

        // The verdict on the text as it stands now, without waiting for a pause in the
        // typing: the status line shows it and the journal keeps it.
        if (nm == "BtnRawCheck")
        {
            if (m_RawPicked == "")
            {
                Hint("#STR_OZ_VPP_FL_PICK_FIRST");
                return true;
            }
            string probeText = RawText();
            RawCheck(probeText);
            RawPaintStatus(false);
            if (m_RawCheckedOk)
            {
                Hint("#STR_OZ_VPP_FL_JSON_OK");
                return true;
            }
            string verdict = OvTr("#STR_OZ_VPP_FL_JSON_BAD");
            if (m_RawCheckedErr != "")
                verdict = verdict + ": " + m_RawCheckedErr;
            Hint(verdict);
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
            // Recoded from the system's ANSI text (OZ_VppClip).
            string clip = OZ_VppClip.Read();
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
            OZ_VppClip.Write(whole);
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

        // The core's own pages have no listbox any more: their lists are rows of row
        // templates and answer in OnClick. A glue's layer may still have one.
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

        // The status bar's count of config problems leads to the page that lists them.
        if (nm == "BtnVppProblems")
        {
            ShowPane("overview");
            return true;
        }

        if (nm == "BtnJrAll" || nm == "BtnJrErr" || nm == "BtnJrMine")
        {
            bool errs = nm == "BtnJrErr";
            bool mine = nm == "BtnJrMine";
            m_JournalErrors = errs;
            m_JournalMine = mine;
            RebuildJournal();
            return true;
        }

        if (nm == "BtnOvRefresh")
        {
            AskOverview();
            return true;
        }

        // A module's card opens the module's first page.
        if (nm.IndexOf("OvCard") == 0 && nm.Length() == 7 && m_Ov)
        {
            int cardN = nm.Substring(6, 1).ToInt();
            if (cardN >= 0 && cardN < m_OvOrder.Count())
            {
                string cardPage = FirstPageOf(m_Ov.Modules[m_OvOrder[cardN]].Id);
                if (cardPage != "" && cardPage != "overview")
                    ShowPane(cardPage);
            }
            return true;
        }

        if (nm == "BtnOvReload")
        {
            Confirm("#STR_OZ_VPP_OV_ASK", "#STR_OZ_VPP_OV_YES", "reload_all");
            return true;
        }

        // Ядро › Точки появи: its buttons, then the rows of its list.
        if (SpClick(nm))
            return true;
        if (SpRowClick(w))
            return true;

        if (nm == "BtnPlrRefresh")
        {
            PlrAskList();
            if (m_PlrPick != "")
                PlrPick(m_PlrPick);
            return true;
        }

        // Ядро › Гравці: a row of the list, a link out of the card.
        if (PlrRowClick(w))
            return true;
        if (nm == "PlrLink0" || nm == "PlrLink1" || nm == "PlrLink2")
        {
            PlrOpenLink(nm.Substring(7, 1).ToInt());
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

        if (nm == "BtnMrRefresh")
        {
            Ask(OZ_AdminSect.CONFIG, "mirror_list", "{}");
            return true;
        }

        // ДЗЕРКАЛА -- ПО РЯДКУ, А НЕ ПО ІМЕНІ. Рядки згенеровані, і всі їхні
        // кнопки звуться однаково (MrRowBtn): рід везе сам рядок.
        if (MirrorRowClick(w, nm))
            return true;

        // Ядро › Файли конфігів: its buttons, then the rows of its list.
        if (RawClick(nm))
            return true;
        if (RawRowClick(w))
            return true;

        // Фракції › Групи FFR: its buttons, then the rows of its list.
        if (FfrClick(nm))
            return true;
        if (FfrRowClick(w))
            return true;

        return super.OnClick(w, x, y, button);
    }

    // ---------------------------------------------------------- Фракції › Групи FFR
    //
    // Design §4.3, §5.2. The page exists only when the server's source of factions is the
    // FFR glue (identity says so): that glue is a server mod (-serverMod), so the client
    // never loads it and cannot know of it at compile time. It shows FFR's groups with
    // the faction each maps to (ffr_groups of the glue's section "ffr") and edits the
    // glue's own OZ_Identity_FFR.json through cfg_get/cfg_set; FFR's file is only read.

    protected void FfrRegister()
    {
        if (m_FfrOn || !M_SUB_WIDGET)
            return;

        // The page as the board FactionsFfr draws it (css2layout); the root's name is
        // checked, as the engine has handed one path another file's widgets.
        Widget pane = OZ_VppFlex.Create("OpenZone_VPP/gui/layouts/oz_vpp_pane_ffr.layout", M_SUB_WIDGET, "PaneFfr");
        if (!pane)
        {
            OZ_Log.Error("vpp ffr: layout failed to load");
            return;
        }

        m_FfrOn = true;
        m_FfrFacBox = EditBoxWidget.Cast(pane.FindAnyWidget("FfrFacId"));
        m_FfrBaseBox = EditBoxWidget.Cast(pane.FindAnyWidget("FfrBase"));
        m_FfrReloadBox = EditBoxWidget.Cast(pane.FindAnyWidget("FfrReload"));
        RegisterPage("factions", "#STR_OZ_VPP_MOD_FACTIONS", "ffr", "#STR_OZ_VPP_PAGE_FFR", pane, "");
    }

    protected void FfrAsk()
    {
        Ask(FFR_SECTION, FFR_OP_GROUPS, "{}");
        AskCfg(FFR_CFG);
    }

    // The groups of FFR's file as the glue read them. A refusal is in the journal.
    protected void OnFfrAnswer(string op, bool ok, string json, string error)
    {
        if (!ok)
            return;
        if (op != FFR_OP_GROUPS && op != FFR_OP_REREAD)
            return;

        // Root made by the script, read through a copy made by the script.
        OZ_VppFfrGroups parsed = new OZ_VppFfrGroups();
        string err;
        if (!JsonFileLoader<OZ_VppFfrGroups>.LoadData(json, parsed, err) || !parsed)
            return;
        m_FfrGroups = parsed.Copy();

        FfrRebuild();
        FfrPaintAll();
    }

    // The settings file came (cfg_get). The draft follows it unless it holds edits not
    // applied yet; after this page's own apply it follows anyway, as the server put the
    // text in order (empty rows dropped, the period at 5 s or more).
    protected void FfrTakeSettings(string body)
    {
        OZ_VppFfrSettings parsed = new OZ_VppFfrSettings();
        string err;
        if (!JsonFileLoader<OZ_VppFfrSettings>.LoadData(body, parsed, err) || !parsed)
            return;
        OZ_VppFfrSettings saved = parsed.Copy();

        bool keep = false;
        if (m_FfrDraft && m_FfrSaved && !m_FfrApplied && FfrChanges() != "")
            keep = true;
        m_FfrApplied = false;
        m_FfrSaved = saved;
        if (!keep)
        {
            m_FfrDraft = saved.Copy();
            FfrFillFields();
        }

        FfrRebuild();
        FfrPaintAll();
    }

    // The draft's base faction and period into the fields, its spender into the switch;
    // the picked group's list back on the faction the draft maps it to, the typed id gone.
    protected void FfrFillFields()
    {
        if (!m_FfrDraft)
            return;
        if (m_FfrBaseBox)
            m_FfrBaseBox.SetText(m_FfrDraft.BaseFaction);
        if (m_FfrReloadBox)
            m_FfrReloadBox.SetText(m_FfrDraft.ReloadSeconds.ToString());
        m_FfrFacPick = FfrDraftFaction(m_FfrPick);
        if (m_FfrFacBox)
            m_FfrFacBox.SetText("");
    }

    // The fields back into the draft, before it is compared or sent. A period that is
    // not a whole number keeps the draft's.
    protected void FfrReadFields()
    {
        if (!m_FfrDraft)
            return;
        if (m_FfrBaseBox)
        {
            string baseId = m_FfrBaseBox.GetText().Trim();
            m_FfrDraft.BaseFaction = baseId;
        }
        if (m_FfrReloadBox)
        {
            string per = m_FfrReloadBox.GetText().Trim();
            if (FfrIsNumber(per))
                m_FfrDraft.ReloadSeconds = per.ToInt();
        }
    }

    protected bool FfrIsNumber(string s)
    {
        if (s == "" || s.Length() > 6)
            return false;
        for (int i = 0; i < s.Length(); i++)
        {
            if ("0123456789".IndexOf(s.Substring(i, 1)) == -1)
                return false;
        }
        return true;
    }

    protected string FfrDraftFaction(string gname)
    {
        if (!m_FfrDraft || gname == "")
            return "";
        return m_FfrDraft.FactionOf(gname);
    }

    protected string FfrSavedFaction(string gname)
    {
        if (!m_FfrSaved || gname == "")
            return "";
        return m_FfrSaved.FactionOf(gname);
    }

    protected int FfrMembersOf(string gname)
    {
        if (!m_FfrGroups)
            return 0;
        for (int i = 0; i < m_FfrGroups.Groups.Count(); i++)
        {
            if (m_FfrGroups.Groups[i].Name == gname)
                return m_FfrGroups.Groups[i].Members;
        }
        return 0;
    }

    // A group's state: mapped, changed and not applied yet, or on the base axis only.
    protected string FfrStateKey(string gname)
    {
        string draft = FfrDraftFaction(gname);
        if (draft != FfrSavedFaction(gname))
            return "#STR_OZ_VPP_FFR_STATE_CHANGED";
        if (draft == "")
            return "#STR_OZ_VPP_FFR_STATE_NONE";
        return "#STR_OZ_VPP_FFR_STATE_OK";
    }

    // A faction as the page writes it: its id, with its name after a dash when the
    // server's source of factions names it; a dash for none.
    protected string FfrFacWords(string id)
    {
        if (id == "")
            return "—";
        int at = m_IdFactions.Find(id);
        if (at != -1 && at < m_IdNames.Count())
        {
            string nm = m_IdNames[at];
            if (nm != "" && nm != id)
                return id + " — " + nm;
        }
        return id;
    }

    // The groups, a row each: name, players, the faction the draft maps it to, the state.
    protected void FfrRebuild()
    {
        if (!m_FfrGroups)
            return;
        if (!m_FfrRows)
        {
            Widget host = M_SUB_WIDGET.FindAnyWidget("FfrListRows");
            if (!host)
                return;
            m_FfrRows = new OZ_VppFlexRows(m_Flex, host, "OpenZone_VPP/gui/layouts/oz_vpp_ffr_row.layout", "OZ_VppFfrRow", 852, 38);
        }

        int ink = ARGB(255, 224, 223, 218);
        int muted = ARGB(255, 160, 159, 153);
        m_FfrRows.Clear();
        Widget row;
        for (int i = 0; i < m_FfrGroups.Groups.Count(); i++)
        {
            OZ_VppFfrGroupRow g = m_FfrGroups.Groups[i];
            row = m_FfrRows.Add(g.Name);
            if (!row)
                continue;
            string fac = FfrFacWords(FfrDraftFaction(g.Name));
            string members = g.Members.ToString();
            RowText(row, "FfrRowName", g.Name, ink);
            RowText(row, "FfrRowCount", members, muted);
            RowText(row, "FfrRowFaction", fac, ink);
        }
        m_FfrRows.Fit();
        FfrMarkRows();
    }

    // The picked group's row is lit (the accent's frame on its soft ground, as the board
    // draws it); each row's state is written in its own colour: mapped green, changed and
    // not applied yet in the accent colour, base axis only in the alert colour.
    protected void FfrMarkRows()
    {
        if (!m_FfrRows)
            return;

        int accent = ARGB(255, 232, 120, 43);
        int okInk = ARGB(255, 102, 199, 92);
        int alert = ARGB(255, 245, 199, 64);
        Widget row;
        for (int r = 0; r < m_FfrRows.Count(); r++)
        {
            row = m_FfrRows.Row(r);
            if (!row)
                continue;
            string gname = m_FfrRows.Key(r);
            if (gname == m_FfrPick)
            {
                OZ_VppFlex.Paint(row, "OZ_VppFfrRowEdge", accent);
                OZ_VppFlex.Paint(row, "OZ_VppFfrRowBg", ARGB(255, 47, 29, 17));
            }
            else
            {
                OZ_VppFlex.Paint(row, "OZ_VppFfrRowEdge", ARGB(255, 19, 19, 18));
                OZ_VppFlex.Paint(row, "OZ_VppFfrRowBg", ARGB(255, 19, 19, 18));
            }

            string key = FfrStateKey(gname);
            int stateInk = okInk;
            if (key == "#STR_OZ_VPP_FFR_STATE_CHANGED")
                stateInk = accent;
            else if (key == "#STR_OZ_VPP_FFR_STATE_NONE")
                stateInk = alert;
            RowText(row, "FfrRowState", key, stateInk);
        }
    }

    // A click somewhere in the list; false: not this list. The group is picked and its
    // faction list stands on what the draft maps it to.
    protected bool FfrRowClick(Widget w)
    {
        if (!m_FfrRows)
            return false;
        int i = m_FfrRows.IndexOf(w);
        if (i == -1)
            return false;
        m_FfrPick = m_FfrRows.Key(i);
        m_FfrFacPick = FfrDraftFaction(m_FfrPick);
        if (m_FfrFacBox)
            m_FfrFacBox.SetText("");
        FfrMarkRows();
        FfrPaintPick();
        return true;
    }

    protected void FfrPaintAll()
    {
        FfrPaintCount();
        FfrPaintMissing();
        FfrPaintSpend();
        FfrPaintTrade();
        FfrPaintPick();
        FfrPaintBar();
    }

    // "players: 32 · groups: 7 · read, s ago: 12", beside the list's heading whatever
    // the heading's length; an unread file in the bad colour.
    protected void FfrPaintCount()
    {
        int muted = ARGB(255, 160, 159, 153);
        int badInk = ARGB(255, 224, 82, 74);
        CoreFlowAfter("FfrGroupsHead", "FfrGroupsCount", 10);
        if (!m_FfrGroups)
        {
            OvText("FfrGroupsCount", "", muted);
            return;
        }
        if (!m_FfrGroups.FileRead)
        {
            OvText("FfrGroupsCount", "#STR_OZ_VPP_FFR_UNREAD", badInk);
            return;
        }

        string playersWord = OvTr("#STR_OZ_VPP_FFR_N_PLAYERS");
        string groupsWord = OvTr("#STR_OZ_VPP_FFR_N_GROUPS");
        string ago = string.Format(OvTr("#STR_OZ_VPP_FFR_READ_AGO"), m_FfrGroups.ReadAgo);
        string line = playersWord + " " + m_FfrGroups.Players.ToString();
        line = line + " · " + groupsWord;
        line = line + " " + m_FfrGroups.Groups.Count().ToString();
        line = line + " · " + ago;
        OvText("FfrGroupsCount", line, muted);
    }

    // The names the draft maps that FFR has no group for now, under the rule of exact
    // names (which the page always shows); nothing when every name has its group.
    protected void FfrPaintMissing()
    {
        int muted = ARGB(255, 160, 159, 153);
        if (!m_FfrDraft || !m_FfrGroups)
        {
            OvText("FfrMissing", "", muted);
            return;
        }

        // The paragraph has two lines: six names fit them with the words around, the rest
        // are counted.
        string names = "";
        int shown = 0;
        int more = 0;
        for (int i = 0; i < m_FfrDraft.GroupFactions.Count(); i++)
        {
            string gname = m_FfrDraft.GroupFactions[i].FfrGroup;
            bool found = false;
            for (int k = 0; k < m_FfrGroups.Groups.Count(); k++)
            {
                if (m_FfrGroups.Groups[k].Name == gname)
                    found = true;
            }
            if (found)
                continue;
            if (shown >= 6)
            {
                more++;
                continue;
            }
            if (names != "")
                names = names + ", ";
            names = names + gname;
            shown++;
        }

        if (names == "")
        {
            OvText("FfrMissing", "", muted);
            return;
        }
        if (more > 0)
        {
            names = names + ", … (+";
            names = names + more.ToString() + ")";
        }
        string head = OvTr("#STR_OZ_VPP_FFR_MISSING");
        string tail = OvTr("#STR_OZ_VPP_FFR_MISSING_TAIL");
        string text = head + " " + names;
        text = text + ". " + tail;
        OvText("FfrMissing", text, muted);
    }

    // The spender switch: two choices, the chosen one ticked.
    protected void FfrPaintSpend()
    {
        bool any = true;
        if (m_FfrDraft)
            any = m_FfrDraft.AnyMemberSpends;
        CorePaintTick("FfrSpendAnyBox", any);
        CorePaintTick("FfrSpendNoneBox", !any);
    }

    // What the trader's conditions mean here: a faction works, a post never holds (FFR
    // has none) -- both written in the layout --, and a leader is any member while "any
    // member" spends, nobody otherwise.
    protected void FfrPaintTrade()
    {
        bool any = true;
        if (m_FfrDraft)
            any = m_FfrDraft.AnyMemberSpends;
        int ink = ARGB(255, 224, 223, 218);
        if (any)
            OvText("FfrTradeLeader", "#STR_OZ_VPP_FFR_T_LEADER_ANY", ink);
        else
            OvText("FfrTradeLeader", "#STR_OZ_VPP_FFR_T_LEADER_NONE", ink);
    }

    // The picked group's card: its name, how many players it has (right after the name,
    // whatever its length), and the faction its list stands on.
    protected void FfrPaintPick()
    {
        int ink = ARGB(255, 224, 223, 218);
        int muted = ARGB(255, 160, 159, 153);
        if (m_FfrPick == "")
        {
            OvText("FfrPickName", "#STR_OZ_VPP_FFR_PICK", muted);
            OvText("FfrPickInfo", "", muted);
            OvText("BtnFfrFacText", "", muted);
            CoreFlowAfter("FfrPickName", "FfrPickInfo", 8);
            return;
        }
        OvText("FfrPickName", m_FfrPick, ink);
        string word = OvTr("#STR_OZ_VPP_FFR_PICK_INFO");
        string info = word + " " + FfrMembersOf(m_FfrPick).ToString();
        OvText("FfrPickInfo", info, muted);
        CoreFlowAfter("FfrPickName", "FfrPickInfo", 8);
        FfrPaintFac();
    }

    // The list of factions shows the one it stands on: "id — name", or that the group
    // gets no faction.
    protected void FfrPaintFac()
    {
        int ink = ARGB(255, 224, 223, 218);
        if (m_FfrFacPick == "")
        {
            OvText("BtnFfrFacText", "#STR_OZ_VPP_FFR_FAC_NONE", ink);
            return;
        }
        OvText("BtnFfrFacText", FfrFacWords(m_FfrFacPick), ink);
    }

    // What the draft changes against the file, in short; empty: nothing.
    protected string FfrChanges()
    {
        if (!m_FfrDraft || !m_FfrSaved)
            return "";
        FfrReadFields();

        array<string> names = new array<string>();
        for (int i = 0; i < m_FfrDraft.GroupFactions.Count(); i++)
        {
            if (names.Find(m_FfrDraft.GroupFactions[i].FfrGroup) == -1)
                names.Insert(m_FfrDraft.GroupFactions[i].FfrGroup);
        }
        for (int k = 0; k < m_FfrSaved.GroupFactions.Count(); k++)
        {
            if (names.Find(m_FfrSaved.GroupFactions[k].FfrGroup) == -1)
                names.Insert(m_FfrSaved.GroupFactions[k].FfrGroup);
        }

        string summary = "";
        for (int n = 0; n < names.Count(); n++)
        {
            string now = m_FfrDraft.FactionOf(names[n]);
            if (now == m_FfrSaved.FactionOf(names[n]))
                continue;
            if (now == "")
                now = "-";
            if (summary != "")
                summary = summary + "; ";
            summary = summary + names[n];
            summary = summary + " -> " + now;
        }

        if (m_FfrDraft.BaseFaction != m_FfrSaved.BaseFaction)
        {
            string baseWord = OvTr("#STR_OZ_VPP_FFR_CH_BASE");
            if (summary != "")
                summary = summary + "; ";
            summary = summary + baseWord;
            summary = summary + " " + m_FfrDraft.BaseFaction;
        }
        if (m_FfrDraft.AnyMemberSpends != m_FfrSaved.AnyMemberSpends)
        {
            string spendWord = OvTr("#STR_OZ_VPP_FFR_CH_SPEND_NONE");
            if (m_FfrDraft.AnyMemberSpends)
                spendWord = OvTr("#STR_OZ_VPP_FFR_CH_SPEND_ANY");
            if (summary != "")
                summary = summary + "; ";
            summary = summary + spendWord;
        }
        if (m_FfrDraft.ReloadSeconds != m_FfrSaved.ReloadSeconds)
        {
            string reloadWord = OvTr("#STR_OZ_VPP_FFR_CH_RELOAD");
            if (summary != "")
                summary = summary + "; ";
            summary = summary + reloadWord;
            summary = summary + " " + m_FfrDraft.ReloadSeconds.ToString();
        }
        return summary;
    }

    // The draft's bar: what changed (a sign and the words, in the alert colour) or that
    // nothing did; the note on where it is written stands after the words while there is
    // room for it before the buttons.
    protected void FfrPaintBar()
    {
        int muted = ARGB(255, 160, 159, 153);
        int alert = ARGB(255, 245, 199, 64);
        string changes = FfrChanges();
        Widget dot = M_SUB_WIDGET.FindAnyWidget("FfrBarDot");
        if (dot)
            dot.Show(changes != "");
        if (changes == "")
        {
            OvText("FfrBarText", "#STR_OZ_VPP_FFR_NO_CHANGES", muted);
            FfrFlowBar("");
            return;
        }
        string head = OvTr("#STR_OZ_VPP_FFR_CHANGED");
        string said = head + " " + changes;
        OvText("FfrBarText", said, alert);
        FfrFlowBar(said);
    }

    // The bar's words by their length: the changes take what they need up to the
    // buttons (cut with an ellipsis past that), the note takes the rest and goes when
    // less than 160 units are left. `said` is the words as written ("": the caption of
    // no changes, which is never cut).
    protected void FfrFlowBar(string said)
    {
        TextWidget text = TextWidget.Cast(M_SUB_WIDGET.FindAnyWidget("FfrBarText"));
        Widget note = M_SUB_WIDGET.FindAnyWidget("FfrBarNote");
        Widget btn = M_SUB_WIDGET.FindAnyWidget("BtnFfrDiscard");
        if (!text || !note || !btn)
            return;
        Widget clip = note.GetParent();
        if (!clip)
            return;
        int tw = OZ_VppFlex.TextWidth(text);
        if (tw <= 0)
            return;

        float s = UnitScale();
        float x;
        float y;
        float w;
        float h;
        float bx;
        float by;
        text.GetPos(x, y);
        text.GetSize(w, h);
        btn.GetPos(bx, by);
        float most = bx - 12 * s - x;
        if (most < 40 * s)
            return;

        if (tw > most && said != "")
        {
            int n = said.LengthUtf8();
            float f = n;
            int keep = f * most / tw;
            keep = keep - 2;
            if (keep < 1)
                keep = 1;
            if (keep < n)
                text.SetText(said.SubstringUtf8(0, keep) + "…");
            tw = most;
        }
        text.SetSize(tw + 2, h);

        float at = x + tw + 12 * s;
        float room = bx - 12 * s - at;
        clip.Show(room >= 160 * s);
        if (room < 160 * s)
            return;
        clip.GetPos(x, y);
        clip.GetSize(w, h);
        clip.SetPos(at, y);
        clip.SetSize(room, h);
    }

    // Every 300 ms on this page (Tick): the bar follows what is typed in the fields.
    protected void FfrFollow()
    {
        int now = GetGame().GetTime();
        if (now - m_FfrTickAt < 300)
            return;
        m_FfrTickAt = now;
        FfrPaintBar();
    }

    // The factions the picked group's list goes through: the ids the draft already
    // uses, the base faction, those the server's source of factions names, then none.
    protected void FfrFacOptions(array<string> ids)
    {
        if (m_FfrDraft)
        {
            for (int i = 0; i < m_FfrDraft.GroupFactions.Count(); i++)
            {
                string id = m_FfrDraft.GroupFactions[i].FactionId;
                if (id != "" && ids.Find(id) == -1)
                    ids.Insert(id);
            }
            FfrReadFields();
            if (m_FfrDraft.BaseFaction != "" && ids.Find(m_FfrDraft.BaseFaction) == -1)
                ids.Insert(m_FfrDraft.BaseFaction);
        }
        for (int k = 0; k < m_IdFactions.Count(); k++)
        {
            if (ids.Find(m_IdFactions[k]) == -1)
                ids.Insert(m_IdFactions[k]);
        }
        ids.Insert("");
    }

    // A click on the list of factions moves it to the next one; an id typed under it
    // goes, as the list is what the admin is choosing with now.
    protected void FfrCycleFaction()
    {
        array<string> ids = new array<string>();
        FfrFacOptions(ids);
        int at = ids.Find(m_FfrFacPick) + 1;
        if (at >= ids.Count())
            at = 0;
        m_FfrFacPick = ids[at];
        if (m_FfrFacBox)
            m_FfrFacBox.SetText("");
        FfrPaintFac();
    }

    // The page's buttons; false: not one of them.
    protected bool FfrClick(string nm)
    {
        if (nm == "BtnFfrReread")
        {
            Ask(FFR_SECTION, FFR_OP_REREAD, "{}");
            AskCfg(FFR_CFG);
            return true;
        }

        if (nm == "BtnFfrFac")
        {
            if (m_FfrPick == "")
            {
                Hint("#STR_OZ_VPP_FFR_PICK");
                return true;
            }
            FfrCycleFaction();
            return true;
        }

        // The picked group gets the faction typed under the list, or the one the list
        // stands on; "no faction" drops the group's row from the draft.
        if (nm == "BtnFfrMap")
        {
            if (m_FfrPick == "")
            {
                Hint("#STR_OZ_VPP_FFR_PICK");
                return true;
            }
            if (!m_FfrDraft)
                return true;
            string id = m_FfrFacPick;
            if (m_FfrFacBox)
            {
                string typed = m_FfrFacBox.GetText().Trim();
                if (typed != "")
                    id = typed;
                m_FfrFacBox.SetText("");
            }
            m_FfrDraft.SetFaction(m_FfrPick, id);
            m_FfrFacPick = id;
            FfrRebuild();
            FfrPaintAll();
            return true;
        }

        // The two choices of who spends the pool: a label each, with its own tick box.
        bool spendAny = nm == "BtnFfrSpendAny" || nm == "FfrSpendAnyBox";
        bool spendNone = nm == "BtnFfrSpendNone" || nm == "FfrSpendNoneBox";
        if (spendAny || spendNone)
        {
            if (!m_FfrDraft)
                return true;
            m_FfrDraft.AnyMemberSpends = spendAny;
            FfrPaintSpend();
            FfrPaintTrade();
            FfrPaintBar();
            return true;
        }

        if (nm == "BtnFfrDiscard")
        {
            if (FfrChanges() == "")
            {
                Hint("#STR_OZ_VPP_FFR_NO_CHANGES");
                return true;
            }
            Confirm("#STR_OZ_VPP_FFR_DISCARD_ASK", "#STR_OZ_VPP_FFR_YES_DISCARD", "ffr:discard");
            return true;
        }

        if (nm == "BtnFfrApply")
        {
            if (FfrChanges() == "")
            {
                Hint("#STR_OZ_VPP_FFR_NO_CHANGES");
                return true;
            }
            Confirm("#STR_OZ_VPP_FFR_APPLY_ASK", "#STR_OZ_VPP_YES_APPLY", "ffr:apply");
            return true;
        }

        return false;
    }

    // The confirm strip's "yes": the whole draft into OZ_Identity_FFR.json, which the
    // glue's applier checks, writes (keeping a backup) and uses at once.
    protected void FfrApply()
    {
        if (!m_FfrDraft)
            return;
        FfrReadFields();
        string text = m_FfrDraft.ToJsonText();
        if (text == "")
        {
            OZ_Log.Error("vpp ffr: cannot build the settings text");
            return;
        }
        SendCfg(FFR_CFG, text);
    }

    protected void FfrDiscard()
    {
        if (!m_FfrSaved)
            return;
        m_FfrDraft = m_FfrSaved.Copy();
        FfrFillFields();
        FfrRebuild();
        FfrPaintAll();
    }

    // ---------------------------------------------------------- Ядро › Дзеркало Discord

    // Рядки будуються З ВІДПОВІДІ СЕРВЕРА, а не з розмітки: скільки родів
    // зареєстрували моди, стільки й рядків. A row per kind (the page CoreMirror.html,
    // row template oz_vpp_mirror_row); the row's key is the kind, and the three lists
    // beside the rows run in the rows' order.
    protected void BuildMirrorRows(OZ_MirrorState st)
    {
        if (!st || !st.Mirrors)
            return;
        if (!m_MrRows)
        {
            Widget host = M_SUB_WIDGET.FindAnyWidget("MrListRows");
            if (!host)
                return;
            m_MrRows = new OZ_VppFlexRows(m_Flex, host, "OpenZone_VPP/gui/layouts/oz_vpp_mirror_row.layout", "OZ_VppMirrorRow", 1312, 66);
        }

        m_MrRows.Clear();
        m_MirrorKinds.Clear();
        m_MirrorOn.Clear();
        m_MirrorNotes.Clear();

        for (int i = 0; i < st.Mirrors.Count(); i++)
        {
            OZ_KindMirror km = st.Mirrors[i];
            if (!km || km.Kind == "")
                continue;

            Widget row = m_MrRows.Add(km.Kind);
            if (!row)
            {
                OZ_Log.Error("vpp mirror: layout failed to load: " + km.Kind);
                continue;
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
        }

        m_MrRows.Fit();
        PaintMirror();
    }

    // Each row: the kind, its state (a sign and a word), the server's sentence on what
    // switching it does, and the switch's caption; in the page's head, how many kinds
    // there are.
    protected void PaintMirror()
    {
        int ink = ARGB(255, 224, 223, 218);
        int muted = ARGB(255, 160, 159, 153);
        int okInk = ARGB(255, 102, 199, 92);

        if (!m_MirrorKnown)
        {
            OvText("MrCount", "#STR_OZ_VPP_OV_ASKING", muted);
        }
        else
        {
            string countWord = OvTr("#STR_OZ_VPP_MR_COUNT");
            OvText("MrCount", countWord + " " + m_MirrorKinds.Count().ToString(), muted);
        }
        if (!m_MrRows)
            return;

        Widget row;
        Widget dot;
        string cap;
        for (int i = 0; i < m_MrRows.Count(); i++)
        {
            row = m_MrRows.Row(i);
            if (!row || i >= m_MirrorKinds.Count())
                continue;

            cap = m_MirrorKinds[i];
            cap.ToUpper();
            RowText(row, "MrRowKind", cap, ink);
            RowText(row, "MrRowNote", m_MirrorNotes[i], muted);
            dot = row.FindAnyWidget("MrRowDot");

            if (!m_MirrorKnown)
            {
                RowText(row, "MrRowState", "?", muted);
                RowText(row, "MrRowBtnText", "", ink);
                if (dot)
                    dot.SetColor(muted);
                continue;
            }
            if (m_MirrorOn[i] == 1)
            {
                RowText(row, "MrRowState", "#STR_OZ_VPP_MR_ON", okInk);
                RowText(row, "MrRowBtnText", "#STR_OZ_VPP_MR_TURN_OFF", ink);
                if (dot)
                    dot.SetColor(okInk);
            }
            else
            {
                RowText(row, "MrRowState", "#STR_OZ_VPP_MR_OFF", muted);
                RowText(row, "MrRowBtnText", "#STR_OZ_VPP_MR_TURN_ON", ink);
                if (dot)
                    dot.SetColor(muted);
            }
        }
    }

    // A click on a row's switch; false: not a switch of this list.
    protected bool MirrorRowClick(Widget w, string nm)
    {
        if (nm != "MrRowBtn" || !m_MrRows)
            return false;
        int i = m_MrRows.IndexOf(w);
        if (i == -1)
            return false;
        MirrorClick(i);
        return true;
    }

    // Один перемикач на рід, через смугу підтвердження (design §3.4). Речення про
    // наслідок складає СЕРВЕР -- сторінка в ядрі не знає, чим чат відрізняється
    // від ролей (дизайн платформи §4).
    protected void MirrorClick(int row)
    {
        if (row < 0 || row >= m_MirrorKinds.Count())
            return;

        if (!m_MirrorKnown)
        {
            Ask(OZ_AdminSect.CONFIG, "mirror_list", "{}");
            return;
        }

        string kind = m_MirrorKinds[row];
        string ask = OvTr("#STR_OZ_VPP_MR_ASK_ON");
        string yes = "#STR_OZ_VPP_MR_YES_ON";
        if (m_MirrorOn[row] == 1)
        {
            ask = OvTr("#STR_OZ_VPP_MR_ASK_OFF");
            yes = "#STR_OZ_VPP_MR_YES_OFF";
        }
        string line = ask + " " + kind;
        line = line + "?";
        if (m_MirrorNotes[row] != "")
            line = line + " " + m_MirrorNotes[row];
        Confirm(line, yes, "mirror:" + row.ToString());
    }

    // The confirm strip's "yes" for a mirror row.
    protected void MirrorSet(int row)
    {
        if (row < 0 || row >= m_MirrorKinds.Count())
            return;

        string kind = m_MirrorKinds[row];
        if (m_MirrorOn[row] == 1)
            Ask(OZ_AdminSect.CONFIG, "mirror_set:" + kind + ":off", "{}");
        else
            Ask(OZ_AdminSect.CONFIG, "mirror_set:" + kind + ":on", "{}");
    }

    // What the switch did, for the journal: the state, and the history it pushed.
    protected void MrReport(string kind, bool on, int pushed, int skipped, int failed, string note)
    {
        string state = OvTr("#STR_OZ_VPP_MR_OFF");
        if (on)
            state = OvTr("#STR_OZ_VPP_MR_ON");
        string line = kind + ": " + state;

        // A switch to the state it was in pushes nothing and says so.
        bool unchanged = false;
        if (skipped == 1 && pushed == 0 && failed == 0 && note.IndexOf("already") == 0)
            unchanged = true;
        if (unchanged)
        {
            string same = OvTr("#STR_OZ_VPP_MR_UNCHANGED");
            line = line + ", " + same;
        }
        else
        {
            string pushedWord = OvTr("#STR_OZ_VPP_MR_PUSHED");
            string skippedWord = OvTr("#STR_OZ_VPP_MR_SKIPPED");
            string failedWord = OvTr("#STR_OZ_VPP_MR_FAILED");
            line = line + "; " + pushedWord;
            line = line + " " + pushed.ToString();
            line = line + ", " + skippedWord;
            line = line + " " + skipped.ToString();
            line = line + ", " + failedWord;
            line = line + " " + failed.ToString();
        }
        if (note != "")
            line = line + " - " + note;
        Hint(line);
    }

    // ---------------------------------------------------------- the core's pages: shared

    // A tick box or a radio of a converted page: a button whose children are, in the
    // layout's order, its frame, its ground and its mark (<name>Mark). On: filled in the
    // accent colour, the mark shown; off: the cell's ground in a faint frame, no mark.
    protected void CorePaintTick(string name, bool on)
    {
        Widget box = M_SUB_WIDGET.FindAnyWidget(name);
        if (!box)
            return;

        int frame = ARGB(255, 92, 90, 88);
        int ground = ARGB(255, 7, 7, 7);
        if (on)
        {
            frame = ARGB(255, 232, 120, 43);
            ground = ARGB(255, 232, 120, 43);
        }
        string markName = name + "Mark";
        int n = 0;
        Widget part = box.GetChildren();
        while (part)
        {
            if (part.GetName() == markName)
            {
                part.Show(on);
            }
            else
            {
                if (n == 0)
                    part.SetColor(frame);
                else
                    part.SetColor(ground);
                n++;
            }
            part = part.GetSibling();
        }
    }

    // Two texts of one line and one parent: the first gets the width of its words (a
    // text is cut at its widget's width, and the layout sized it for the board's sample)
    // and the second stands `gap` units after its last letter, whatever language it is
    // in, up to the edge it ended at. A text that was not drawn yet may answer no width:
    // both keep what they have until it does.
    protected void CoreFlowAfter(string first, string second, float gap)
    {
        TextWidget a = TextWidget.Cast(M_SUB_WIDGET.FindAnyWidget(first));
        Widget b = M_SUB_WIDGET.FindAnyWidget(second);
        if (!a || !b)
            return;
        int tw = OZ_VppFlex.TextWidth(a);
        if (tw <= 0)
            return;

        float x;
        float y;
        float w;
        float h;
        float bx;
        float by;
        float bw;
        float bh;
        a.GetPos(x, y);
        a.GetSize(w, h);
        b.GetPos(bx, by);
        b.GetSize(bw, bh);

        // The pair ends where the second ends now, and that edge stays: the first is cut
        // there if its words are longer, the second keeps what is left.
        float end = bx + bw;
        float aw = tw + 2;
        if (aw > end - x)
            aw = end - x;
        if (aw < 1)
            aw = 1;
        float at = x + aw + gap * UnitScale();
        float room = end - at;
        if (room < 1)
        {
            at = end - 1;
            room = 1;
        }
        a.SetSize(aw, h);
        b.SetPos(at, by);
        b.SetSize(room, bh);
    }

    // The legend of a fieldset (a frame <name> with its ground <name>Bg and its caption
    // <name>Text): as wide as its words with the board's 6 units on each side, so a longer
    // caption of the other language is not cut and the box's line stops under it.
    protected void CoreHugLegend(string name)
    {
        Widget box = M_SUB_WIDGET.FindAnyWidget(name);
        Widget bg = M_SUB_WIDGET.FindAnyWidget(name + "Bg");
        TextWidget t = TextWidget.Cast(M_SUB_WIDGET.FindAnyWidget(name + "Text"));
        if (!box || !t)
            return;
        if (OZ_VppFlex.TextWidth(t) <= 0)
            return;
        OZ_VppFlex.Hug(box, bg, t, 6 * UnitScale());
    }

    // One of the core's pages was just shown, or the window changed its size while it
    // was open (Tick): what the page lays out by script follows -- the rows of its lists
    // as wide as their lists (a resize puts them back to the width they had), the words
    // that stand by their length, the scheme, the cards that hug their text.
    protected void CoreRefit(string id)
    {
        if (id == "spawns")
        {
            if (m_SpRows)
            {
                m_SpHeads.Fit();
                m_SpRows.Fit();
            }
            SpDrawScheme();
        }
        if (id == "news")
        {
            if (m_NwRows)
                m_NwRows.Fit();
            m_NwFitDue = 3;
        }
        if (id == "players")
        {
            if (m_PlrRows)
                m_PlrRows.Fit();
            m_PlrFlowDue = 3;
            CoreHugLegend("PlrCharTag");
            CoreHugLegend("PlrLinksTag");
            CoreHugLegend("PlrWipeTag");
        }
        if (id == "mirror" && m_MrRows)
            m_MrRows.Fit();
        if (IsFilesPage(id))
        {
            if (m_RawFiles)
            {
                m_RawHeads.Fit();
                m_RawFiles.Fit();
                m_RawNones.Fit();
            }
            RawFlowStatus();
        }
        if (id == "ffr")
        {
            if (m_FfrRows)
                m_FfrRows.Fit();
            CoreFlowAfter("FfrGroupsHead", "FfrGroupsCount", 10);
            CoreFlowAfter("FfrPickName", "FfrPickInfo", 8);
            CoreHugLegend("FfrAllTag");
            FfrPaintBar();
        }
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
