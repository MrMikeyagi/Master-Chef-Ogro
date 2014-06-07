struct sspentities : icliententities
{
	sspclient &cl;
	vector<extentity *> ents;
	
	sspentities(sspclient &_cl) : cl(_cl) {}

	vector<extentity *> &getents() { return ents; }
	extentity *newentity() { return new sspentity(); }
	void deleteentity(extentity *e) { delete (sspentity *)e; }
	
	int testdist(extentity &e)
	{
		switch(e.type)
		{
			case TELEPORT:
			case CHECKPOINT:
				return e.attr2 ? abs(e.attr2) : 16;
			case JUMPPAD:
				return e.attr4 ? abs(e.attr4) : 12;
			case PICKUP:
				if(cl.armourpickups.inrange(cl.player1->armourvec))
					return cl.player1->armour==ARM_ATTRACT ? 48 : 12; //quadruple distance for attractive armour
			case AXIS:
				return 16;
		}
		return 24;
	}
	
	void fixentity(extentity &e)
	{
		switch(e.type)
		{
			case BOX:
			case ENEMY:
			case TELEDEST:
			case CHECKPOINT:
			case PLATFORM:
			{
				e.attr5 = e.attr4;
				e.attr4 = e.attr3;
				e.attr3 = e.attr2;
				e.attr2 = e.attr1;
				e.attr1 = cl.player1->yaw;
				break;
			}
		}	
	}
	
	bool teleport(sspent *d, int dest)
	{
		loopv(ents)
		{
			extentity &e = *ents[i];
			if(e.type==TELEDEST && e.attr2 == dest)
			{
				d->o = d->newpos = vec(e.o).add(vec(0, 0, d->eyeheight));
				d->vel = d->falling = vec(0,0,0);
				d->yaw = abs(e.attr1) % 360;
				particle_splash(PART_EDIT, 1250, 750, e.o, d==cl.player1 ? 0x0000FF : 0xFF0000);
				playsound(S_TELEPORT, &e.o);
				return true;
				
			}
		}
		conoutf("no such teledest: %i", dest);
		return false;
	}
	
	void pickuppowerup(sspent *d, extentity &e)
	{
		if(d!=cl.player1) return; //we currently only want the player to be able to pickup these powerups
		int x = 0;
		vec emit = e.o;
		emit.z += 5;
		switch(e.attr1)
		{
			case PICKUP_COIN:
				if(cl.coinpickups.inrange(e.attr2))
				{
					x = cl.coinpickups[e.attr2]->amount;
					e.spawned = false;
					if(x>0)
					{
						cl.player1->coins += x;
						s_sprintfd(ds)("@%i coin%s", x, x>1 ? "s" : "");
						particle_text(emit, ds, PART_TEXT_RISE, 2000, 0xffd700, 8.0f);
					}
				}
				return;
			case PICKUP_WEAPON:
				if(cl.weaponpickups.inrange(e.attr2))
				{
					cl.player1->gunselect = e.attr2; //what else are we going to set :P
					e.spawned = false;
				}
				return;			
			case PICKUP_ARMOUR:
				if(cl.armourpickups.inrange(e.attr2))
				{
					cl.player1->armourvec = e.attr2; //mostly for model information
					//memo to self, make an int clamp (int min, int val, int max) function
					cl.player1->armour = min(ARM_MAX-1, max(cl.armourpickups[e.attr2]->type, ARM_PLAIN+0 /*+0 needed to avoid compilation error*/));
					e.spawned = false;
					
					const char *type[ARM_MAX] = {"", "Plain", "Attractive", "Winged", "Spiked"};
					s_sprintfd(ds)("@%s Armour", type[cl.armourpickups[e.attr2]->type]);
					particle_text(emit, ds, PART_TEXT_RISE, 2000, 0x007FFF, 8.0f);
				}
				return;			
			case PICKUP_HEALTH:
				if(cl.healthpickups.inrange(e.attr2))
				{
					x = cl.healthpickups[e.attr2]->amount; //there should be bad health packs too, eg, poisoned banana :P
					
					if(x>0 && cl.player1->maxhealth<=cl.player1->health) return; //keeps useful pickups from dissapearing, if you've no need of them yet
					e.spawned = false;
					
					cl.player1->health = min(cl.player1->maxhealth /*should be 3*/, cl.player1->health + x);
					if(x<0) cl.player1->lastpain = lastmillis; //my tummy hurts :(
					
					s_sprintfd(ds)("@%i HP", x);
					particle_text(emit, ds, PART_TEXT_RISE, 2000, x>=0 ? 0x00FF00 : 0xFF0000, 8.0f);
				}						
				return;
			case PICKUP_TIME:
				if(cl.timepickups.inrange(e.attr2))
				{
					x = cl.timepickups[e.attr2]->amount;
					e.spawned = false;
					cl.secsallowed += x; //there should be bad pickups that reduce time too!
					s_sprintfd(ds)("@%i seconds", x, x==1 ? "s" : "");
					particle_text(emit, ds, PART_TEXT_RISE, 2000, x>= 0 ? 0xAFAFAF : 0xFF0000, 8.0f);
				}
				return;
			case PICKUP_LIVES:
				if(cl.lifepickups.inrange(e.attr2))
				{
					x = cl.lifepickups[e.attr2]->amount;
					e.spawned = false;
					if(x>0)
					{
						cl.player1->lives = min(99, cl.player1->lives + x);
						s_sprintfd(ds)("@%i %s", x, x>1 ? "lives" : "life");
						particle_text(emit, ds, PART_TEXT_RISE, 2000, 0xffd700, 8.0f);
					}
				}
				return;			
			default:
				return;
		}
	}
	
	void trypickup(int n, sspent *d)
	{
		switch(ents[n]->type)
		{
			case TELEPORT:
				if(lastmillis < d->lastpickupmillis + 200) return;
				if(teleport(d, ents[n]->attr1))
					playsound(S_TELEPORT, &ents[n]->o);
				d->lastpickupmillis  = lastmillis;
				return;
			case JUMPPAD:
				if(lastmillis < d->lastpickupmillis + 200) return;
				d->falling = vec(0, 0, 0);
				d->vel.z = ents[n]->attr1 * 10;
				d->vel.y += ents[n]->attr2 * 10;
				d->vel.x += ents[n]->attr3 * 10;
				playsound(S_JUMPPAD, &ents[n]->o);
				d->lastpickupmillis = lastmillis;
				return;
			case PICKUP:
				//TODO, make items creep closer, when the player gets closer, as if attracted to the player
				pickuppowerup(d, *ents[n]);
				cl.player1->lastpickupmillis  = lastmillis;
				return;
			case CHECKPOINT:
				if(d==cl.player1 && n!=cl.player1->checkpoint)
				{
					vec emit = ents[n]->o;
					emit.z += 5;
					cl.player1->checkpoint = n;
					s_sprintfd(ds)("@Checkpoint!");
					particle_text(emit, ds, PART_TEXT_RISE, 2000, 0x7FFF7F, 8.0f);
				}
				cl.player1->lastpickupmillis  = lastmillis;
				return;
		}
	}
	
	void checkitems(sspent *d)
	{
		if(d==cl.player1 && editmode) return;
		vec o = d->o;
		o.z -= d->eyeheight;
		loopv(ents)
		{
			extentity &e = *ents[i];
			if(e.type==NOTUSED) continue;
			if(!e.spawned && e.type!=TELEPORT && e.type!=JUMPPAD && e.type!=CHECKPOINT) continue;
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
				case TELEPORT:
					renderent(e, mapmodelname(e.attr3), (float)(1+sin(lastmillis/100.0+e.o.x+e.o.y)/20), lastmillis/10.0f);
					continue;
				case CHECKPOINT:
					renderent(e, "butterfly", 0, e.attr1, ANIM_IDLE);
					continue;
				case PICKUP:
					if(e.spawned)
					{
						const char *mdl = "tentus/moneybag";
						switch(e.attr1)
						{
							case PICKUP_COIN:
								if(cl.coinpickups.inrange(e.attr2))
									mdl = cl.coinpickups[e.attr2]->mdl;
								else if(cl.coinpickups.length()>0)
									mdl = cl.coinpickups[0]->mdl;
									break;
							case PICKUP_WEAPON:
								if(cl.weaponpickups.inrange(e.attr2))
									mdl = cl.weaponpickups[e.attr2]->mdl;
								else if(cl.weaponpickups.length()>0)
									mdl = cl.weaponpickups[0]->mdl;
									break;
							case PICKUP_ARMOUR:
								if(cl.armourpickups.inrange(e.attr2))
									mdl = cl.armourpickups[e.attr2]->mdl;
								else if(cl.armourpickups.length()>0)
									mdl = cl.armourpickups[0]->mdl;
									break;
							case PICKUP_HEALTH:
								if(cl.healthpickups.inrange(e.attr2))
									mdl = cl.healthpickups[e.attr2]->mdl;
								else if(cl.healthpickups.length()>0)
									mdl = cl.healthpickups[0]->mdl;
									break;
							case PICKUP_TIME:
								if(cl.timepickups.inrange(e.attr2))
									mdl = cl.timepickups[e.attr2]->mdl;
								else if(cl.timepickups.length()>0)
									mdl = cl.timepickups[0]->mdl;
									break;
							case PICKUP_LIVES:
								if(cl.lifepickups.inrange(e.attr2))
									mdl = cl.lifepickups[e.attr2]->mdl;
								else if(cl.lifepickups.length()>0)
									mdl = cl.lifepickups[0]->mdl;
									break;
						}
						char *m = newstring(mdl);
						renderent(e, m, (float)(1+sin(lastmillis/100.0+e.o.x+e.o.y)/20), lastmillis/10.0f);
					}
					continue;
				case AXIS:
				{
					int radius = e.attr4 ? abs(e.attr4) : 16;
					vec dir = vec(0, 0, 0);
					vecfromyawpitch(e.attr1, 0, 1, 0, dir);
					dir.mul(radius);
					dir.add(e.o);
					particle_flare(e.o, dir, 1, PART_STREAK, 0x007FFF, 0.4);
					
					vecfromyawpitch(e.attr2, 0, 1, 0, dir);
					dir.mul(radius);
					dir.add(e.o);
					particle_flare(e.o, dir, 1, PART_STREAK, 0x007FFF, 0.4);
					continue;
				}
				default:
					continue;
			}
		}
	}
	
	void entradius(extentity &e, float &radius, float &angle, vec &dir, vec &color)
	{
		switch(e.type)
		{
			case ENEMY:
			case TELEDEST:
			case CHECKPOINT:
			case PLATFORMROUTE:
				radius = 4;
				vecfromyawpitch(e.attr1, 0, 1, 0, dir);
				break;
			case CAMERA:
				radius = 4;
				vecfromyawpitch(e.attr2, e.attr3, 1, 0, dir);
				break;
			case WAYPOINT:
				loopv(ents)
				{
					if(ents[i]->type == WAYPOINT && e.attr2==ents[i]->attr2)
					{
						radius = e.o.dist(ents[i]->o);
						dir = vec(ents[i]->o).sub(e.o).normalize();
						break;
					}
				}
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
			case ENEMY:
			case WAYPOINT:
			case TELEPORT:
			case TELEDEST:
			case CHECKPOINT:
			case JUMPPAD:
			case PLATFORMROUTE:
			case CAMERA:
				return true;
				break;
			default:
				return false;
				break;
		}
	}
	
	void prepareents()
	{
		loopv(ents)
		{
			extentity &e = *ents[i];
			switch(e.type)
			{
				case PICKUP:
					e.spawned = true;
					continue;
				case BOX:
				case ENEMY:
				case PLATFORM:
					continue; //TODO, box enemy and platform spawning
			}
		}
	}
	
	
	
	const char *entnameinfo(entity &e) { return ""; }
	int extraentinfosize() {return 0;}
	
	const char *entname(int i)
	{
		static const char *entnames[] =
		{
			"none?", "light", "mapmodel", "playerstart", "envmap", "particles", "sound", "spotlight", "box", "pickup",
			"enemy", "waypoint", "teleport", "teledest", "checkpoint", "jumppad", "platform", "platformroute", "camera", "axis", "", "", "" 
		};
	return i>=0 && size_t(i)<sizeof(entnames)/sizeof(entnames[0]) ? entnames[i] : "";
	}
	
	void renderhelpertext(extentity &e, int &colour, vec &pos, string &tmp)
	{
		switch(e.type)
		{
			case BOX:
				
				return;
			case PICKUP:
				pos.z += 3.0;
				s_sprintf(tmp)("Type: %s (%i)\nIndex: %i",
					e.attr1 == 0 ? "Coin" : e.attr1 == 1 ? "Weapon" : e.attr1 == 2 ? "Armour" : e.attr1 == 3 ? "Health" : e.attr1 == 4 ? "Time" : "Lives", e.attr1,
					e.attr2
				);
				return;
			case ENEMY:
				pos.z += 3.0;
				s_sprintf(tmp)("Yaw: %i\nIndex: %i",
					e.attr1,
					e.attr2
				);
				return;
			case WAYPOINT:
				
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
			case PLATFORM:
			
				return;
			case PLATFORMROUTE:
				
				return;
			case CAMERA:
				pos.z += 6.0;
				s_sprintf(tmp)("Tag: %i\nYaw: %i\nPitch: %i\n Distance: %i",
					e.attr1,
					e.attr2,
					e.attr3,
					e.attr4
				);
				return;
			case AXIS:
				pos.z += 7.5;
				s_sprintf(tmp)("Yaw1: %i\nYaw2:\nTag: axis_script_%i\nRadius: %i\nVert Radius: %i",
					e.attr1,
					e.attr2,
					e.attr3,
					e.attr4,
					e.attr5
				);
				return;
		}
	}
	
	void writeent(entity &e, char *buf) {}  // write any additional data to disk (except for ET_ ents)

	void readent(entity &e, char *buf)     // read from disk, and init
	{
		//int ver = getmapversion(); //commented only because it'd unused
	}
	
	float dropheight(entity &e)
	{
		if (e.type==MAPMODEL) return 0.0f;
		return 4.0f;
	}
	
	void clearents()
	{
		while(ents.length()) deleteentity(ents.pop());
	}
	
	//stubs
	void editent(int i) {}
	void rumble(const extentity &e) {}
	void trigger(extentity &e){}	
};

