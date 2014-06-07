struct movierender
{      
	movieclient &cl;
	
	movierender(movieclient &_cl) : cl(_cl) {}
		
  //ToDo preload playermodels
  void preloadplayermodels()
  {
    loadmodel(DEFAULT_PLAYER_MODEL, -1, true);
  }
  
	void renderplayer(movieent *d) 
	{
		if(!d->mdl) { loadmodel(d->mdl, -1, true); }
		renderclient(d, d->mdl ? d->mdl : DEFAULT_PLAYER_MODEL, NULL, ANIM_SHOOT, 0, 0, 0);
	}
	
	void rendergame(int gamemode)
	{
		startmodelbatches();
		
		movieent *d;
				
		loopv(cl.players) if((d = cl.players[i]) && d->state!=CS_SPECTATOR && d->state!=CS_SPAWNING)
		{
			if (cl.player1->state==CS_EDITING)
				particle_text(d->abovehead(), d->name,11,1);
			renderplayer(d);
		}
		if(isthirdperson()) { renderplayer(cl.player1); }
		
		//~ cl.mo.render(); //moveables
		cl.et.renderentities();
    
		endmodelbatches();
	}
};
