struct sspweapons
{
	sspclient &cl;
	vector<proj *> projs;
	vector<projectile *> projectiles;
	
	sspweapons(sspclient &_cl) : cl(_cl)
	{
		CCOMMAND(addproj, "siiiii", (sspweapons *self, const char *s, int *u, int *w, int *x, int *y, int *z), self->projs.add(new proj(s, *u, *w, *x, *y, *z)));
	}
	
	void addproj(int p, sspent *d)
	{
		if(projs.inrange(p))
			projectiles.add(new projectile(p, vec(d->o.x, d->o.y, d->o.z + d->eyeheight), d->yaw, d->pitch));
	}
	
	void clearprojs()
	{
		projectiles.setsize(0);
	}

	void updateprojs()
	{
		loopv(projectiles)
		{ //do simplistic flying first, add gravity effects, timers, and so forth, later
			projectile &p = *projectiles[i];
			vec v = p.d;
			v.mul(projs[p.prj]->speed); //assume nothing silly happened to bork this and cause a crash
			p.o.add(v);
		}
	}
	
	void renderprojs()
	{
		loopv(projectiles)
		{
			projectile &p = *projectiles[i];
			float yaw, pitch;
			vectoyawpitch(p.d, yaw, pitch);
			
			rendermodel(NULL, projs[p.prj]->mdl, ANIM_MAPMODEL|ANIM_LOOP, p.o, yaw, pitch, MDL_SHADOW);
		}
	};
	
	void dynlights()
	{
	
	}
};

