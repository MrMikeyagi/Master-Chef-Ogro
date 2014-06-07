
#include "pch.h"
#include "cube.h"
#include "iengine.h"
#include "igame.h"
#include "stubs.h"


struct rpgclient : igameclient, g3d_callback
{
    struct rpgent;

    #include "rpgeffect.h"
    #include "entities.h"
    #include "stats.h"
    #include "rpgobj.h"
    #include "rpgobjset.h"
    #include "rpgent.h"
    IVARP(hudgun, 0, 1, 1);

	int nextmode, gamemode;

    rpgentities et;
    rpgdummycom cc;
    rpgobjset os;
    rpgfx fx;

    rpgent player1;

    int maptime;
    string mapname;

    int menutime, menutab, menuwhich;
    vec menupos;

    rpgclient() : et(*this), os(*this), fx(*this), player1(os.playerobj, *this, vec(0, 0, 0), 0, 100, ENT_PLAYER), maptime(0), menutime(0), menutab(1), menuwhich(0)
    {
        CCOMMAND(map, "s", (rpgclient *self, char *s), load_world(s));
        CCOMMAND(showplayergui, "i", (rpgclient *self, int *which), self->showplayergui(*which));
    }
    ~rpgclient() {}

    icliententities *getents() { return &et; }
    iclientcom *getcom() { return &cc; }

    void updateworld()
    {
        if(!maptime) { maptime = lastmillis + curtime; return; }

        if(!curtime) return;
        physicsframe();
        os.update();
	fx.updatefx();
        player1.updateplayer(worldpos);
        checktriggers();
    }

		int getgamemode() {
		return 0;
	}

    void showplayergui(int which)
    {
        if((menutime && which==menuwhich) || !which)
        {
            menutime = 0;
        }
        else
        {
            menutime = starttime();
            menupos  = menuinfrontofplayer();
            menuwhich = which;
        }
    }

    void gui(g3d_gui &g, bool firstpass)
    {
        g.start(menutime, 0.03f, &menutab);
        switch(menuwhich)
        {
            default:
            case 1:
                g.tab("inventory", 0xFFFFF);
                os.playerobj->invgui(g);
                break;

            case 2:
                g.tab("stats", 0xFFFFF);
                os.playerobj->st_show(g);
                break;

            case 3:
                g.tab("active quests", 0xFFFFF);
                os.listquests(false, g);
                g.tab("completed quests", 0xFFFFF);
                os.listquests(true, g);
        }
        g.end();
    }

    void initclient() {}

	void physicstrigger(physent *d, bool local, int floorlevel, int waterlevel, int material)
	{
		switch(material)
		{
			case MAT_LAVA:
				if (waterlevel==0) break;
				playsound(S_BURN, d==&player1 ? NULL : &d->o);
				particle_splash(PART_FIREBALL1, 100, 500, d->o, 0xAF7F00, 4);
				break;
			case MAT_WATER:
				if (waterlevel==0) break;
				playsound(waterlevel > 0 ? S_SPLASH1 : S_SPLASH2 , d==&player1 ? NULL : &d->o);
				particle_splash(PART_WATER, 100, 200, d->o, 0x00AFFF, 0.5);
				break;
			default:
				if (floorlevel==0) break;
				playsound(floorlevel > 0 ? S_JUMP : S_LAND, local ? NULL : &d->o);
				break;
		}
	}

    void edittrigger(const selinfo &sel, int op, int arg1 = 0, int arg2 = 0, int arg3 = 0) {}
    char *getclientmap() { return mapname; }
    void resetgamestate() {}
    void suicide(physent *d)
    {
    	((rpgent *)d)->state = CS_DEAD;
	((rpgent *)d)->lastpain = ((rpgent *)d)->lastaction = lastmillis;
	((rpgent *)d)->vel = ((rpgent *)d)->falling = vec(0, 0, 0);
	
	if(d==&player1)
	{
		conoutf("\f2Using produce magic you whisk yourself back to a regeneration center!");
		particle_splash(PART_EDIT, 1000, 500, player1.o, 0x007FFF);
	}
    }
    void newmap(int size) {}

    void setwindowcaption()
    {
		extern string version;
		s_sprintfd(capt)("Sandbox Engine %s: Master Chef Ogro - %s", version, getclientmap()[0] ? getclientmap() : "[new map]");
		SDL_WM_SetCaption(capt, NULL);
    }

    void startmap(const char *name)
    {
		player1.checkpoint = -1;
        os.clearworld();
        s_strcpy(mapname, name);
        maptime = 0;
        findplayerspawn(&player1);
        if(*name) os.playerobj->st_init();
        et.startmap();
        setwindowcaption();
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
        glOrtho(0, w*2, h*2, 0, -1, 1);
        draw_textf("using: %s", 636*2, h*2-256+149, os.selected ? os.selected->name : "(none)");       // temp

        glLoadIdentity();
        glOrtho(0, w, h, 0, -1, 1);
        settexture("packages/hud/hud_rpg.png", true);

        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        quad(0, h-128, 768, 128);
        settexture("packages/hud/hbar.png", true);
        glColor4f(1, 0, 0, 0.5f);
        quad(130, h-128+57, 193*os.playerobj->s_hp/os.playerobj->eff_maxhp(), 17);
        glColor4f(0, 0, 1, 0.5f);
        quad(130, h-128+87, 193*os.playerobj->s_mana/os.playerobj->eff_maxmana(), 17);
    }

    void drawhudmodel(int anim, float speed = 0, int base = 0)
    {
        rendermodel(NULL, "hudguns/fist", anim, player1.o, player1.yaw+90, player1.pitch, MDL_LIGHT, NULL, NULL, base, speed);
    }

    void drawhudgun()
    {
        if(editmode) return;

        int rtime = 250;
        if(lastmillis-player1.lastaction<rtime)
        {
            drawhudmodel(ANIM_GUN_SHOOT, rtime/17.0f, player1.lastaction);
        }
        else
        {
            drawhudmodel(ANIM_GUN_IDLE|ANIM_LOOP);
        }
    }

    bool canjump() { return true; }
    void doattack(bool on) { player1.attacking = on; }
    dynent *iterdynents(int i) { return i ? os.set[i-1]->ent : &player1; }
    int numdynents() { return os.set.length()+1; }

    void rendergame(bool mainpass)
    {
        if(isthirdperson() && player1.state != CS_DEAD) renderclient(&player1, "ogre/blue", NULL, ANIM_HOLD1|ANIM_LOOP, ANIM_ATTACK7, 300, player1.lastaction, player1.lastpain);
        os.render();
        et.renderentities();
    }

    void g3d_gamemenus() { os.g3d_npcmenus(); if(menutime) g3d_addgui(this, menupos, GUI_2D); }

    void writegamedata(vector<char> &extras) {}
    void readgamedata (vector<char> &extras) {}

    void doanimation(bool on){} //dummy

    const char *gameident()     { return "rpg"; }
    const char *defaultmap()    { return "Ograria_Castle"; }
    const char *defaultconfig() { return "data/defaults.cfg"; }
    const char *autoexec()      { return "rpg_autoexec.cfg"; }
    const char *savedservers() { return "servers.cfg"; }
	const char *server() { return "server.cfg"; } //caledit adding server.cfg
    const char *loadimage() { return "data/sandbox_logo_rpg"; }
};

#define N(n) int rpgclient::stats::pointscale_##n, rpgclient::stats::percentscale_##n;
RPGSTATNAMES
#undef N

REGISTERGAME(rpggame, "rpg", new rpgclient(), new rpgdummyserver());

