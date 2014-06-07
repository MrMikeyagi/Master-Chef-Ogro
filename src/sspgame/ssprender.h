struct ssprender
{
	sspclient &cl;
	ssprender(sspclient &_cl) : cl(_cl) {}
	
	void renderplayer(sspent *d, const char *mdl)
	{
		int lastaction = d->lastaction,
			anim = ANIM_ATTACK1+d->gunselect,
			delay = 300,
			hold = ANIM_HOLD1|ANIM_LOOP;
			
		if(cl.intermission && d->state!=CS_DEAD)
		{
			if (cl.secsremain)
				hold = anim = ANIM_WIN|ANIM_LOOP;
			else
				hold = anim = ANIM_LOSE|ANIM_LOOP;
		}
		modelattach a[4];
		int ai = 0;
		if(d->state==CS_ALIVE)
		{
			if(cl.armourpickups.inrange(d->gunselect))
			{
				a[ai].m = NULL;
				a[ai].name = cl.weaponpickups[d->gunselect]->attachmdl;
				a[ai].tag = "tag_weapon";
				a[ai].anim = ANIM_VWEP_IDLE|ANIM_LOOP;
				a[ai].basetime = 0;
				ai++;
			}
			if(cl.armourpickups.inrange(d->armourvec))
			{
				a[ai].m = NULL;
				a[ai].name = cl.armourpickups[d->armourvec]->attachmdl;
				a[ai].tag = "tag_shield";
				a[ai].anim = ANIM_VWEP_IDLE|ANIM_LOOP;
				a[ai].basetime = 0;
				ai++;	
			}
			if(d->powerup>=0)
			{
				//TODO
				//ai++;
			}
		}
		a[ai].name = NULL;
		
		if(d->lastpain + 2000 > lastmillis && (lastmillis % 100) > 50) return; //flicker if hurt
		renderclient(d, mdl, a[0].tag ? a : NULL, hold, anim, delay, lastaction, cl.intermission ? 0 : d->lastpain, true);
	}
	
	void rendergame(bool mainpass)
	{
		startmodelbatches();
		if(isthirdperson()) renderplayer(cl.player1, "rc");
		cl.sm.render();
		cl.et.renderentities();
		cl.wp.renderprojs();
		endmodelbatches();
	}
};
