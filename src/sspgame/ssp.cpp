#ifndef STANDALONE

#include "pch.h"
#include "cube.h"
#include "iengine.h"
#include "igame.h"
#include "sspgame.h"
#include "sspstubs.h"

struct sspclient : igameclient
{
	#include "ssprender.h"
	#include "sspent.h"
	#include "sspmonster.h"
	#include "sspweapons.h"
		
	int  maptime, secsremain, secsallowed, cameraent, lastcameraent, cameramillis;
	// time started / time left / time allowed / camera / last camera / camera assignment time
	bool intermission;
	string clientmap;
	
	sspent *player1;
	sspent lastplayerstate;

	sspentities et;
	sspclientcom cc;
	sspmonster sm;
	ssprender sr;
	sspweapons wp;
	
	//items
	vector<coinpickup *> coinpickups;
	vector<weaponpickup *> weaponpickups;
	vector<armourpickup *> armourpickups;
	vector<healthpickup *> healthpickups;
	vector<timepickup *> timepickups;
	vector<lifepickup *> lifepickups;
	
	vector<eventimage *> eventimages;
	
	sspclient() : maptime(0), secsremain(0),
		intermission(false), player1(spawnstate(new sspent())), et(*this), sm(*this), sr(*this), wp(*this)
		{
			coinpickups.setsizenodelete(0); weaponpickups.setsizenodelete(0); armourpickups.setsizenodelete(0);
			healthpickups.setsizenodelete(0); timepickups.setsizenodelete(0); lifepickups.setsizenodelete(0);
			
			CCOMMAND(map, "s", (sspclient *self, char *s), load_world(s));
			
			CCOMMAND(addcoin, "si", (sspclient *self, char *s, int *x), self->coinpickups.add(new coinpickup (s, *x)));
			CCOMMAND(addweapon, "ssii", (sspclient *self, char *s, char *t, int *x, int *y), self->weaponpickups.add(new weaponpickup(s, t, *x, *y)));
			CCOMMAND(addarmour, "ssi", (sspclient *self, char *s, char *t, int *x), self->armourpickups.add(new armourpickup(s, t, *x)));
			CCOMMAND(addhealth, "si", (sspclient *self, char *s, int *x), self->healthpickups.add (new healthpickup(s, *x)));
			CCOMMAND(addtime, "si", (sspclient *self, char *s, int *x), self->timepickups.add(new timepickup(s, *x)));
			CCOMMAND(addlife, "si", (sspclient *self, char *s, int *x), self->lifepickups.add(new lifepickup(s, *x)));
			
			CCOMMAND(eventimage, "si", (sspclient *self, char *s, int *x), self->eventimages.add(new eventimage(s, *x)));
					
			CCOMMAND(moveright, "D", (sspclient *self, int *down), { self->moveright(*down); });
			CCOMMAND(moveleft, "D", (sspclient *self, int *down), { self->moveleft(*down); });
			
			CCOMMAND(setcamera, "i", (sspclient *self, int *x), { self->setcamera(*x);});
			CCOMMAND(switchaxis, "", (sspclient *self), self->switchaxis());
			CCOMMAND(getcamera, "", (sspclient *self), self->getcamera(););
			CCOMMAND(getyaw, "", (sspclient *self), s_sprintfd(s)("%f", self->player1->yaw); result(s););
		}
		
	iclientcom      *getcom()  { return &cc; }
	icliententities *getents() { return &et; }
	
	
	//movement
	void switchaxis() //flip the rotation if a player is close to an axis entity, try to make it automatic eventually
	{
		loopv(et.ents)
		{
			extentity &e = *et.ents[i];
			if(e.type !=AXIS) continue;
			vec pos1 = e.o;
			vec pos2 = player1->o;
			pos1.z = pos2.z = 0;

			if(pos1.dist(pos2) <= (e.attr4 ? abs(e.attr4) : 16) && abs(player1->o.z-e.o.z) <= (e.attr5 ? abs(e.attr5) : 64))
			{
				vec pos = player1->o;
				if((int) player1->yaw % 180 == e.attr1 % 180)
					player1->yaw = e.attr2;
				else
					player1->yaw = e.attr1;
				
				player1->newpos = e.o;
				player1->vel = vec(0, 0, player1->vel.z);
				player1->newpos.z = pos.z;
				
				s_sprintfd(id)("axis_script_%i", e.attr3); //ie, to change cameras
				if(identexists(id)) execute(id);
				return;
			}
		}
	}
	
	void changeyaw(int val1, int val2) //done seperately to avoid undoing yaw changes
	{
		extern physent *camera1;
		int yaw = closestyaw(camera1);
		int playeryaw = player1->yaw;
		if(playeryaw != (yaw+val1) % 360 && playeryaw != (yaw+val2) % 360)
		{
			player1->yaw = (playeryaw+180) % 360; //test failed, turn around
		}
	}
	
	void moveright(int down) //if cam.yaw == 0, then right = 0 && 90
	{
		changeyaw(0, 90);
		player1->k_up = down!=0;
		player1->move = player1->k_up ? 1 : (player1->k_down ? -1 : 0);
	}
	
	void moveleft(int down)
	{
		changeyaw(180, 270);
		player1->k_down = down!=0;
		player1->move = player1->k_down ? 1 : (player1->k_up ? -1 : 0);
	}
	
	char *getclientmap() { return clientmap; }
	void resetgamestate() {}
	
	void adddynlights() { wp.dynlights(); }
	
	void takedamage(sspent *d, int damage, int invuln)
	{
		if(d->lastpain + invuln > lastmillis) return;
		
		if(d->armour)
			d->armour = ARM_NONE;		
		else
			d->health -= damage;
			
		d->lastpain = lastmillis;
	}
	
	void suicide(physent *d)
	{
		if(d==player1 && player1->state==CS_ALIVE)
		{
			//to ponder... should the player shoot into the air, or just scream owie for a bit
			takedamage(player1, 1, 2000); //just give the player a booboo, don't kill him :P
		}
		else if(d!=player1 && d->state==CS_ALIVE) //assume it's a monster
			d->state=CS_DEAD;
	}
	
	sspent *spawnstate(sspent *d)              // reset player state not persistent accross spawns
	{
		d->respawn();
		return d;
   	}
   	
	void respawnself()
	{
		spawnplayer(player1);
	}
    
	int getgamemode() {
		return 0; //stub
	}
	
	void setwindowcaption()
	{
		extern string version;
		s_sprintfd(capt)("Sandbox Engine %s: Side Scrolling Platformer - %s", version, getclientmap()[0] ? getclientmap() : "[new map]");
		SDL_WM_SetCaption(capt, NULL);
	}
	
	void fixplayeryaw()
	{
		//constrains player's yaw to 90 degree angles, round up
		player1->yaw = closestyaw(player1);
	}
	
	int closestyaw(physent *d)
	{
		int yaw = d->yaw; //ignore decimals
		if(yaw>= 45 && yaw <= 134)
			return 90;
			
		else if(yaw >= 135 && yaw <= 224)
			return 180;
			
		else if(yaw>=225 && yaw<=314)
			return 270;
			
		else
			return 0;
	}
	
	void getcamera()
	{
		s_sprintfd(s)("%i", cameraent<0 ? -1 : et.ents[cameraent]->attr1);
		result(s);
	}
	
	//set cameraent to the valid ent if it exists, otherwise, just ignore the request
	void setcamera(int attr1)
	{
		loopv(et.ents)
		{
			extentity &e = *et.ents[i];
			if(e.type == CAMERA && e.attr1 == attr1)
			{
				lastcameraent = cameraent;
				cameramillis = lastmillis;
				cameraent = i;
				return;
			}
		}
	}
	
	void startmap(const char *name)   // called just after a map load
	{
		secsallowed = 300;
		
		player1->checkpoint = -1;
		cameraent = lastcameraent = -1;
		
		setcamera(0); //set the camera if it exists
		
        	s_strcpy(clientmap, name);
        	setwindowcaption();
		intermission = false;
		maptime = cameramillis = lastmillis;
		et.prepareents();
		sm.initialise();
		findplayerspawn(player1, -1);
		player1->lastpain = lastmillis; //for spawning invulnerability
        	conoutf(CON_GAMEINFO, "\f2D to move right, A to move left, W to jump, and click to attack, have fun!");
        	
        	if(identexists("mapstart")) execute("mapstart");
	}
	
	void spawnplayer(sspent *d)   // place at random spawn. also used by monsters!
	{
		if (d==player1)
		{
			if (et.ents.inrange(d->checkpoint)) d->yaw = et.ents[d->checkpoint]->attr1;
			findplayerspawn(d, d->checkpoint>=0 ? d->checkpoint : -1, 0);
		}
		else
			findplayerspawn(d, -1, 0);
		spawnstate(d);
		d->state = d==player1 && editmode ? CS_EDITING : CS_ALIVE;
		d->lastpain = lastmillis;
	}
	
	void respawn()
	{
		if(player1->state==CS_DEAD)
		{
			player1->attacking = false;
			if(player1->lives > 0)
			{
				respawnself();
				player1->lives--;
			}
			else
			{
				player1->lives = 6; //i-1
				cc.changemap(getclientmap());
			}
		}
	}
	
	void doattack(bool on)
	{
		if(intermission) return;
		if((player1->attacking = on)) respawn();
	}
	
	bool canjump()
	{
		if(!intermission) respawn();
		return player1->state!=CS_DEAD && !intermission;
	}
	
	bool allowdoublejump()
	{
		if(player1->armour==ARM_FLY && lastmillis - player1->lastjump > 600) return true;
		return false;
	}
	
	bool detachcamera()
	{
		return !editmode; //we want the camera detached if the player isn't editing;
   	}
   	
	void updatecamera()
	{
		if (editmode) return;
		extern physent *camera1;
		physent &newcam = *camera1;
			
		float dist = 100, pitch = 0, yaw = 0;
		if(et.ents.inrange(cameraent))
		{
			if(cameramillis + 750 > lastmillis)
			{
				//basic interpolation, simply a transition between two cameras, values above 360 and below 0 are allowed for yaw
				float mult = (lastmillis - cameramillis) / 750.0f, deltayaw, deltapitch, deltadist;
				
				if(et.ents.inrange(lastcameraent))
				{ 
					deltayaw = et.ents[lastcameraent]->attr2 - et.ents[cameraent]->attr2;
					deltapitch = et.ents[lastcameraent]->attr3 - et.ents[cameraent]->attr3;
					deltadist = et.ents[lastcameraent]->attr4 - et.ents[cameraent]->attr4;
					
					yaw = et.ents[lastcameraent]->attr2;
					pitch = et.ents[lastcameraent]->attr3;
					dist = et.ents[lastcameraent]->attr4;
				}
				else
				{
					deltayaw = 0 - et.ents[cameraent]->attr2;
					deltapitch = 0 - et.ents[cameraent]->attr3;
					deltadist = 100 - et.ents[cameraent]->attr4;
				}
				
				yaw -= deltayaw * mult;
				pitch -= deltapitch * mult;
				dist -= deltadist * mult;
			}
			else
			{
				yaw = et.ents[cameraent]->attr2;
				pitch = et.ents[cameraent]->attr3;
				dist = et.ents[cameraent]->attr4;
			}
		}
		
		newcam.yaw = yaw;
	   	newcam.pitch = (90 < pitch ? 90 : (-90 > pitch ? -90 : pitch));
		loopi(10) moveplayer(&newcam, 10, true, dist);	
	}
	
	void updateworld()        // main game update loop
	{
		if(editmode)
		{
			secsremain = 300;
			intermission = false;
			maptime = lastmillis;
		}
		else
		{
			player1->pitch = 0;
			setvar("zoom", -1, true);
			setvar("fov", 100, true); //no zooming :D
			fixplayeryaw();
		}
			
		if (!intermission) secsremain = secsallowed - (lastmillis - maptime)/1000;
		if (secsremain <= 0) intermission = true;

		if(!curtime) return;

		physicsframe();
		sm.update();
		wp.updateprojs();
		
		if(player1->state==CS_DEAD)
		{
			if(player1->ragdoll) moveragdoll(player1);
			else
			{
				player1->move = player1->strafe = 0;
				moveplayer(player1, 10, false);
			}
			
		}
		else if(!intermission)
		{
			et.checkitems(player1);
			moveplayer(player1, 10, true);
			
			//other monitoring stuff
			if(player1->health<=0 && player1->state==CS_ALIVE)
			{
				player1->state = CS_DEAD;
				eventimages.add(new eventimage("data/hirato/ssp/died", 2000));
			}
			for(; player1->coins>= 100 && player1->lives < 99;) //change every 100 coins into a life, assuming the amount is below 99, yay!
			{
				player1->lives++;
				player1->coins -= 100;
			}
		}

		if(intermission)
		{
			wp.clearprojs();
		}
	}
	
	
	void quad(int x, int y, int xs, int ys)
	{
		glBegin(GL_QUADS);
		glTexCoord2f(0, 0); glVertex2i(x,    y);
		glTexCoord2f(1, 0); glVertex2i(x+xs, y);
		glTexCoord2f(1, 1); glVertex2i(x+xs, y+ys);
		glTexCoord2f(0, 1); glVertex2i(x,    y+ys);
		glEnd();
	}
	
	void gameplayhud(int w, int h)
	{
		glLoadIdentity();
		glOrtho(0, w, h, 0, -1, 1);
		
		
		int width = (w - 512) / 2, height = (h-512) / 2;
		loopv(eventimages)
		{
			eventimage &img = *eventimages[i];
			
			if(img.deltime < lastmillis) eventimages.remove(i);
			
			settexture(img.tex, 3);
			if(img.deltime - lastmillis < 500)
				glColor4f(1, 1, 1, (img.deltime - lastmillis) / 500.0);
			else if(lastmillis - img.starttime < 200)
				glColor4f(1, 1, 1, (lastmillis - img.starttime) / 200.0);
			else
				glColor4f(1, 1, 1, 1);
			
			quad(width, height, 512, 512);
			
			settexture("data/hud/2.3/guioverlay", 3);
			quad(width, height, 512, 512);
			
		}

		width = max(w, h)/32;
		draw_textf("%d", 4*width , h-3*width, player1->lives);
		draw_textf("%d", 16*width, h-1.5*width, secsremain);
		draw_textf("%d", 27*width, h-1.5*width, player1->coins);
		
		settexture("data/hud/2.3/icons/rc", 3); //character
		
		if(lastmillis < player1->lastpain + 2000)
			glColor4f(1, .5, .5, 1); //set colours, he obviously had a recent owie
		else
			glColor4f(1, 1, 1, 1);
			
		quad(.5 * width, h - 3.5 * width, 3*width, 3*width);
		
		settexture("data/hirato/hp", 3); //HP, try a cube or something
		loopi(player1->health)
		{
			glColor4f(1- (0.5f * i), 0.5f*i, 0, 1);
			quad((4 + 1.25 * i) * width, h - 1.5*width, width, width);
		}
		
		settexture("data/hirato/time", 3); //clock
		glColor4f(1, 1, 1, 1);
		quad(13*width, h- 2.5*width, 2*width, 2*width);
		
		settexture("data/hirato/coin", 3); //bling bling!
		glColor4f(1, 1, 1, 1);
		quad(24*width, h-2.5*width, 2*width, 2*width);
		
		settexture("data/hirato/shield", 3); //are you protected? :P
		switch(player1->armour)
		{
			case ARM_PLAIN:
				glColor4f(.75, 1, .5, 1);
				break;
			case ARM_ATTRACT:
				glColor4f(0., .5, 1, 1);
				break;
			case ARM_FLY:
				glColor4f(1, 1, 1, 1);
				break;
			case ARM_SPIKE:
				glColor4f(0.75, 0.75, 0.75, 1);
				break;
			default:
				glColor4f(1, 1, 1, .25);
				break;
		}
		quad(0.5 * width, h - 6 * width, 2*width, 2*width);
	}
	
	const char *defaultcrosshair(int index)
	{
		//memo to self, make 1^2 px crosshair
		return editmode ? "data/edit" : "data/items";
	}
	
	int selectcrosshair(float &r, float &g, float &b)
	{
		r = b = 0.5;
		g = 1.0f;
		return editmode;
	}

	void initclient()
	{
		clientmap[0] = 0;
	}
	
	void newmap(int size) {}
	
	void physicstrigger(physent *d, bool local, int floorlevel, int waterlevel, int material)
	{
		if(d->state!=CS_ALIVE) return;
		switch(material)
		{
			case MAT_LAVA:
				if (waterlevel==0) break;
				playsound(S_BURN, d==player1 ? NULL : &d->o);
				particle_splash(PART_FIREBALL1, 200, 500, d->o, 0xFF4F00, 8);
				break;
			case MAT_WATER:
				if (waterlevel==0) break;
				playsound(waterlevel > 0 ? S_SPLASH1 : S_SPLASH2 , d==player1 ? NULL : &d->o);
				uchar col[3]; getwatercolour(col);
				particle_splash(PART_WATER, 200, 200, d->o, (col[0]<<16) | (col[1]<<8) | col[2], 0.5); 
				break;
			default:
				if (floorlevel==0) break;
				playsound(floorlevel > 0 ? S_JUMP : S_LAND, local ? NULL : &d->o);
				break;
		}
	}
	
	int numdynents() { return 1+sm.monsters.length(); }

	dynent *iterdynents(int i)
	{
		if(!i) return player1;
		i--;
		if(i<sm.monsters.length()) return sm.monsters[i];
		/*if(i<ms.monsters.length()) return ms.monsters[i];
		i -= ms.monsters.length();
		*/
		return NULL;
	}
	
	void g3d_gamemenus() {}
	
	void rendergame(bool mainpass)
	{
		sr.rendergame(mainpass);
	}
	
	void edittrigger(const selinfo &sel, int op, int arg1 = 0, int arg2 = 0, int arg3 = 0) {}
	
	void writegamedata(vector<char> &extras) {}
	void readgamedata(vector<char> &extras) {}
	
	const char *gameident() { return "ssp"; }
	const char *defaultmap() { return "ssptest"; }
	const char *defaultconfig() { return "data/defaults.cfg"; }
	const char *autoexec() { return "autoexec.cfg"; }
	const char *savedservers() { return ""; } //stub
	const char *loadimage() { return "data/sandbox_ssp_logo"; }
};

REGISTERGAME(sspgame, "ssp", new sspclient(), new sspclientserver());
#endif

