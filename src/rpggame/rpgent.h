struct mproj
{
	vec pos, dir;
	rpgobj *weapon;
	float dist;
	
	mproj(vec _p, vec _d, rpgobj *_w, float _di) : pos(_p), dir(_d), weapon(_w), dist(_di)
	{}
	~mproj() {}
};

struct rpgent : dynent
{
    rpgobj *ro;
    rpgclient &cl;

    int lastaction, lastpain;
    bool attacking;
	int lastpickupmillis, checkpoint;

	
    vector<mproj *> magicproj;

    rpgent *enemy;

    enum { R_STARE, R_ROAM, R_SEEK, R_ATTACK, R_BLOCKED, R_BACKHOME };
    int npcstate;

    int trigger;

    float sink;

    vec home;

    enum { ROTSPEED = 200 };

    rpgent(rpgobj *_ro, rpgclient &_cl, const vec &_pos, float _yaw, int _maxspeed = 40, int _type = ENT_AI) : ro(_ro), cl(_cl), lastaction(0), lastpain(0), attacking(false), lastpickupmillis(0), checkpoint(-1), enemy(NULL), npcstate(R_STARE), trigger(0), sink(0)
    {
        o = _pos;
        home = _pos;
        yaw = _yaw;
        maxspeed = _maxspeed;
        type = _type;
        enemy = &cl.player1;
	magicproj.setsize(0);
    }

    float vecyaw(vec &t) { return -(float)atan2(t.x-o.x, t.y-o.y)/RAD+180; }

    static const int ATTACKSAMPLES = 32;

    void tryattackobj(rpgobj &eo, rpgobj &weapon)
    {
        if(!eo.s_ai || (eo.s_ai==ro->s_ai && eo.ent!=enemy)) return;

        rpgent &e = *eo.ent;
        if(e.state!=CS_ALIVE) return;

        vec d = e.o;
        d.sub(o);
        d.z = 0;
        if(d.magnitude()>e.radius+weapon.s_maxrange) return;

        if(o.z+aboveeye<=e.o.z-e.eyeheight || o.z-eyeheight>=e.o.z+e.aboveeye) return;

        vec p(0, 0, 0), closep;
        float closedist = 1e10f;
        loopj(ATTACKSAMPLES)
        {
            p.x = e.xradius * cosf(2*M_PI*j/ATTACKSAMPLES);
            p.y = e.yradius * sinf(2*M_PI*j/ATTACKSAMPLES);
            p.rotate_around_z((e.yaw+90)*RAD);

            p.x += e.o.x;
            p.y += e.o.y;
            float tyaw = vecyaw(p);
            normalize_yaw(tyaw);
            if(fabs(tyaw-yaw)>weapon.s_maxangle) continue;

            float dx = p.x-o.x, dy = p.y-o.y, dist = dx*dx + dy*dy;
            if(dist<closedist) { closedist = dist; closep = p; }
        }

        if(closedist>weapon.s_maxrange*weapon.s_maxrange) return;

        weapon.useaction(eo, *this, true);
    }

    #define loopallrpgobjsexcept(ro) loop(i, cl.os.set.length()+1) for(rpgobj *eo = i ? cl.os.set[i-1] : cl.os.playerobj; eo; eo = NULL) if((ro)!=eo)

    void tryattack(vec &lookatpos, rpgobj &weapon)
    {
        if(lastmillis-lastaction<weapon.s_attackrate) return;

        lastaction = lastmillis;

        switch(weapon.s_usetype)
        {
            case 1:
                if(!weapon.s_damage) return;
                weapon.usesound(this);
                loopallrpgobjsexcept(ro) tryattackobj(*eo, weapon);
                break;

            case 2:
            {
                if(!weapon.s_damage) return;
                weapon.usesound(this);
                vec flarestart = o;
                flarestart.z -= 2;
                particle_flare(flarestart, lookatpos, 600, PART_STREAK, 0x7FFF00);  // FIXME hudgunorigin(), and shorten to maxrange
                float bestdist = 1e16f;
                rpgobj *best = NULL;
                loopallrpgobjsexcept(ro)
                {
                    if(eo->ent->state!=CS_ALIVE) continue;
                    if(!intersect(eo->ent, o, lookatpos)) continue;
                    float dist = o.dist(eo->ent->o);
                    if(dist<weapon.s_maxrange && dist<bestdist)
                    {
                        best = eo;
                        bestdist = dist;
                    }
                }
                if(best) weapon.useaction(*best, *this, true);
                break;
            }
            case 3:
                if(weapon.s_maxrange)   // projectile, cast on target
                {
                    if(!ro->usemana(weapon)) return;
                   
                    vec dir;
                    float worlddist = lookatpos.dist(o, dir);
                    dir.normalize();
		    
		    magicproj.add(new mproj(o, dir, &weapon, min(float(weapon.s_maxrange), worlddist)));
                }
                else
                {
                    weapon.useaction(*ro, *this, true);   // cast on self
		    cl.fx.addfx(weapon.s_effect, *ro->ent);
                }
                break;
        }

    }

    void updateprojectile()
    {
        loopvj(magicproj)
	{
		mproj &mp = *magicproj[j];
		rpgeffect fx;
		
		if(cl.fx.rpgeffects.inrange(mp.weapon->s_effect))
			fx = *cl.fx.rpgeffects[mp.weapon->s_effect];
		else if(cl.fx.rpgeffects.length())
			fx = *cl.fx.rpgeffects[0];
		else
		{
			conoutf("error: no effects declared");
			magicproj.remove(j); j--;
			continue;
		}
		
		regular_particle_splash(fx.trailpart, 2, 450, mp.pos, fx.trailcol, fx.trailsize);
		particle_splash(fx.projpart, 1, 1, mp.pos, fx.projcol, fx.projsize);

		float dist = (curtime * (float)fx.vel) / 1000;
		if((mp.dist -= dist)<0) { magicproj.remove(j); j--; };

		vec mpto = vec(mp.dir).mul(dist).add(mp.pos);
		bool hit = false;

		loopallrpgobjsexcept(ro)     // FIXME: make fast "give me all rpgobs in range R that are not X" function
		{
			if(eo->ent->o.dist(mp.pos)<fx.rad && intersect(eo->ent, mp.pos, mpto))  // quick reject, for now
			{
				mp.weapon->useaction(*eo, *this, false);    // cast on target
				cl.fx.addfx(mp.weapon->s_effect, *eo->ent);
				hit = true;
			}
		}

		if(hit) {magicproj.remove(j); j--; continue;}
		mp.pos = mpto;
	}
    }

    void transition(int _state, int _moving, int n)
    {
        npcstate = _state;
        move = _moving;
        trigger = lastmillis+n;
    }

    void gotoyaw(float yaw, int s, int m, int t)
    {
        targetyaw = yaw;
        rotspeed = ROTSPEED;
        transition(s, m, t);
    }

    void gotopos(vec &pos, int s, int m, int t) { gotoyaw(vecyaw(pos), s, m, t); }

    void goroam()
    {
        if(home.dist(o)>128 && npcstate!=R_BACKHOME)  gotopos(home, R_ROAM, 1, 1000);
        else                                          gotoyaw(targetyaw+90+rnd(180), R_ROAM, 1, 1000);
    }

    void stareorroam()
    {
        if(rnd(10)) transition(R_STARE, 0, 500);
        else goroam();
    }

    void update(float playerdist)
    {
        updateprojectile();

        if(state==CS_DEAD) { stopmoving(); return; };

        if(blocked && npcstate!=R_BLOCKED && npcstate!=R_SEEK)
        {
            blocked = false;
            gotoyaw(targetyaw+90+rnd(180), R_BLOCKED, 1, 1000);
        }

        if(ro->s_hp<ro->eff_maxhp() && npcstate!=R_SEEK) gotopos(enemy->o, R_SEEK,  1, 200);

        #define ifnextstate   if(trigger<lastmillis)
        #define ifplayerclose if(playerdist<64)

        switch(npcstate)
        {
            case R_BLOCKED:
            case R_BACKHOME:
                ifnextstate goroam();
                break;

            case R_STARE:
                ifplayerclose
                {
					if(ro->s_ai==4) {npcstate = R_ROAM;}
                    if(ro->s_ai==3) { gotopos(cl.player1.o, R_SEEK,  1, 200); enemy = &cl.player1; }
                    else            { gotopos(cl.player1.o, R_STARE, 0, 500); }
                }
                else ifnextstate stareorroam();
                break;

            case R_ROAM:
                ifplayerclose    transition(R_STARE, 0, 500);
                else ifnextstate stareorroam();
                break;

            case R_SEEK:
                ifnextstate
                {
                    vec target;
                    if(raycubelos(o, enemy->o, target))
                    {
                        rpgobj &weapon = ro->selectedweapon();
                        if(target.dist(o)<weapon.s_maxrange) tryattack(target, weapon);
                    }
                    if(enemy->o.dist(o)>256) goroam();
                    else gotopos(enemy->o, R_SEEK, 1, 100);
                }
        }

        #undef ifnextstate
        #undef ifplayerclose
    }

    void updateplayer(vec &lookatpos)     // alternative version of update() if this ent is the main player
    {
        updateprojectile();

        if(state==CS_DEAD)
        {
            //lastaction = lastmillis;
            if(lastmillis-lastaction>1500)
            {
                findplayerspawn(this, checkpoint);
                state = CS_ALIVE;
		lastpickupmillis = 0;
                ro->st_respawn();
                attacking = false;
                particle_splash(PART_EDIT, 1000, 500, o, 0x007FFF);
            }
        }
        else
        {
            moveplayer(this, 20, true);
            ro->st_update(lastmillis);
            if(attacking) tryattack(lookatpos, ro->selectedweapon());
            cl.et.checkitems(this);
        }
    }
};
