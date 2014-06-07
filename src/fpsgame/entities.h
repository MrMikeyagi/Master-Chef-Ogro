
struct entities : icliententities
{
    fpsclient &cl;
    
    vector<extentity *> ents;
    
    IVARP(showlighting, 0, 1, 1); 
    IVARP(showdynlights, 0 ,1, 1); 

    entities(fpsclient &_cl) : cl(_cl)
    {
    }

    vector<extentity *> &getents() { return ents; }
    
    const char *itemname(int i)
    {
        int t = ents[i]->type;
        if(t<I_SHELLS || t>I_QUAD) return NULL;
        return itemstats[t-I_SHELLS].name;
    }
   
    const char *entmdlname(int type)
    {
        static const char *entmdlnames[] =
        {
            NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
            "ammo/shells", "ammo/bullets", "ammo/rockets", "ammo/rrounds", "ammo/grenades", "ammo/cartridges",
            "health", "boost", "armor/green", "armor/yellow", "quad", "teleporter",
            NULL, NULL,
            "carrot",
            NULL, NULL,
            "checkpoint",
            NULL, NULL,
            NULL, NULL,
            NULL, NULL,
        };
        return entmdlnames[type];
    }

    void preloadentities()
    {
        loopi(MAXENTTYPES)
        {
            const char *mdl = entmdlname(i);
            if(!mdl) continue;
            loadmodel(mdl, -1, true);
        }
    }

    void renderent(extentity &e, const char *mdlname, float z, float yaw)
    {
        if(!mdlname) return;
        rendermodel(&e.light, mdlname, ANIM_MAPMODEL|ANIM_LOOP, vec(e.o).add(vec(0, 0, z)), yaw, 0, MDL_SHADOW | MDL_CULL_VFC | MDL_CULL_DIST | MDL_CULL_OCCLUDED);
    }

    void renderent(extentity &e, int type, float z, float yaw)
    {
        renderent(e, entmdlname(type), z, yaw);
    }

    void renderentities()
    {
        loopv(ents)
        {
            extentity &e = *ents[i];
            if(e.type==CARROT || e.type==RESPAWNPOINT)
            {
                renderent(e, e.type, (float)(1+sin(lastmillis/100.0+e.o.x+e.o.y)/20), lastmillis/(e.attr2 ? 1.0f : 10.0f));
                continue;
            }
            if(e.type==TELEPORT)
            {
                if(e.attr2 < 0) continue;
                if(e.attr2 > 0)
                {
                    renderent(e, mapmodelname(e.attr2), (float)(1+sin(lastmillis/100.0+e.o.x+e.o.y)/20), lastmillis/10.0f);        
                    continue;
                }
            }
            else
            {
                if(!e.spawned) continue;
                if(e.type<I_SHELLS || e.type>I_QUAD) continue;
            }
            renderent(e, e.type, (float)(1+sin(lastmillis/100.0+e.o.x+e.o.y)/20), lastmillis/10.0f);
        }
    }
    
    #define renderfocus(i,f) { extentity &e = *ents[i]; f; } 
     
    void renderentlight(extentity &e) 
    { 
        vec color(e.attr2, e.attr3, e.attr4); 
        color.div(255.f); 
        adddynlight(vec(e.o), float(e.attr1 != 0 ? e.attr1 > 0 ? e.attr1 : 0 : getworldsize() ), color); 
    } 
     
    bool dynlightsactive[32767]; 
     
    void adddynlights() 
    { 
        loopv(ents) 
        { 
            if((ents[i]->attr5 > -1 && ents[i]->type == DYNLIGHT && dynlightsactive[ents[i]->attr5] && (!editmode || showdynlights())) //dynlights 
                || ((entgroup.find(i) >= 0 || enthover == i) &&    (ents[i]->type == LIGHT) && (editmode && showlighting()))) //normal lights 
            { 
                renderfocus(i, renderentlight(e)); 
            } 
        } 
    } 

    void rumble(const extentity &e)
    {
        playsound(S_RUMBLE, &e.o);
    }

    void trigger(extentity &e)
    {
        switch(e.attr3)
        {
            case 29:
                cl.ms.endsp(false);
                break;
        }
    }

    void addammo(int type, int &v, bool local = true)
    {
        itemstat &is = itemstats[type-I_SHELLS];
        v += is.add;
        if(v>is.max) v = is.max;
        if(local) cl.playsoundc(is.sound);
    }

    void repammo(fpsent *d, int type)
    {
        addammo(type, d->ammo[type-I_SHELLS+GUN_SG]);
    }

    // these two functions are called when the server acknowledges that you really
    // picked up the item (in multiplayer someone may grab it before you).

    void pickupeffects(int n, fpsent *d)
    {
        if(!ents.inrange(n)) return;
        int type = ents[n]->type;
        if(type<I_SHELLS || type>I_QUAD) return;
        ents[n]->spawned = false;
        if(!d) return;
        itemstat &is = itemstats[type-I_SHELLS];
        if(d!=cl.player1 || isthirdperson()) particle_text(d->abovehead(), is.name, PART_TEXT_RISE, 2000, 0xFFC864, 4.0f);
        playsound(itemstats[type-I_SHELLS].sound, d!=cl.player1 ? &d->o : NULL); 
        d->pickup(type);
        if(d==cl.player1) switch(type)
        {
            case I_BOOST:
                conoutf(CON_GAMEINFO, "\f2you have a permanent +10 health bonus! (%d)", d->maxhealth);
                playsound(S_V_BOOST);
                break;

            case I_QUAD:
                conoutf(CON_GAMEINFO, "\f2you got the quad!");
                playsound(S_V_QUAD);
                break;
        }
    }

    // these functions are called when the client touches the item

    void teleport(int n, fpsent *d)     // also used by monsters
    {
        int e = -1, tag = ents[n]->attr1, beenhere = -1;
        for(;;)
        {
            e = findentity(TELEDEST, e+1);
            if(e==beenhere || e<0) { conoutf(CON_WARN, "no teleport destination for tag %d", tag); return; }
            if(beenhere<0) beenhere = e;
            if(ents[e]->attr2==tag)
            {
                d->o = ents[e]->o;
                d->yaw = ents[e]->attr1;
                d->pitch = 0;
                d->vel = vec(0, 0, 0);//vec(cosf(RAD*(d->yaw-90)), sinf(RAD*(d->yaw-90)), 0);
                entinmap(d);
                updatedynentcache(d);
                cl.playsoundc(S_TELEPORT, d);
                break;
            }
        }
    }

    void trypickup(int n, fpsent *d)
    {
        switch(ents[n]->type)
        {
            default:
                if(d->canpickup(ents[n]->type))
                {
                    cl.cc.addmsg(SV_ITEMPICKUP, "ri", n);
                    ents[n]->spawned = false; // even if someone else gets it first
                }
                break;
                    
            case TELEPORT:
            {
                if(d->lastpickup==ents[n]->type && lastmillis-d->lastpickupmillis<500) break;
                d->lastpickup = ents[n]->type;
                d->lastpickupmillis = lastmillis;
                teleport(n, d);
                break;
            }

            case RESPAWNPOINT:
                if(d!=cl.player1) break;
                if(n==cl.respawnent) break;
                cl.respawnent = n;
                conoutf(CON_GAMEINFO, "\f2respawn point set!");
                playsound(S_V_RESPAWNPOINT);
                break;

            case JUMPPAD:
            {
                if(d->lastpickup==ents[n]->type && lastmillis-d->lastpickupmillis<300) break;
                d->lastpickup = ents[n]->type;
                d->lastpickupmillis = lastmillis;
                vec v((int)(char)ents[n]->attr3*10.0f, (int)(char)ents[n]->attr2*10.0f, ents[n]->attr1*12.5f);
                d->timeinair = 0;
                d->falling = vec(0, 0, 0);
                d->vel.z = 0;
                d->vel.add(v);
                cl.playsoundc(S_JUMPPAD, d);
                break;
            }
        }
    }

    void checkitems(fpsent *d)
    {
        if(d==cl.player1 && (editmode || cl.cc.spectator)) return;
        vec o = d->o;
        o.z -= d->eyeheight;
        loopv(ents)
        {
            extentity &e = *ents[i];
            if(e.type==NOTUSED) continue;
            if(!e.spawned && e.type!=TELEPORT && e.type!=JUMPPAD && e.type!=RESPAWNPOINT) continue;
            float dist = e.o.dist(o);
            if(dist<(e.type==TELEPORT ? 16 : 12)) trypickup(i, d);
        }
    }

    void checkquad(int time, fpsent *d)
    {
        if(d->quadmillis && (d->quadmillis -= time)<=0)
        {
            d->quadmillis = 0;
            playsound(S_PUPOUT, d==cl.player1 ? NULL : &d->o);
            if(d==cl.player1) conoutf(CON_GAMEINFO, "\f2quad damage is over");
        }
    }

    void putitems(ucharbuf &p)            // puts items in network stream and also spawns them locally
    {
        putint(p, SV_ITEMLIST);
        loopv(ents) if(ents[i]->type>=I_SHELLS && ents[i]->type<=I_QUAD && (!m_noammo || ents[i]->type<I_SHELLS || ents[i]->type>I_CARTRIDGES))
        {
            putint(p, i);
            putint(p, ents[i]->type);
        }
        putint(p, -1);
    }

    void resetspawns() { loopv(ents) ents[i]->spawned = false; }

    void spawnitems()
    {
        if(m_noitems) return;
        loopv(ents) if(ents[i]->type>=I_SHELLS && ents[i]->type<=I_QUAD && (!m_noammo || ents[i]->type<I_SHELLS || ents[i]->type>I_CARTRIDGES))
        {
            ents[i]->spawned = m_sp || (ents[i]->type!=I_QUAD && ents[i]->type!=I_BOOST);
        }
    }

    void setspawn(int i, bool on) { if(ents.inrange(i)) ents[i]->spawned = on; }

    extentity *newentity() { return new fpsentity(); }
    void deleteentity(extentity *e) { delete (fpsentity *)e; }

    void clearents()
    {
        while(ents.length()) deleteentity(ents.pop());
    }

    void fixentity(extentity &e)
    {
        switch(e.type)
        {
            case FLAG:
            case BOX:
            case BARREL:
            case PLATFORM:
            case ELEVATOR:
                e.attr5 = e.attr4;
                e.attr4 = e.attr3;
                e.attr3 = e.attr2;
            case MONSTER:
            case TELEDEST:
                e.attr2 = e.attr1;
            case RESPAWNPOINT:
                e.attr1 = (int)cl.player1->yaw;
                break;
        }
    }

    void entradius(extentity &e, float &radius, float &angle, vec &dir, vec &color)
    {
        switch(e.type)
        {
            case DYNLIGHT:
                radius = e.attr1 ? e.attr1 : getworldsize();
                color = vec(e.attr2/255.0f, e.attr3/255.0f, e.attr4/255.0f);
                break;
            case TELEPORT:
                loopv(ents) if(ents[i]->type == TELEDEST && e.attr1==ents[i]->attr2)
                {
                    radius = e.o.dist(ents[i]->o);
                    dir = vec(ents[i]->o).sub(e.o).normalize();
                    break;
                }
                break;

            case JUMPPAD:
                radius = 4;
                dir = vec((int)(char)e.attr3*10.0f, (int)(char)e.attr2*10.0f, e.attr1*12.5f).normalize();
                break;

            case FLAG:
            case MONSTER:
            case TELEDEST:
            case MAPMODEL:
            case RESPAWNPOINT:
            case BOX:
            case BARREL:
            case PLATFORM:
            case ELEVATOR:
                radius = 4;
                vecfromyawpitch(e.attr1, 0, 1, 0, dir);
                break;
        }
    }
    
    bool radiusent(extentity &e)
    {
    	switch(e.type)
    	{
    		case LIGHT:
    		case ENVMAP:
    		case MAPSOUND:
    		case DYNLIGHT:
    			return true;
    			break;
    		default:
    			return false;
    			break;
    	}
    }
    
    bool dirent(extentity &e)
    {
    	switch(e.type)
    	{
    		case MAPMODEL:
    		case PLAYERSTART:
    		case SPOTLIGHT:
    		case TELEDEST:
    		case TELEPORT:
    		case MONSTER:
    		case JUMPPAD:
    		case BOX:
    		case BARREL:
    		case PLATFORM:
    		case ELEVATOR:
    		case RESPAWNPOINT:
    		case FLAG:
    			return true;
    			break;
    		default:
    			return false;
    			break;
    	}
    }

    const char *entnameinfo(entity &e) { return ""; }
    const char *entname(int i)
    {
        static const char *entnames[] =
        {
            "none?", "light", "mapmodel", "playerstart", "envmap", "particles", "sound", "spotlight",
            "shells", "bullets", "rockets", "riflerounds", "grenades", "cartridges",
            "health", "healthboost", "greenarmour", "yellowarmour", "quaddamage",
            "teleport", "teledest",
            "creature", "carrot", "jumppad",
            "banana", "respawnpoint",
            "box", "barrel",
            "platform", "elevator",
            "flag", "dynlight"
            "", "", "", "",
        };
        return i>=0 && size_t(i)<sizeof(entnames)/sizeof(entnames[0]) ? entnames[i] : "";
    }
    
    void renderhelpertext(extentity &e, int &colour, vec &pos, string &tmp)
    {
	switch(e.type)
	{
		case TELEPORT:
			pos.z += 3.0;
			s_sprintf(tmp)("Teledest Tag: %i\nModel: %s (%i)",
				e.attr1,
				mapmodelname(e.attr2),
				e.attr2
			);
			return;
		case TELEDEST:
			pos.z += 3;
			s_sprintf(tmp)("Yaw: %1: %i\nTeleport Tag: %i\n",
				e.attr1,
				e.attr2
			);
			return;
		case MONSTER:
			pos.z += 4.5;
			s_sprintf(tmp)("Yaw: %i\nMonster: %s (%i)\nTag: monster_dead_%i",
				e.attr1,
				(e.attr2 >= 0 && e.attr2 < cl.ms.NUMMONSTERTYPES) ? cl.ms.monstertypes[e.attr2].name : "(null)", e.attr2,
				e.attr3
			);
			return;
		case JUMPPAD:
			pos.z += 4.5;
			s_sprintf(tmp)("Z: %i\nY: %i\nX: %i",
				e.attr1,
				e.attr2,
				e.attr3
			);
			return;
		case BASE:
			pos.z += 3.0f;
			s_sprintf(tmp)("Ammo: %i\nTag: base_%i",
				e.attr1,
				e.attr2
			);
			return;
		case RESPAWNPOINT:
			pos.z += 1.5;
			s_sprintf(tmp)("Yaw: %i",
				e.attr1
			);
			return;
		case BOX:
		case BARREL:
			pos.z += 6.0;
			s_sprintf(tmp)("Yaw: %i\nModel: %s (%i)\nWeight: %i\nHealth: %i",
				e.attr1,
				mapmodelname(e.attr2), e.attr2,
				e.attr3,
				e.attr4
			);
			return;
		case PLATFORM:
		case ELEVATOR:
			pos.z += 6.0;
			s_sprintf(tmp)("Yaw: %i\nModel: %s (%i)\nTag: %i\nSpeed: %i",
				e.attr1,
				mapmodelname(e.attr2), e.attr2,
				e.attr3,
				e.attr4
			);
			return;
		case FLAG:
			pos.z += 3.0f;
			s_sprintf(tmp)("Yaw: %i\nTeam: %i",
				e.attr1,
				e.attr2
			);
			return;
		case DYNLIGHT:
			pos.z += 7.5;
			s_sprintf(tmp)("Radius %i\n\fs\fRRed: %i\n\fJGreen: %i\n\fDBlue: %i\fr\nTag: %i",
				e.attr1,
				e.attr2,
				e.attr3,
				e.attr4,
				e.attr5
			);
			return;
	}
    }
    
    int extraentinfosize() { return 0; }       // size in bytes of what the 2 methods below read/write... so it can be skipped by other games

    void writeent(entity &e, char *buf)   // write any additional data to disk (except for ET_ ents)
    {
    }

    void readent(entity &e, char *buf)     // read from disk, and init
    {
        int ver = getmapversion();
        if(ver <= 10)
        {
            if(e.type >= 7) e.type++;
        }
        if(ver <= 12)
        {
            if(e.type >= 8) e.type++;
        }
    }

    void editent(int i)
    {
        extentity &e = *ents[i];
        cl.cc.addmsg(SV_EDITENT, "rii3ii5", i, (int)(e.o.x*DMF), (int)(e.o.y*DMF), (int)(e.o.z*DMF), e.type, e.attr1, e.attr2, e.attr3, e.attr4, e.attr5);
    }

    float dropheight(entity &e)
    {
        if(e.type==MAPMODEL || e.type==BASE || e.type==FLAG) return 0.0f;
        return 4.0f;
    }
};
