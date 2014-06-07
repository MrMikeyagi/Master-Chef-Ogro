#include "pch.h"
#include "cube.h"

#include "iengine.h"
#include "igame.h"

#include "moviecube.h"
#include "movieserver.h"

#ifndef STANDALONE

struct movieclient : igameclient
{
// these define classes local to movieclient
//~ #include "movable.h"
#include "movierender.h"
#include "movieentities.h"
#include "movieclient.h"
#include "movieactor.h" 
#include "moviecamera.h"
#include "movietimer.h"


	int nextmode, gamemode;         // nextmode becomes gamemode after next map load
	string clientmap;

	int following, followdir;
  
	bool openmainmenu;
	
	movieent *player1;                // our client
	vector<movieent *> players;       // other clients
	movieent lastplayerstate;
	
	//~ movableset  mo;
	movierender   	fr;
	movieentities   et;
	movieclientcom  cc;
	movieactorset   as;
	moviecamera     ca;
  movietimer      tm;
	
	movieclient()
		: nextmode(0), gamemode(0),
	following(-1), followdir(0), openmainmenu(false),
	player1(spawnstate(new movieent())),
	fr(*this), et(*this), cc(*this), as(*this), ca(*this), tm(*this)
	{
		CCOMMAND(mode, "i", (movieclient *self, int *val), { self->setmode(*val); });
		CCOMMAND(follow, "s", (movieclient *self, char *s), { self->follow(s); });
		CCOMMAND(nextfollow, "i", (movieclient *self, int *dir), { self->nextfollow(*dir < 0 ? -1 : 1); });
		
        CCOMMAND(pitchup, "D", (movieclient *self,int *down), { *down ? self->player1->keypitch=2 : self->player1->keypitch=0; }); 
        CCOMMAND(pitchdown, "D", (movieclient *self,int *down), { *down ? self->player1->keypitch=1 : self->player1->keypitch=0; }); 
        CCOMMAND(turnleft, "D", (movieclient *self,int *down), { *down ? self->player1->keyturn=2 : self->player1->keyturn=0; }); 
        CCOMMAND(turnright, "D", (movieclient *self,int *down), { *down ? self->player1->keyturn=1 : self->player1->keyturn=0; }); 
        CCOMMAND(animate, "D", (movieclient *self, int *down), { self->player1->animate = (*down!=0); }); 
	}

	iclientcom      *getcom()  { return &cc; }
	icliententities *getents() { return &et; }
	
	int getgamemode() {  
		return gamemode;  
	}

	void setmode(int mode)
	{
		if(multiplayer(false) && !m_default) { conoutf(CON_ERROR, "mode %d not supported in multiplayer", mode); return; }
		nextmode = mode;
	}

	void follow(char *arg)
	{
		if(arg[0] ? player1->state==CS_SPECTATOR : following>=0)
		{
			following = arg[0] ? cc.parseplayer(arg) : -1;
			if(following==player1->clientnum) following = -1;
			followdir = 0;
			conoutf("follow %s", following>=0 ? "on" : "off");
		}
	}
  
	void nextfollow(int dir)
	{
		if(player1->state!=CS_SPECTATOR || players.empty())
		{
			stopfollowing();
			return;
		}
		int cur = following >= 0 ? following : (dir < 0 ? players.length() - 1 : 0);
		loopv(players) 
		{
			cur = (cur + dir + players.length()) % players.length();
			if(players[cur])
			{
				if(following<0) conoutf("follow on");
				following = cur;
				followdir = dir;
				return;
			}
		}
		stopfollowing();
	}
  
	char *getclientmap() { return clientmap; }
	
	void adddynlights() {} //{ ws.adddynlights(); }
	
	void rendergame() { fr.rendergame(gamemode); }
	
	void resetgamestate()
	{
		//~ resettriggers();
	}

	movieent *spawnstate(movieent *d)              // reset player state not persistent accross spawns
	{
		d->respawn();
		d->spawnstate(gamemode);
		return d;
	}
	
	void respawnself() {}
	
	movieent *pointatplayer()
	{
		loopv(players)
		{
			movieent *o = players[i];
			if(!o) continue;
			if(intersect(o, player1->o, worldpos)) return o;
		}
		return NULL;
	}
	
	void stopfollowing()
	{
		if(following<0) return;
		following = -1;
		followdir = 0;
		conoutf("follow off");
	}
	
	movieent *followingplayer()
	{
		if(player1->state!=CS_SPECTATOR || following<0) return NULL;
		movieent *target = getclient(following);
		if(target && target->state!=CS_SPECTATOR) return target;
		return NULL;
	}
	  
	movieent *hudplayer()
	{
		extern int thirdperson;
		if(thirdperson) return player1;
		movieent *target = followingplayer();
		return target ? target : player1;
	}
	
	void setupcamera()
	{
		movieent *target = followingplayer();
		if(target) 
		{
			player1->yaw = target->yaw;    
			player1->pitch = target->pitch;
			player1->o = target->o;
			player1->resetinterp();
      return;
    }
 	}

  bool detachcamera()
	{
    return ca.cameramode();
	}
	
	IVARP(smoothmove, 0, 75, 100);
	IVARP(smoothdist, 0, 32, 64);
	
	void otherplayers(int curtime)
	{
		loopv(players) if(players[i])
		{
			movieent *d = players[i];
			const int lagtime = lastmillis-d->lastupdate;
      if(!lagtime) continue;
			else if(lagtime>1000 && d->state==CS_ALIVE)
			{
				d->state = CS_LAGGED;
				continue;
			}
			if(d->state==CS_ALIVE || d->state==CS_EDITING)
			{
				if(smoothmove() && d->smoothmillis>0)
				{
					d->o = d->newpos;
					d->yaw = d->newyaw;
					d->pitch = d->newpitch;
					moveplayer(d, 1, false);
					d->newpos = d->o;
					float k = 1.0f - float(lastmillis - d->smoothmillis)/smoothmove();
					if(k>0)
					{
						d->o.add(vec(d->deltapos).mul(k));
						d->yaw += d->deltayaw*k;
						if(d->yaw<0) d->yaw += 360;
						else if(d->yaw>=360) d->yaw -= 360;
						d->pitch += d->deltapitch*k;
					}
				}
				else moveplayer(d, 1, false);
			}
		}
	}
 
	void updateworld()        // main game update loop
	{
		if(!curtime) return;
		
       //turn / pitch 
        if ( player1->keypitch == 1 ) { player1->pitch<89 ? player1->pitch += 1 : player1->pitch = 90; }  
        if ( player1->keypitch == 2 ) { player1->pitch>-89 ? player1->pitch -= 1 : player1->pitch = -90; }  
        if ( player1->keyturn == 1 ) { player1->yaw<359 ? player1->yaw += 1 : player1->yaw = 0; }  
        if ( player1->keyturn == 2 ) { player1->yaw>1  ? player1->yaw -= 1 : player1->yaw = 360; }  
		
    tm.update();
    if ( player1->state != CS_SPECTATOR && ca.cameramode() ) { ca.updatecamera(curtime, lastmillis); }

		physicsframe();
		otherplayers(curtime);
		gets2c();
		moveplayer(player1, 10, true);
    as.update();
		if(player1->clientnum>=0) c2sinfo(player1);   // do this last, to reduce the effective frame lag
	}
  
	void spawnplayer(movieent *d)   // place at random spawn. also used by monsters!
	{
		findplayerspawn(d, -1, 0);
		spawnstate(d);
		d->state = cc.spectator ? CS_SPECTATOR : (d==player1 && editmode ? CS_EDITING : CS_ALIVE);
	}
 
	// inputs
	void doattack(bool on) {}
	bool canjump() { return true; }
  
	void deathstate(movieent *d, bool restore = false){}
	
	void killed(movieent *d, movieent *actor) {}
 
	movieent *newclient(int cn)   // ensure valid entity
	{
		if(cn<0 || cn>=MAXCLIENTS)
		{
			neterr("clientnum");
			return NULL;
		}
		while(cn>=players.length()) players.add(NULL);
		if(!players[cn])
		{
			movieent *d = new movieent();
			d->clientnum = cn;
			players[cn] = d;
		}
		return players[cn];
	}

	movieent *getclient(int cn)   // ensure valid entity
	{
		return players.inrange(cn) ? players[cn] : NULL;
	}

	void clientdisconnected(int cn, bool notify = true)
	{
    if(!players.inrange(cn)) return;
		if(following==cn) 
		{
			if(followdir) nextfollow(followdir);
			else stopfollowing();
		}
		movieent *d = players[cn];
		if(!d) return;
		if(notify && d->name[0]) conoutf("player %s disconnected", colorname(d));
    DELETEP(players[cn]);
		cleardynentcache();
	}

	void initclient()
	{
		clientmap[0] = 0;
		cc.initclientnet();
	}
	
  void preload()
  {
    fr.preloadplayermodels();
  }
  
	IVARP(startmenu, 0, 1, 1);
	
	void setwindowcaption()  
	{  
		extern string version;  
		s_sprintfd(capt)("Sandbox Engine %s: %s - %s", version, movieserver::modestr(gamemode), getclientmap()[0] ? getclientmap() : "[new map]");  
		SDL_WM_SetCaption(capt, NULL);  
	}  
	
	void startmap(const char *name)   // called just after a map load
	{
		cc.mapstart();
		if(!m_default) spawnplayer(player1);
		else findplayerspawn(player1, -1);
		s_strcpy(clientmap, name);
		setvar("zoom", -1, true);
		if(*name) conoutf(CON_GAMEINFO, "\f2game mode is %s", movieserver::modestr(gamemode));
		if(*name && openmainmenu && startmenu())
		{
			showgui("main");
			openmainmenu = false;
		}
		setwindowcaption();
		if(identexists("mapstart")) execute("mapstart");
	}
	
	void physicstrigger(physent *d, bool local, int floorlevel, int waterlevel, int material)
	{
		//~ play sounds related to physics (touch ground, touch water ...)
		//~ playsoundc(S_JUMP, (movieent *)d);
	}
	
	void playsoundc(int n, movieent *d = NULL) 
	{ 
		if(!d || d==player1)
		{
			cc.addmsg(SV_SOUND, "i", n); 
			playsound(n); 
		}
		else playsound(n, &d->o);
	}
	
	int numdynents() { return 1+players.length(); } //+ms.monsters.length()+mo.movables.length(); }
	
	dynent *iterdynents(int i)
	{
		if(!i) return player1;
		i--;
		if(i<players.length()) return players[i];
		i -= players.length();
		//~ if(i<mo.movables.length()) return mo.movables[i];
		return NULL;
	}
	
	bool duplicatename(movieent *d, char *name = NULL)
	{
		if(!name) name = d->name;
		if(d!=player1 && !strcmp(name, player1->name)) return true;
		loopv(players) if(players[i] && d!=players[i] && !strcmp(name, players[i]->name)) return true;
		return false;
	}
	
	char *colorname(movieent *d, char *name = NULL, const char *prefix = "")
	{
		if(!name) name = d->name;
		if(name[0] && !duplicatename(d, name)) return name;
		static string cname;
		s_sprintf(cname)("%s%s \fs\f5(%d)\fr", prefix, name, d->clientnum);
		return cname;
	}
	
	void suicide(physent *d) {}
	
	void drawhudgun() {}
  
	void gameplayhud(int w, int h) 
  {
		if (tm.status == TIMER_RUNNING)
		{
			glLoadIdentity();
			glOrtho(0, w, h, 0, -1, 1);
			int sec = tm.getcurtime()/1000;
			int millis = tm.getcurtime()-sec;
			draw_textf("timer: %d.%d",  10, h-100, sec, millis);
		}
	}
	
	const char *defaultcrosshair(int index)
	{
		return "data/crosshair";
	}
	
	void newmap(int size)
	{
		cc.addmsg(SV_NEWMAP, "ri", size);
	}
	
	void edittrigger(const selinfo &sel, int op, int arg1, int arg2, int arg3)
	{
		if(gamemode==0) switch(op)
		{
			case EDIT_FLIP:
			case EDIT_COPY:
			case EDIT_PASTE:
			case EDIT_DELCUBE:
			{
				cc.addmsg(SV_EDITF + op, "ri9i4",
									sel.o.x, sel.o.y, sel.o.z, sel.s.x, sel.s.y, sel.s.z, sel.grid, sel.orient,
									sel.cx, sel.cxs, sel.cy, sel.cys, sel.corner);
				break;
			}
			case EDIT_MAT:
			case EDIT_ROTATE:
			{
				cc.addmsg(SV_EDITF + op, "ri9i5",
									sel.o.x, sel.o.y, sel.o.z, sel.s.x, sel.s.y, sel.s.z, sel.grid, sel.orient,
									sel.cx, sel.cxs, sel.cy, sel.cys, sel.corner,
									arg1);
				break;
			}
			case EDIT_FACE:
			case EDIT_TEX:
			case EDIT_REPLACE:
			{
				cc.addmsg(SV_EDITF + op, "ri9i6",
									sel.o.x, sel.o.y, sel.o.z, sel.s.x, sel.s.y, sel.s.z, sel.grid, sel.orient,
									sel.cx, sel.cxs, sel.cy, sel.cys, sel.corner,
									arg1, arg2);
				break;
			}
			case EDIT_REMIP:
			{
				cc.addmsg(SV_EDITF + op, "r");
				break;
			}
		}
	}
	
	bool serverinfostartcolumn(g3d_gui *g, int i)
	{
		static const char *names[] = { "ping ", "players ", "map ", "mode ", "master ", "host ", "description " };
		if(size_t(i) >= sizeof(names)/sizeof(names[0])) return false;
		g->pushlist();
		g->text(names[i], 0xFFFF80, !i ? "server" : NULL);
		g->mergehits(true);
		return true;
	}
	
	void serverinfoendcolumn(g3d_gui *g, int i)
	{
		g->mergehits(false);
		g->poplist();
	}
	
	bool serverinfoentry(g3d_gui *g, int i, const char *name, const char *sdesc, const char *map, int ping, const vector<int> &attr, int np)
	{
		if(ping < 0 || attr.empty() || attr[0]!=PROTOCOL_VERSION)
		{
			switch(i)
			{
				case 0:
					if(g->button(" ", 0xFFFFDD, "server")&G3D_UP) return true;
					break;
					
				case 1:
				case 2:
				case 3:
				case 4:
					if(g->button(" ", 0xFFFFDD)&G3D_UP) return true; 
					break;
					
				case 5:
					if(g->buttonf("%s ", 0xFFFFDD, NULL, name)&G3D_UP) return true;
					break;
					
				case 6:
					if(ping < 0)
				{
					if(g->button(sdesc, 0xFFFFDD)&G3D_UP) return true;
				}
					else if(g->buttonf("[%s protocol] ", 0xFFFFDD, NULL, attr.empty() ? "unknown" : (attr[0] < PROTOCOL_VERSION ? "older" : "newer"))&G3D_UP) return true;
					break;
			}       
			return false;
		}
		
		switch(i)
		{
			case 0:
				if(g->buttonf("%d ", 0xFFFFDD, "server", ping)&G3D_UP) return true;
				break;
				
			case 1:
				if(attr.length()>=4)
			{
				if(g->buttonf("%d/%d ", 0xFFFFDD, NULL, np, attr[3])&G3D_UP) return true;
			}
				else if(g->buttonf("%d ", 0xFFFFDD, NULL, np)&G3D_UP) return true;
				break;
				
			case 2:
				if(g->buttonf("%.25s ", 0xFFFFDD, NULL, map)&G3D_UP) return true;
				break;
				
			case 3:
				if(g->buttonf("%s ", 0xFFFFDD, NULL, attr.length()>=2 ? movieserver::modestr(attr[1], "") : "")&G3D_UP) return true;
				break;
				
			case 4:
				if(g->buttonf("%s ", 0xFFFFDD, NULL, attr.length()>=5 ? movieserver::mastermodestr(attr[4], "") : "")&G3D_UP) return true;
				break;
			
			case 5:
				if(g->buttonf("%s ", 0xFFFFDD, NULL, name)&G3D_UP) return true;
				break;
			
			case 6:
			{
				if(g->buttonf("%.25s", 0xFFFFDD, NULL, sdesc)&G3D_UP) return true;
				break;
			}
		}
		return false;
	}

	void g3d_gamemenus() {}

	// any data written into this vector will get saved with the map data. Must take care to do own versioning, and endianess if applicable. Will not get called when loading maps from other games, so provide defaults.
	//ToDo map version check
	void writegamedata(vector<char> &extras) {}
	void readgamedata(vector<char> &extras) {}
  
	const char *gameident() { return "movie"; }
	const char *defaultmap() { return "mansion"; }
	const char *defaultconfig() { return "data/defaults.cfg"; }
	const char *autoexec() { return "autoexec.cfg"; }
	const char *savedservers() { return "servers.cfg"; }
	const char *loadimage() { return "data/sandbox_movielogo"; }
	};

	REGISTERGAME(moviegame, "movie", new movieclient(), new movieserver());
	
#else
	
	REGISTERGAME(moviegame, "movie", NULL, new movieserver());

#endif
