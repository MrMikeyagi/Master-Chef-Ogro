static const int SPAWNNAMELEN = 64;

struct rpgentity : extentity
{
    char name[SPAWNNAMELEN];

    rpgentity() { memset(name, 0, SPAWNNAMELEN); }
};

struct rpgentities : icliententities
{
    rpgclient &cl;
    vector<rpgentity *> ents;
    rpgentity *lastcreated;

    ~rpgentities() {}
    rpgentities(rpgclient &_cl) : cl(_cl), lastcreated(NULL)
    {
        CCOMMAND(spawnname, "s", (rpgentities *self, char *s), { if(self->lastcreated) { s_strncpy(self->lastcreated->name, s, SPAWNNAMELEN); self->spawnfroment(*self->lastcreated); } });
    }

    vector<extentity *> &getents() { return (vector<extentity *> &)ents; }

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
	SPAWN = ET_GAMESPECIFIC,
	TELEPORT, //attr1 = teledest, attr2 = radius (0 = 16), attr3 = model
	TELEDEST, //attr1 = yaw, attr2 = from
	JUMPPAD, //attr1 = Z, attr2 = Y, attr3 = X, attr4 = radius
	CHECKPOINT,
	MAXENTTYPES
};

	int testdist(extentity &e)
	{
		switch(e.type)
		{
			case TELEPORT:
				return e.attr2 ? abs(e.attr2) : 16;
			case CHECKPOINT:
				return e.attr2 ? abs(e.attr2) : 16;
			case JUMPPAD:
				return e.attr4 ? abs(e.attr4) : 12;

		}
		return 24;
	}
	void fixentity(extentity &e)
	{
		lastcreated = (rpgentity *)&e;
		switch(e.type)
		{
			case SPAWN:
			case PLAYERSTART:
			case CHECKPOINT:
			case TELEDEST:
			{
				e.attr5 = e.attr4;
				e.attr4 = e.attr3;
				e.attr3 = e.attr2;
				e.attr2 = e.attr1;
				e.attr1 = (int)cl.player1.yaw;
				break;
			}
		}
	}
	bool teleport(rpgent *d, int dest)
	{
		loopv(ents)
		{
			extentity &e = *ents[i];
			if(e.type==TELEDEST && e.attr2 == dest)
			{
				d->o = d->newpos = vec(e.o).add(vec(0, 0, d->eyeheight));
				d->vel = d->falling = vec(0,0,0);
				d->yaw = abs(e.attr1) % 360;
				//particle_splash(PART_EDIT, 1250, 750, e.o, d==cl.player1 ? 0x0000FF : 0xFF0000);
				//playsound(S_TELEPORT, &e.o);
				return true;

			}
		}
		conoutf("no such teledest: %i", dest);
		return false;
	}

	void trypickup(int n, rpgent *d)
	{
		switch(ents[n]->type)
		{
			case TELEPORT:
				if(lastmillis < d->lastpickupmillis + 200) return;
				if(teleport(d, ents[n]->attr1))
					d->lastpickupmillis  = lastmillis;
				return;
			case JUMPPAD:
				if(lastmillis < d->lastpickupmillis + 200) return;
				d->falling = vec(0, 0, 0);
				d->vel.z = ents[n]->attr1 * 10;
				d->vel.y += ents[n]->attr2 * 10;
				d->vel.x += ents[n]->attr3 * 10;
				//playsound(S_JUMPPAD, &ents[n]->o);
				d->lastpickupmillis = lastmillis;
				return;
			case CHECKPOINT:
				if(d==&cl.player1 && n!=cl.player1.checkpoint)
				{
					vec emit = ents[n]->o;
					emit.z += 5;
					cl.player1.checkpoint = n;
					conoutf("\f2REGENERATION CENTER ACTIVATED!!");
					s_sprintfd(ds)("@Checkpoint!");
					particle_text(emit, ds, PART_TEXT_RISE, 2000, 0x7FFF7F, 8.0f);
				}
				return;
		}
	}
	void checkitems(rpgent *d)
	{
		if(d==&cl.player1 && editmode) return;
		vec o = d->o;
		o.z -= d->eyeheight;
		loopv(ents)
		{
			extentity &e = *ents[i];
			if(e.type==NOTUSED) continue;
			if(e.type!=TELEPORT && e.type!=JUMPPAD && e.type!=CHECKPOINT) continue;
			float dist = e.o.dist(o);
			if(dist<testdist(e)) trypickup(i, d);
		}
	}

	void renderent(extentity &e, const char *mdlname, float z, float yaw, int anim = ANIM_MAPMODEL)
	{
		if(!mdlname) return;
		rendermodel(&e.light, mdlname, anim|ANIM_LOOP, vec(e.o).add(vec(0, 0, z)), yaw, 0, MDL_SHADOW | MDL_CULL_VFC | MDL_CULL_DIST | MDL_CULL_OCCLUDED);
	}

	void renderentities()
	{
		loopv(ents)
		{
			extentity &e = *ents[i];
			switch(e.type)
			{
				case CHECKPOINT:
					renderent(e, "dcp/jumppad2/regenpad", 0, e.attr1, ANIM_IDLE);
					continue;
				case TELEPORT:
					renderent(e, mapmodelname(e.attr3), (float)(1+sin(lastmillis/100.0+e.o.x+e.o.y)/20), lastmillis/10.0f);
					continue;
				default:
					continue;
			}
		}
	}

	void entradius(extentity &e, float &radius, float &angle, vec &dir, vec &color)
	{
		switch(e.type)
		{
			case TELEDEST:
				radius = 4;
				vecfromyawpitch(e.attr1, 0, 1, 0, dir);
				break;
			case TELEPORT:
				loopv(ents)
				{
					if(ents[i]->type == TELEDEST && e.attr1==ents[i]->attr2)
					{
						radius = e.o.dist(ents[i]->o);
						dir = vec(ents[i]->o).sub(e.o).normalize();
						break;
					}
				}
                		break;
			case JUMPPAD:
				radius = 4;
				dir = vec((int)(char)e.attr3*10.0f, (int)(char)e.attr2*10.0f, e.attr1*12.5f).normalize();
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
			case TELEPORT:
			case TELEDEST:
			case CHECKPOINT:
			case JUMPPAD:
				return true;
				break;
			default:
				return false;
				break;
		}
	}

    void editent(int i) {}

    const char *entnameinfo(entity &e) { return ((rpgentity &)e).name; }
    const char *entname(int i)
    {
        static const char *entnames[] = { "none?", "light", "mapmodel", "playerstart", "envmap", "particles", "sound", "spotlight", "spawn", "teleport", "teledest", "jumppad","checkpoint" };
        return i>=0 && size_t(i)<sizeof(entnames)/sizeof(entnames[0]) ? entnames[i] : "";
    }

    void renderhelpertext(extentity &e, int &colour, vec &pos, string &tmp)
    {
	switch(e.type)
	{
		case SPAWN:
			pos.z += 1.5;
			s_sprintf(tmp)("Yaw: %i",
				e.attr1
			);
			return;
		case TELEPORT:
			pos.z += 4.5;
			s_sprintf(tmp)("Teleport Tag: %i\nRadius: %i\nModel: %s (%i)",
				e.attr1,
				e.attr2,
				mapmodelname(e.attr3), e.attr3
			);
			return;
		case TELEDEST:
			pos.z += 3.0;
			s_sprintf(tmp)("Yaw: %i\nTeleport Tag: %i",
				e.attr1,
				e.attr2
			);
			return;
		case CHECKPOINT:
				pos.z += 1.5;
				s_sprintf(tmp)("Yaw: %i",
					e.attr1
				);
				return;
		case JUMPPAD:
			pos.z += 6.0;
			s_sprintf(tmp)("Z: %i\nY: %i\nX: %i\nRadius: %i",
				e.attr1,
				e.attr2,
				e.attr3,
				e.attr4
			);
			return;
	}
    }

    int extraentinfosize() { return SPAWNNAMELEN; }
    void writeent(entity &e, char *buf) { memcpy(buf, ((rpgentity &)e).name, SPAWNNAMELEN); }
    void readent (entity &e, char *buf) { memcpy(((rpgentity &)e).name, buf, SPAWNNAMELEN); }

    float dropheight(entity &e) { return e.type==ET_MAPMODEL ? 0 : 4; }

    void rumble(const extentity &e) { playsoundname("free/rumble", &e.o); }
    void trigger(extentity &e) {}

    extentity *newentity() { return new rpgentity; }
    void deleteentity(extentity *e) { delete (rpgentity *)e; }

    void clearents()
    {
        while(ents.length()) deleteentity(ents.pop());
    }

    void spawnfroment(rpgentity &e)
    {
        cl.os.spawn(e.name);
        cl.os.placeinworld(e.o, e.attr1);
    }

    void startmap()
    {
        lastcreated = NULL;
        loopv(ents) if(ents[i]->type==SPAWN) spawnfroment(*ents[i]);
    }
};
