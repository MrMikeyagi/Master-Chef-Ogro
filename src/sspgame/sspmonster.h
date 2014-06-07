struct sspmonster
{
	sspclient &cl;
	vector<extentity *> &ents;
	vector<int> intents; //INTeractive ENTities
	
	struct monster : sspent
	{
		sspclient &cl;
		sspmonster *sm;
		vec destination;
		int mtype, lastwaypoint;
		
		monster(vec _o, int _yaw, int _type, int _lwp, sspmonster *_sm) : cl(_sm->cl), sm(_sm), destination (0, 0, 0), mtype(_type), lastwaypoint(_lwp) {
			sspent();
			physent();
			type = ENT_AI;
			collidetype = COLLIDE_AABB;
			respawn();
			
			enemy &d = *sm->monstertypes[mtype];
			health = maxhealth = d.health;
			maxspeed = d.speed;
			
			o = newpos = _o; yaw = _yaw;
			o.z += eyeheight;
		}
		
		void move()
		{
			if(state==CS_ALIVE)
			{
				//check entities
				vec pos(o);
				pos.z -= (eyeheight);
				loopv(sm->intents)
				{
					if(lastpickupmillis+500 > lastmillis) break;
					entity &e = *sm->ents[sm->intents[i]];

					float dist = e.o.dist(pos);

					if(e.type==TELEPORT && dist <= (e.attr2 ? abs(e.attr2) : 16)) cl.et.trypickup(sm->intents[i], this);
					if(e.type==JUMPPAD && dist <= (e.attr4 ? abs(e.attr4) : 12)) cl.et.trypickup(sm->intents[i], this);
				}
				//check the player's position
				sspent &d = *cl.player1;
				if(d.state==CS_ALIVE)  //no point doing this is if either are dead
				{
					vec ppos(d.o);
					ppos.z -=d.eyeheight;
					
					if(o.dist(ppos) < 1.75 * radius && ppos.z - o.z < 2) //player gives the monster a booboo
					{
						d.vel.z = 100; //jump
						d.falling = vec(0, 0, 0);
						cl.takedamage(this, 1, 1000); //monsters also have a shorter invul time
					}
					else if(d.o.dist(pos) < 1.75 * d.radius && pos.z - d.o.z < 2) //monster gives the player a booboo
					{
						vel.z = 100;
						falling = vec(0, 0, 0);
						if(d.armour==ARM_SPIKE) {cl.takedamage(this, health, 1000); vel.mul(4);}
						cl.takedamage(&d, 1, 2000);
					}
				}
			}
			//finally, move the monster
			moveplayer(this, 10, true);
		}
	};
	
	vector<enemy *> monstertypes;
	vector<monster *> monsters;
	
	sspmonster(sspclient &_cl) : cl(_cl), ents(_cl.et.ents) {
		monstertypes.setsizenodelete(0);
		CCOMMAND(addmonster, "siiii", (sspmonster *self, char *s, int *t, int *x, int *y, int *z), {self->monstertypes.add(new enemy(s, *t, *x, *y, *z));});
	}
	
	void initialise()
	{
		monsters.setsizenodelete(0); //delete existing vectors first
		intents.setsizenodelete(0);
		
		loopv(ents)
		{
			extentity &e = *ents[i];
			if(e.type==ENEMY)
			{
				if(monstertypes.inrange(e.attr2))
				{
					monsters.add(new monster(e.o, e.attr1, e.attr2, e.attr3, this));
				}
			}
			if(e.type==TELEPORT || e.type==JUMPPAD)
			{
				intents.add(i);
			}
		}
	}
	
	void render()
	{
		loopv(monsters)
		{
			monster &m = *monsters[i];
			if(monstertypes.inrange(m.mtype))
			{
				if(m.lastpain + 1000 > lastmillis && (lastmillis % 100) > 50) continue; //flicker
				renderclient(&m, monstertypes[m.mtype]->mdl, NULL, 0, 300, ANIM_HOLD1|ANIM_LOOP, m.lastaction, m.lastpain);
					
			}
			else monsters.remove(i); //no point spawning if the monster it's supposed to be isn't declared
		}
	}
	
	void update()
	{
		loopv(monsters)
		{
			monster &m = *monsters[i];
			if(m.health <=0) m.state = CS_DEAD; //similar to player check.
			if(m.lastpain + 2000 > lastmillis || m.state == CS_ALIVE)
			{
				m.move();
			}
			else monsters.remove(i); //it's dead, so remove it
		}
	}
};
