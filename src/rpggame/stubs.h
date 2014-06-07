struct rpgdummycom : iclientcom
{
    ~rpgdummycom() {}

    void gamedisconnect() {}
    void parsepacketclient(int chan, ucharbuf &p) {}
    int sendpacketclient(ucharbuf &p, bool &reliable, dynent *d) { return -1; }
    void gameconnect(bool _remote) {}
    bool allowedittoggle() { return true; }
    void writeclientinfo(FILE *f) {}
    void toserver(char *text) {}
    void changemap(const char *name) { load_world(name); }
    void connectattempt(const char *name, const char *password, const ENetAddress &address) {}
    void connectfail() {}
};

struct rpgdummyserver : igameserver
{
    ~rpgdummyserver() {}

    void *newinfo() { return NULL; }
    void deleteinfo(void *ci) {}
    void serverinit() {}
    void clientdisconnect(int n) {}
    int clientconnect(int n, uint ip) { return DISC_NONE; }
    void localdisconnect(int n) {}
    void localconnect(int n) {}
    const char *servername() { return "foo"; }
    void parsepacket(int sender, int chan, bool reliable, ucharbuf &p) {}
    bool sendpackets() { return false; }
    int welcomepacket(ucharbuf &p, int n, ENetPacket *packet) { return -1; }
    void serverinforeply(ucharbuf &q, ucharbuf &p) {}
    void serverupdate(int lastmillis, int totalmillis) {}
    bool servercompatible(char *name, char *sdec, char *map, int ping, const vector<int> &attr, int np) { return false; }
    void serverinfostr(char *buf, const char *name, const char *desc, const char *map, int ping, const vector<int> &attr, int np) {}
    int serverinfoport() { return 0; }
    int serverport() { return 0; }
    const char *getdefaultmaster() { return "localhost"; }
    void sendservmsg(const char *s) {}
    int reserveclients() { return 0; }
    bool allowbroadcast(int n) { return false; }
};
//universal sound definitions
enum
{
    S_JUMP = 0, S_LAND, S_RIFLE, S_TELEPORT, S_SPLASH1, S_SPLASH2, S_CG,  
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

struct rpgeffect //to make up for the loss of old particle numbers
{
	int type, colour;
	float size, vel;
	
	rpgeffect(int _t, int _c, float _s, float _v)
	{
		type = _t ? _t : PART_FIREBALL1;
		colour = _c ? min(0xFFFFFF, max(_c, 0)) : 0xFF7F00;
		size = _s ? _s : 2.0f;
		vel = _v ? _v : 200.0f;
	}
};

	