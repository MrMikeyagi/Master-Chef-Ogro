// console message types
enum
{
    CON_CHAT       = 1<<8,
    CON_TEAMCHAT   = 1<<9,
    CON_GAMEINFO   = 1<<10,
    CON_FRAG_SELF  = 1<<11,
    CON_FRAG_OTHER = 1<<12
};

// network quantization scale
#define DMF 16.0f                // for world locations
#define DNF 100.0f              // for normalized vectors
#define DVELF 1.0f              // for playerspeed based velocity vectors

const char* DEFAULT_PLAYER_MODEL = "rc";

#define m_demo         (gamemode==-1)
#define m_default      (gamemode==0)

// network messages codes, c2s, c2c, s2c

enum { PRIV_NONE = 0, PRIV_MASTER, PRIV_ADMIN };

enum
{
    SV_INITS2C = 0, SV_INITC2S, SV_POS, SV_TEXT, SV_SOUND, SV_CDIS,
    SV_SPAWNSTATE, SV_SPAWN,
    SV_MAPCHANGE, SV_MAPVOTE,
    SV_PING, SV_PONG, SV_CLIENTPING,
    SV_SERVMSG, SV_RESUME,
    SV_EDITMODE, SV_EDITENT, SV_EDITF, SV_EDITT, SV_EDITM, SV_FLIP, SV_COPY, SV_PASTE, SV_ROTATE, SV_REPLACE, SV_DELCUBE, SV_REMIP, SV_NEWMAP, SV_GETMAP, SV_SENDMAP,
		SV_MASTERMODE, SV_KICK, SV_CLEARBANS, SV_CURRENTMASTER, SV_SPECTATOR, SV_SETMASTER, SV_APPROVEMASTER,
    SV_LISTDEMOS, SV_SENDDEMOLIST, SV_GETDEMO, SV_SENDDEMO,
    SV_DEMOPLAYBACK, SV_RECORDDEMO, SV_STOPDEMO, SV_CLEARDEMOS,
    SV_CLIENT, 
    SV_ADDACTOR, SV_DELACTOR, SV_SPAWNACTOR, SV_ACTORSIG
};

//ToDo: set SV_SPAWNSTATE to 1 -> remove sendstate
static char msgsizelookup(int msg)
{
	static char msgsizesl[] =               // size inclusive message token, 0 for variable or not-checked sizes
    {
        SV_INITS2C, 4, SV_INITC2S, 0, SV_POS, 0, SV_TEXT, 0, SV_SOUND, 2, SV_CDIS, 2,
        SV_SPAWNSTATE, 1, SV_SPAWN, 1,
        SV_MAPCHANGE, 0, SV_MAPVOTE, 0,
        SV_PING, 2, SV_PONG, 2, SV_CLIENTPING, 2,
        SV_SERVMSG, 0, SV_RESUME, 0,
        SV_EDITMODE, 2, SV_EDITENT, 11, SV_EDITF, 16, SV_EDITT, 16, SV_EDITM, 15, SV_FLIP, 14, SV_COPY, 14, SV_PASTE, 14, SV_ROTATE, 15, SV_REPLACE, 16, SV_DELCUBE, 14, SV_REMIP, 1, SV_NEWMAP, 2, SV_GETMAP, 1, SV_SENDMAP, 0,
        SV_MASTERMODE, 2, SV_KICK, 2, SV_CLEARBANS, 1, SV_CURRENTMASTER, 3, SV_SPECTATOR, 3, SV_SETMASTER, 0, SV_APPROVEMASTER, 2,
        SV_LISTDEMOS, 1, SV_SENDDEMOLIST, 0, SV_GETDEMO, 2, SV_SENDDEMO, 0,
        SV_DEMOPLAYBACK, 2, SV_RECORDDEMO, 2, SV_STOPDEMO, 1, SV_CLEARDEMOS, 2,
        SV_CLIENT, 0, 
        SV_ADDACTOR, 0, SV_DELACTOR, 2, SV_SPAWNACTOR, 0, SV_ACTORSIG, 0,
        -1
    };
    for(char *p = msgsizesl; *p>=0; p += 2) if(*p==msg) return p[1];
    return -1;
}

#define MOVIECUBE_SERVER_PORT 28785
#define MOVIECUBE_SERVINFO_PORT 28786
#define PROTOCOL_VERSION 10            // bump when protocol changes
#define DEMO_VERSION 1                  // bump when demo format changes
#define DEMO_MAGIC "MOVIECUBE_DEMO"

#define MAXNAMELEN 15
#define MAXMODELLEN 254
#define MAXCLIENTSPEED 1000
#define MINCLIENTSPEED 10

// inherited by movieent and server clients
struct moviestate
{
    moviestate() {}
    void respawn() { return; }
    void spawnstate(int gamemode) { return; }
};

struct movieent : dynent, moviestate
{   
	int clientnum;
	int privilege;
	int lastupdate;
	int plag;
	int ping;
	editinfo *edit;
	float deltayaw;
	float deltapitch;
	float newyaw;
	float newpitch;
	int smoothmillis;
	
	int keypitch; 
	int keyturn; 
	
	string name;
	string mdl;
	string info;
  bool scripted;
  bool flat; //ToDo render "flat"
  
	movieent() : clientnum(-1), privilege(PRIV_NONE), lastupdate(0), plag(0), ping(0), edit(NULL), smoothmillis(-1), scripted(false)
	{ 
		name[0] = info[0] = 0;
		s_strcpy(mdl, DEFAULT_PLAYER_MODEL);
		respawn(); }
	~movieent() { freeeditinfo(edit); }
	
	void respawn()
	{
		dynent::reset();
		moviestate::respawn();
	}
};
