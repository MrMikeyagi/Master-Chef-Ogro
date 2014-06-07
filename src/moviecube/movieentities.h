// static entity types
enum
{
  NOTUSED = ET_EMPTY,         // entity slot not in use in map
  LIGHT = ET_LIGHT,           // lightsource, attr1 = radius, attr2 = intensity
  MAPMODEL = ET_MAPMODEL,     // attr1 = angle, attr2 = idx
  PLAYERSTART,                // attr1 = angle, attr2 = team
  ENVMAP = ET_ENVMAP,         // attr1 = radius
  PARTICLES = ET_PARTICLES,
  MAPSOUND = ET_SOUND,
  SPOTLIGHT = ET_SPOTLIGHT,
  BOX,                        // attr1 = angle, attr2 = idx, attr3 = weight
  BARREL,                     // attr1 = angle, attr2 = idx, attr3 = weight, attr4 = health
  PLATFORM,                   // attr1 = angle, attr2 = idx, attr3 = tag, attr4 = speed
  ELEVATOR,                   // attr1 = angle, attr2 = idx, attr3 = tag, attr4 = speed
  ACTOR,                      // attr1 = nbr,   attr2 = yaw
  SCRIPT,                     // attr1 = 
  WAYPOINT,										// attr1 = yaw
  CAMERA,                     // attr1 = nbr, attr2 = yaw, attr3 = pitch, attr4 = fov
  MAXENTTYPES
};


//flags for searching entities
enum 
{
  NONE = 1<<0,     //only type
  ATTR1 = 1<<1,    //arg1 ...
  ATTR2 = 1<<2,
  ATTR3 = 1<<3,
  ATTR4 = 1<<4,
  ATTR5 = 1<<5,
};

struct movieentity : extentity
{	
  char name[MAXNAMELEN];
  movieentity()	{memset(name, 0, MAXNAMELEN);}
};

struct movieentities : icliententities
{	
	movieclient 																&cl;
	vector<movieentity*> 												ents;
	
	movieentities(movieclient &_cl) : cl(_cl)
  { 
    CCOMMAND(newentactor, "", (movieentities *self), intret( self->newentactor() ) );
  }
	
  ~movieentities() {}

  //ToDo: remove
  int newentactor() 
  {
    int z = 0;

    if (countents(ACTOR) < 1) { newent(newstring("actor"), &z, &z, &z, &z, &z); return 0;}
    for (int n = 0; n <= countents(ACTOR); n++) 
    { 
      if ( findent(ACTOR, ATTR1, n) == -1 )  { newent(newstring("actor"), &n, &z, &z, &z, &z); return n; }
    }
    return -1;
  }
    	
  vector<extentity *> &getents() { return (vector<extentity *> &)ents; }
  
 	void rumble(const extentity &e) {}
	void trigger(extentity &e) {}
	
	// these two functions are called when the server acknowledges that you really
	// picked up the item (in multiplayer someone may grab it before you).
	
	void pickupeffects(int n, movieent *d) {}
	
	// these functions are called when the client touches the item
	
	void teleport(int n, movieent *d) {}
	void trypickup(int n, movieent *d) {}
	void checkitems(movieent *d) {}
	void checkquad(int time, movieent *d) {}
	void putitems(ucharbuf &p, int gamemode) {}           // puts items in network stream and also spawns them locally
	
	void resetspawns() { loopv(ents) ents[i]->spawned = false; }
	void spawnitems(int gamemode) {}
	void setspawn(int i, bool on) { if(ents.inrange(i)) ents[i]->spawned = on; }
	
	extentity *newentity() { return new movieentity(); }
  
  //find entities by type and attributes
  int findent(int type, int flags, int attr1 = 0, int attr2 = 0, int attr3 = 0, int attr4 = 0, int attr5 = 0)
  {
    int found = -1;
    loopv(ents) if (ents[i]->type == type)
    {
      if(flags & ATTR1) { if (attr1 != ents[i]->attr1) continue; }
      if(flags & ATTR2) { if (attr2 != ents[i]->attr2) continue; }
      if(flags & ATTR3) { if (attr3 != ents[i]->attr3) continue; }
      if(flags & ATTR4) { if (attr4 != ents[i]->attr4) continue; }
      if(flags & ATTR5) { if (attr5 != ents[i]->attr5) continue; }
      found = i;
      break;
    }
    return found;
  }
  
  int countents(int type)
  {
    int n = 0;
    loopv(ents) if (ents[i]->type == type) 
    { 
      n++; 
    }
    return n;
  }
  
	void deleteentity(extentity *e) {  } //dummy
	void fixentity(extentity &e)
	{
		switch(e.type)
		{
			case BOX:
			case BARREL:
			case PLATFORM:
			case ELEVATOR:
			{
				e.attr4 = e.attr3;
				e.attr3 = e.attr2;
        break;
			}
			case ACTOR:
      {
        e.attr2 = cl.player1->yaw;
        break;
      }
 			case SCRIPT:
      {
				break;
      }
      case WAYPOINT: //ToDo: Bug - Waypoint 0 will not be found
      {
        if (countents(WAYPOINT) < 1) break;
        for (int n = 0; n <= countents(WAYPOINT); n++) 
        { 
          if ( findent(WAYPOINT, ATTR1, n) == -1 )  { e.attr1 = n; break; }
        }
        break;
      }
      case CAMERA:
      {
        if (countents(CAMERA) > 0)
        {
          for (int n = 0; n <= countents(CAMERA); n++) 
          { 
            if ( findent(CAMERA, ATTR1, n) == -1 )  
            { 
              e.attr1 = n;
              break; 
            }
          }
        }
        e.o = cl.player1->o;
        e.attr2 = cl.player1->yaw;
        e.attr3 = cl.player1->pitch;
        e.attr4 = 100; // ToDo: reserved for fov
        break;
      }
    }
	}
	
	void entradius(extentity &e, float &radius, float &angle, vec &dir)
	{
		switch(e.type)
		{
			case MAPMODEL:
			case BOX:
			case BARREL:
			case PLATFORM:
			case ELEVATOR:
				radius = 4;
				vecfromyawpitch(e.attr1, 0, 1, 0, dir);
				break;
      case WAYPOINT:
        radius = 1;
        break;
		}
	}

  void renderentities() {}  
  
	int extraentinfosize() { return MAXNAMELEN; }
  const char *entnameinfo(entity &e) { return ((movieentity &)e).name; }

  const char *entname(int i)
	{
		static const char *entnames[] =
		{
			"none?", "light", "mapmodel", "playerstart", "envmap", "particles", "sound", "spotlight",
			"box", "barrel",
			"platform", "elevator", "actor", "script", "waypoint", "camera",
			"", "", "", "",
		};
		return i>=0 && size_t(i)<sizeof(entnames)/sizeof(entnames[0]) ? entnames[i] : "";
	}

	void writeent(entity &e, char *buf)   // write any additional data to disk (except for ET_ ents)
	{ 
    memcpy(buf, ((movieentity &)e).name, MAXNAMELEN); 
	}
		
	void readent(entity &e, char *buf)     // read from disk, and init
	{
		//ToDo check MAPVERSION : int ver = getmapversion();
		memcpy(((movieentity &)e).name, buf, MAXNAMELEN);
    if (e.type == ACTOR)
    {
      cl.as.addactor(e.attr1, ((movieentity &)e).name);
    }
	}

	void editent(int i)
	{
		movieentity &e = *ents[i];
		cl.cc.addmsg(SV_EDITENT, "ri9i", i, (int)(e.o.x*DMF), (int)(e.o.y*DMF), (int)(e.o.z*DMF), e.type, e.attr1, e.attr2, e.attr3, e.attr4, e.attr5);
	}
	
	float dropheight(entity &e)
	{
		if(e.type==MAPMODEL) return 0.0f;
		return 4.0f;
	}
     void clearents()
    {
        while(ents.length()) deleteentity(ents.pop());
    }
};
