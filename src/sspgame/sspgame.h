// console message types

enum
{
	CON_GAMEINFO = 1<<8, //ie, A to blah, D to blah
	CON_COLLECT = 1<<9, //ie, collecting an item
	CON_KILL = 1<<10 //just when an enemy is defeated :P
};

enum                            // static entity types
{
	NOTUSED = ET_EMPTY,         // entity slot not in use in map
	LIGHT = ET_LIGHT,           // lightsource, attr1 = radius, attr2 = intensity
	MAPMODEL = ET_MAPMODEL,     // attr1 = angle, attr2 = idx
	PLAYERSTART,                // attr1 = angle, attr2 = team
	ENVMAP = ET_ENVMAP,         // attr1 = radius
	PARTICLES = ET_PARTICLES,
	MAPSOUND = ET_SOUND,
	SPOTLIGHT = ET_SPOTLIGHT,
	BOX, //attr1 = yaw, attr2 = model, attr3 = script, attr4 = pickup
	PICKUP, //attr1 = itemtype, attr2 = item, as defined in script
	ENEMY, //attr1 = yaw, attr2 = enemy, as defined is a script, attr3 = first waypoint
	WAYPOINT, //attr1 = number, attr2 and attr3 is linked waypoints, as long as they're above 0, an enemy will head to that specific waypoint
	TELEPORT, //attr1 = teledest, attr2 = radius (0 = 16), attr3 = model
	TELEDEST, //attr1 = yaw, attr2 = from
	CHECKPOINT, //attr1 = yaw, attr2 = radius, attr3 = model
	JUMPPAD, //attr1 = Z, attr2 = Y, attr3 = X, attr4 = radius
	PLATFORM, //attr1 = yaw, attr2 = model, attr3 = route, attr4 = speed
	PLATFORMROUTE, //attr1 = route, attr2 = linkedent1, attr3 = linkedent2
	CAMERA, //attr1 = number, attr2 = yaw, attr3 = pitch, attr4 = distance
	AXIS, //attr1 == 0 == manual, else automatic oneway switch, attr2 = axis script, ie camera change //switches the player's yaw
	MAXENTTYPES
};

enum
{
	PICKUP_COIN = 0,
	PICKUP_WEAPON,
	PICKUP_ARMOUR,
	PICKUP_HEALTH,
	PICKUP_TIME,
	PICKUP_LIVES
};

enum //armour : if armour or a weapon is equipped, lose it instead of health
{
	ARM_NONE = 0,
	ARM_PLAIN, //no special abbilities;
	ARM_ATTRACT, //quadruples pickup range of well.. pickups
	ARM_FLY, //the player can jump repeatedly in the air for extra height
	ARM_SPIKE, //kills whatever lands on the player, at the cost of the armour
	ARM_MAX
};

//object structs, mdl is representation model

struct coinpickup
{
	const char *mdl;
	int amount;
	
	coinpickup(const char *_mdl, int _mnt)
	{
		mdl = newstring(_mdl[0] ? _mdl : "tentus/moneybag");
		amount = _mnt ? _mnt : 0;
	}
	~coinpickup();
};

struct weaponpickup
{
	const char *mdl, *attachmdl;
	int projectile, sound;
	
	weaponpickup(const char *_mdl, const char *_amdl, int _proj, int _snd)
	{
		mdl = newstring(_mdl[0] ? _mdl : "tentus/moneybag");
		attachmdl = newstring(_amdl[0] ? _amdl : "banana");
		projectile = _proj ? _proj : 0;
		sound = _snd ? _snd : 0;
	}	
	~weaponpickup();
};

struct armourpickup
{
	const char *mdl, *attachmdl;
	int type;
	
	armourpickup(const char *_mdl, const char *_amdl, int _tp)
	{
		mdl = newstring(_mdl[0] ? _mdl : "tentus/moneybag");
		attachmdl = newstring(_amdl[0] ? _amdl : "banana");
		type = _tp ? _tp : 1;
	}
	~armourpickup();
};

struct healthpickup
{
	const char *mdl;
	int amount; //max at 2
	
	healthpickup(const char *_mdl, int _mnt)
	{
		mdl = newstring(_mdl[0] ? _mdl : "tentus/moneybag");
		amount = _mnt ? _mnt : 0;
	}
	~healthpickup();
};

struct timepickup
{
	const char *mdl;
	int amount; //in seconds
	
	timepickup(const char *_mdl, int _mnt)
	{
		mdl = newstring(_mdl[0] ? _mdl : "tentus/moneybag");
		amount = _mnt ? _mnt : 0;
	}
	~timepickup();
};

struct lifepickup
{
	const char *mdl;
	int amount;
	
	lifepickup(const char *_mdl, int _mnt)
	{
		mdl = newstring(_mdl[0] ? _mdl : "tentus/moneybag");
		amount = _mnt ? _mnt : 0;
	}
	~lifepickup();
};

struct sspentity : extentity {}; //extend with additional properties if needed

enum
{
    S_JUMP = 0, S_LAND, S_UNUSED, S_TELEPORT, S_SPLASH1, S_SPLASH2, S_CG, 
    S_RLFIRE, S_RUMBLE, S_JUMPPAD, S_WEAPLOAD, S_ITEMAMMO, S_ITEMHEALTH, 
    S_ITEMARMOUR, S_ITEMPUP, S_ITEMSPAWN,  S_NOAMMO, S_PUPOUT, 
    S_PAIN1, S_PAIN2, S_PAIN3, S_PAIN4, S_PAIN5, S_PAIN6, 
    S_DIE1, S_DIE2, 
    S_FLAUNCH, S_FEXPLODE, 
    S_SG, S_PUNCH1, 
    S_GRUNT1, S_GRUNT2, S_RLHIT, 
    S_PAINO, 
    S_PAINR, S_DEATHR, 
    S_PAINE, S_DEATHE, 
    S_PAINS, S_DEATHS, 
    S_PAINB, S_DEATHB, 
    S_PAINP, S_PIGGR2, 
    S_PAINH, S_DEATHH, 
    S_PAIND, S_DEATHD, 
    S_PIGR1, S_ICEBALL, S_SLIMEBALL, S_PISTOL, 
 
    S_V_BASECAP, S_V_BASELOST, 
    S_V_FIGHT, 
    S_V_BOOST, S_V_BOOST10, 
    S_V_QUAD, S_V_QUAD10, 
    S_V_RESPAWNPOINT, 

    S_FLAGPICKUP,
    S_FLAGDROP,
    S_FLAGRETURN,
    S_FLAGSCORE,
    S_FLAGRESET,

    S_BURN
};

struct sspstate
{
	int health, maxhealth;
	int armour, armourvec;
	int powerupmillis;
	int gunselect; //gunselect, weapon vector(i-1);

	sspstate() : maxhealth(3) {}


	void respawn()
	{
		health = 1;
		armour = 0;
		armourvec = -1;
		powerupmillis = 0;
		gunselect = -1;
	}
};

struct sspent : dynent, sspstate
{   
	int weight;                         // affects the effectiveness of hitpush
	int lifesequence;                   // sequence id for each respawn, used in damage test
	int lastpain;
	int lastaction;
	bool attacking;
	int lastpickupmillis;
	int powerup;
	editinfo *edit;
	float deltayaw, deltapitch, newyaw, newpitch;
	int lives, coins, checkpoint;

	string name;

	sspent() : weight(100), lifesequence(0), lastpain(0), edit(NULL), lives(5), coins(0) { name[0] = 0; maxspeed = 80; respawn(); }
	~sspent() { freeeditinfo(edit); }

	void respawn()
	{
		dynent::reset();
		sspstate::respawn();
		lastaction = 0;
		attacking = false;
		lastpickupmillis = 0;
		powerup = -1;
	}
};

struct enemy //enemy declaration
{
	const char *mdl;
	int health, speed, painsound, diesound;
	
	enemy(const char *_mdl, int _hlt, int _spd, int _ps, int _ds) {
		mdl = newstring(_mdl[0] ? _mdl : "rc/red");
		health = _hlt ? _hlt : 3;
		speed = _spd ? _spd : 80;
		painsound = _ps ? _ps : S_PAIN1;
		diesound = _ds ? _ds : S_DIE1;
	}
	~enemy();
};

struct proj //ectiles
{
	const char *mdl;
	int damage, radius, force, travelsound, speed;
	
	proj(const char *_mdl, int _dmg, int _rad, int _frce, int _ts, int _spd) {
		mdl = newstring(_mdl[0] ? _mdl : "banana");
		damage = _dmg ? _dmg : 1;
		radius = _rad ? _rad : 1;
		force = _frce ? _frce : 1;
		travelsound = _ts ? _ts : 1;
		speed = _spd ? _spd : 1;
	}
	~proj();		
};

struct projectile //the actual flying ones; move to sspgame.h when done
	{
	int prj;
	vec o, d;

	projectile(int _prj, vec _o, float _yaw, float _pitch)
	{
		prj = _prj;
			
		vecfromyawpitch(_yaw ? _yaw : 0, _pitch ? _pitch : 0, false, true, d);
		vec p = d;
		p.mul(4);

		o = _o;
		o.add(p);
	}
	~projectile();
};

struct eventimage
{
	const char *tex;
	int deltime;
	int starttime;
	
	eventimage(const char *_tex, int _dt)
	{
		tex = newstring(_tex[0] ? _tex : "data/sandboxlogo");
		deltime = lastmillis + (_dt ? _dt : 2000);
		starttime = lastmillis;
	}
	~eventimage();
};


