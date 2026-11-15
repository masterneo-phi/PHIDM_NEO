
static	char	mt_name[16][16]=
{
	"Leather","Wood","Stone","Steel","Silver","Mythril","Auric","Adamant",
	"Magic","-","-","-","-","-","-","No Break"
};

static	char	fn_name[10][16]=
{
	"None","Earth","Light","Air","Spirit","Water","Fire","Mirror","Prism","Rainbow"
};

static	char	wt_name[16][16]=
{
	"claw","sword","blade","rapier","spear","axe","hammer",
	"whip","fang","wide","around","-","-","-","-","-"
};

static  char  it_name[16][16]=
{
  "No Use","Armor","Weapon","Food","Resource","Jewel",
  "Charged item","Accessories","Spell jewel", "-", "-", "-",
  "Evently", "Scroll", "Currency", "-"
};

/* special と Saga の部分は各自のサーヴァにあわせてエディットしてください */
static	char	ae_name[100][16]=
{
	"None","Poison","Palsy","Panic","Confuse",
	"Berserk","Silence","Drain","Robbery","Mental",
	"Energy","Holy","Chaos","Blind","Knock back",
	"Random", "Badstatus", "Rick killer", "Special", "Special",
	"Saga", "Saga", "Saga", "Saga", "Dragon killer",
	"God killer", "Element killer", "Demon killer", "Special", "Special",
	"", "", "", "", "",
	"", "", "", "", "",
	"", "", "", "", "",
	"", "", "", "", "",
	"", "", "", "", "",
	"", "", "", "", "",
	"", "", "", "", "",
	"", "", "", "", "",
	"", "", "", "", "",
	"", "", "", "", "",
	"", "", "", "", "",
	"", "", "", "", "",
	"", "", "", "", "",
	"", "", "", "", ""
};

static	char	at_name[100][16]=
{
	"None","Poison proof","Palsy proof","Drain proof","Thief guard",
	"Mental proof","See invisible","Immortal","Iron stomach","Absorb",
	"Stealth", "Fly", "Head guard", "Umbrella", "",
	"", "", "", "", "",
	"", "", "", "", "",
	"", "", "", "", "",
	"", "", "", "", "",
	"", "", "", "", "",
	"", "", "", "", "",
	"", "", "", "", "",
	"", "", "", "", "",
	"", "", "", "", "",
	"", "", "", "", "",
	"", "", "", "", "",
	"", "", "", "", "",
	"", "", "", "", "",
	"", "", "", "", "",
	"", "", "", "", "",
	"Counter attack", "", "", "", "",
	"", "", "", "", ""
};

static	char	wt_aname[16][16]=
{
	"ring","sheathe","sheathe","shield","symbol","symbol","shield","rope",
	"amulet","ring","ring","amulet","amulet","amulet","amulet","amulet"
};

static	char	se_name[100][16]=
{
	"None","Power","Degrade","Protect","Anti-protect",
	"Shell","Anti-shell","Haste","Slow","Illusion",
	"Regenerate","Will","Poison","Palsy","Panic",
	"Confuse","Berserk","Silence","Vice","Virtue",
	"Clairvoyance","Blind","Raise dead","Pass wall","Giant growth",
	"Vanish","Mirror","Spell","Anti-spell","Stealth",
	"Auto effector", "Backfire","Shrink","Root","Anti-illusion",
	"", "", "", "", "",
	"", "", "", "", "",
	"", "", "", "", "",
	"", "", "", "", "",
	"", "", "", "", "",
	"", "", "", "", "",
	"", "", "", "", "",
	"", "", "", "", "",
	"", "", "", "", "",
	"", "", "", "", "",
	"", "", "", "", "",
	"", "", "", "", "",
	"", "", "", "", ""
};

static	char	ci_name[100][16]=
{
	"None","Hp line","Hp pierce","Hp explode","Hp sphere",
	"Mp line","Mp pierce","Mp explode","Mp sphere","Healing",
	"Healing","Healing","Power","Degrade","Spell",
	"Discast", "Protect","Reduce","Shell","Drain",
	"Speed","Delay","Illusion","Poison","Palsy",
	"Panic","Confuse","Berserk","Silence","Antidote",
	"Remove","Awaken","Force","Reflex","Erase",
	"Restorative","Contrast","","","",
	"Roof shake","Rain","Quake","Roof shake","Rain",
	"Quake","","","","",
	"","","","","",
	"","","","","",
	"",
	"Mirror","Vanish", "Blind","Hp divide","Mp divide",
	"Remote get","Remote put","Revival","","",
	"","","","","",
	"","","","","",
	"","","","","",
	"","","","","",
	"","","","Trap","Trap",
	"Mine","Mine","Disappear","Appear"
};

static	char	rs_effect[100][16]=
{
	"Resolver","Plant","Tree","Water","Wall",
	"Window","Door","Secret door","Protection","Glass",
	"Roof", "", "", "", "",
	"", "", "", "", "",
	"", "", "", "", "",
	"", "", "", "", "",
	"", "", "", "", "",
	"", "", "", "", "",
	"", "", "", "", "",
	"", "", "", "", "",
	"", "", "", "", "",
	"", "", "", "", "",
	"", "", "", "", "",
	"", "", "", "", "",
	"", "", "", "", "",
	"", "", "", "", "",
	"", "", "", "", "",
	"", "", "", "", "",
	"", "", "", "", "",
	"", "", "", "", "-",
};

static	char	use_effect[100][32]=
{
	"None","Hp damage (Line)","Hp damage (Pierce)","Hp damage (Explode)",
	"Hp damage (Sphere)","Mp damage (Line)","Mp damage (Pierce)","Mp damage (Explode)",
	"Mp damage (Sphere)","Heal","Heal (Sphere)","Regenerate","Coat","Degrade",
	"Spell up", "Spell down", "Protect","Anti-protect","Shell","Anti-shell","Haste","Slow","Illusion",
	"Poison","Palsy","Panic","Confuse","Berserk","Silence",
	"Antidote","Remove","Awaken",
	"Force field","Reflex field","Erase",
	"Heal (Mental)","Contrast","","","",
	"Roof Shake (Hp)","Rain (Hp)","Quake (Hp)","Roof Shake (Mp)","Rain (Mp)",
	"Quake (Mp)","","","","",
	"","","","","",
	"","","","","",
	"",
	"Mirror","Vanish","Blind","Divide life","Divide mind",
	"Remote get","Remote put","Revive","","",
	"","","","","",
	"","","","","",
	"","","","","",
	"","","","","",
	"","","","Trap(HP)","Trap(MP)",
	"Mine(HP)","Mine(MP)","Appear", "Disappear"
};

static	char	foods[100][32]=
{
    "Food", "Milk Nuts", "Milk", "Gorp", "Refreshments",
    "Tea", "CoCo Nuts", "Broccoli", "Cress", "Pickles",
    "Pork", "Rib", "Roast", "Salmon", "Satire",
    "Shank", "Sinew", "Veal", "Venison", "Vol-au-vent",
    "Beet", "Radish", "Carrot", "Apple", "Miso",
    "Wine", "Black Coffee", "American Coffee", "Beer", "Grape",
    "Plum", "Lemon", "melon", "Dressing", "Orange",
    "Sauce", "Whiskey", "Thyme", "Primrose", "Snow drop",
    "Herb", "Laughing stock", "Licorice", "Liquorice", "Provender", 
    "Seaweed", "Spinach", "Wildflower", "Zinnia", "Chrysanthemum",
    "Groundnut", "Magnolia", "Mayflower", "Metathesis", "Narcissus",
    "Nectar", "Rowan", "Rose", "Sasanqua", "Tulip",
    "Chinese Cabbage", "Relish", "Vegetable", "Barley", "Corn",
    "Flour", "Malt", "Potato", "Wheat", "Oats", 
    "Curry", "Soup", "Puree", "Casserole", "French toast",
    "Bay leaf", "Custard", "Entree", "Flan", "Hash",
    "Linguini", "Marjoram", "Quenelle", "Vegetarian cuisine", "Beefsteak",
    "Hanburger", "Prime rib", "Steak", "Dr pepper", "Grapefruit",
    "Durian", "Lobster", "Frosty adiposing beef", "Tempura", "Sasimi", 
    "Unaju", "Sushi", "Matsutake", "Fruit pafe", "Food of Foods"
};
