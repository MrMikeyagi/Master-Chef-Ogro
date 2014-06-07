//ToDo: add actorstop
//ToDo: actorreset should spawn actor at spawnpoint too

//actor actions:
enum { MA_END = -1, MA_IDLE, MA_MODEL, MA_SPEED, MA_ANIM, MA_FLY, MA_MOVE, MA_TP, MA_LOOK , MA_RUNSCRIPT };

//actor cue states:
enum { AC_EMPTY = -1, AC_READY, AC_RUNNING, AC_RESET };

struct movieactorset
{	
  
  struct actiontypeidle
  { 
    int millis;
    int begin;
  };
  
  struct actiontypemodel 
  { 
    char name[MAXMODELLEN]; 
  };
  
  struct actiontypespeed 
  { 
    int speed; 
  };
  
  struct actiontypeanim
  { 
    int idx;
  };
  
  struct actiontypefly 
  { 
    bool on; 
  };
  
  struct actiontypemove 
  { 
    int waypoint; 
  };
  
  struct actiontypetp 
  { 
    int waypoint; 
  };
  
  struct actiontypelook 
  { 
    int waypoint;
  };

  struct actiontypescript 
  {
    char *str;
  };
  
  union actiontypes
  {
    actiontypeidle idle;
    actiontypemodel mdl;
    actiontypespeed speed;
    actiontypefly fly;
    actiontypeanim anim;
    actiontypemove move;
    actiontypetp tp;
    actiontypelook look;
    actiontypescript script;
  };
  
  struct movieaction {
    int type;
    actiontypes action;
  };
  
  struct movieactorcue
  {
    vector<movieaction> actions;
    hashtable<int, char*> handler;
    int owner;
    int state;
    int spawn;
    
    movieactorcue() : owner(-1), state(AC_EMPTY) {};
    movieaction &push() { return actions.add(); }
    void pop() { actions.remove(0); }
  };
  
  movieclient &cl;
  hashtable<char *, movieactorcue> cues;  //actual actor cues actors : cues
  
	movieactorset(movieclient &_cl) : cl(_cl) 
  {
    CCOMMAND(addactor, "is", (movieactorset *self, int *sp, char *s), self->addactor(*sp, s) );    
    CCOMMAND(delactor, "s", (movieactorset *self, char *s), self->deleteactor(s) );    
    CCOMMAND(resetactor, "s", (movieactorset *self, char *s), self->resetactor(s) );
    CCOMMAND(sighandler, "sis",  (movieactorset *self, char *actor, int *sig, char *str), self->sighandler(actor,*sig,str) );    
    CCOMMAND(sigsend, "si",  (movieactorset *self, char *s, int *val), self->sigsend(s,*val) );

    //commands that store actions to the actor cue
    CCOMMAND(actoridle, "si", (movieactorset *self, char *s, int *d), self->pushidle(s, *d) );
    CCOMMAND(actormove, "si", (movieactorset *self, char *s, int *d), self->pushmove(s, *d) );
    CCOMMAND(actormodel, "ss", (movieactorset *self, char *s, char *t), self->pushmodel(s, t) );
    CCOMMAND(actorspeed, "si", (movieactorset *self, char *s, int *d), self->pushspeed(s, *d) );
    CCOMMAND(actorfly, "si", (movieactorset *self, char *s, bool *b), self->pushfly(s, *b) );
    CCOMMAND(actorlook, "si", (movieactorset *self, char *s, int *d), self->pushlook(s, *d) );
    CCOMMAND(actortp, "si", (movieactorset *self, char *s, int *d), self->pushtp(s, *d) );
    CCOMMAND(actoranim, "si", (movieactorset *self, char *s, int *d), self->pushanim(s, *d) );

    CCOMMAND(actorscript, "ss", (movieactorset *self, char *s, char *t), self->pushscript(s, t) );
    

    //commands for cue
    CCOMMAND(listcue, "s", (movieactorset *self, char *s), self->listcue(s) );
    CCOMMAND(go, "s", (movieactorset *self, char *s), self->go(s) );    
  }
  		
	//add a new actor 
  void addactor(int sp, char *s) 
  {
    if ( cl.et.findent(ACTOR, ATTR1, sp) < 0) { conoutf ("\f2create a valid actor ent first use newactor"); return; }
		if(strlen(s) < 1) { conoutf ("\f2dont forget to give your actor a name"); execute("delent"); return; }
		char *key = newstring(s);
    if ( cues.access(key) ) 
    { 
      conoutf ("\f2error addactor: actor key %s already exist", s);
      execute("delent");
      return; 
    }
    movieactorcue *cue = &cues[key];
    cue->owner = cl.player1->clientnum;
    cue->state = AC_EMPTY;
    cue->spawn = sp;
    cl.cc.addmsg(SV_ADDACTOR, "ris", sp, key);
  }
  
  movieactorcue* getcue(const char* s)
  {
    char *key = newstring(s);
    if ( ! cues.access(key) ) { return NULL; }
    movieactorcue *cue = &cues[key];
    if (cue->owner == cl.player1->clientnum) { return cue; }
    return NULL;
  }

  void resetactor(char *s)
  {
    movieactorcue* cue = getcue(s);
    if ( !cue ) { conoutf ("\f2error resetactor: actor key not found or you are not the owner of %s", s);  return; }
    cue->state = AC_RESET;
    while (!cue->actions.empty()) { cue->pop(); }
  }

  void sigsend(char *s, int sig)
  {
    cl.cc.addmsg(SV_ACTORSIG, "rsi", s, sig);
  }
  
  void sighandler(char *s, int sig, char *str) 
  {
    movieactorcue* cue = getcue(s);
    if ( !cue ) { conoutf ("\f2error sighandler: actor key not found or you are not the owner of %s", s);  return; }
    str = newstring (str);
    cue->handler[sig] = str;
  }
  
  void sigint(char *s, int sig)
  {
    movieactorcue* cue = getcue(s);
    if ( cue ) 
    { 
      if (cue->handler.access(sig))
      {
        char *handler = cue->handler[sig];
        s_sprintfd(t)("%s %s", handler, s);
        execute(t);
      }
    }
  }

  void deletecue(char* s) //cleanup / disconnect
  {
    char *key = newstring(s);
    if ( ! cues.access(key) ) { conoutf ("\f2error delcue: actor key: %s not found", s); return; }
    movieactorcue *cue = &cues[key];
    while (!cue->actions.empty()) { cue->pop(); }
    cues.remove(key);
    s_sprintfd(t)("m_cmd_del_action %s", key);
    execute(t);

  }
  
	void deleteactor(char* s) 
  {
    movieactorcue* cue = getcue(s);
    if ( !cue ) { conoutf ("\f2error delactor: actor key not found or you are not the owner of %s", s); return; }
    char *key = newstring(s);

    cue->state = AC_EMPTY;
    //ToDo: add a menu to choose if deleted or not (important for saving stuff later)
    s_sprintfd(t)("cancelsel; entfind actor %d; delent; ", cue->spawn);
    execute(t);    
    
    loopv(cl.players) if (cl.players[i] && cl.players[i]->scripted == true && strcmp (cl.players[i]->name, key) == 0)
    {
      cl.cc.addmsg(SV_DELACTOR, "ri", i);
      break;
    }
    deletecue(s);
  }
  

  void pushidle(char *s, int d) 
  {
    movieactorcue* cue = getcue(s);
    if ( !cue ) { conoutf ("\f2error actoridle: actor key not found or you are not the owner of %s", s);  return; }
    movieaction &a = cue->push();
    a.type = MA_IDLE;
    a.action.idle.millis = d;
    a.action.idle.begin = -1;
    cue->state = AC_READY;
  }
  
  void pushmove(char *s, int d)
  {
    movieactorcue* cue = getcue(s);
    if ( !cue ) { conoutf ("\f2error actormove: actor key not found or you are not the owner of %s", s); return; }

    int i = cl.et.findent(WAYPOINT, ATTR1, d);
    if ( i < 0 ) { conoutf ("\f2error actormove: waypoint %d not found", d); return; }
    
    movieaction &a = cue->push();
    a.type = MA_MOVE;      
    a.action.move.waypoint = i;
    cue->state = AC_READY;
  }

  void pushtp(char *s, int d)
  {
    movieactorcue* cue = getcue(s);
    if ( !cue ) { conoutf ("\f2error actortp: actor key not found or you are not the owner of %s", s);  return; }

    int i = cl.et.findent(WAYPOINT, ATTR1, d);
    if ( i < 0 ) { conoutf ("\f2error actortp: waypoint %d not found", d); return; }

    movieaction &a = cue->push();
    a.type = MA_TP;
    a.action.tp.waypoint = i;
    cue->state = AC_READY;
  }
  
  void pushlook(char *s, int d)
  {
    movieactorcue* cue = getcue(s);
    if ( !cue ) { conoutf ("\f2error actorlook: actor key not found or you are not the owner of %s", s);  return; }
    int i = cl.et.findent(WAYPOINT, ATTR1, d);
    if ( i < 0 ) { conoutf ("\f2error actorlook: waypoint %d not found", d); return; }

    movieaction &a = cue->push();
    a.type = MA_LOOK;
    a.action.look.waypoint = i;
    cue->state = AC_READY;
  }
  
  void pushspeed(char *s, int d)
  {
    movieactorcue* cue = getcue(s);
    if ( !cue ) { conoutf ("\f2error actorspeed: actor key not found or you are not the owner of %s", s); return; }
    movieaction &a = cue->push();
    a.type = MA_SPEED;
    d<MINCLIENTSPEED?a.action.speed.speed=1:d>MAXCLIENTSPEED?a.action.speed.speed=MAXCLIENTSPEED:a.action.speed.speed=d;
    cue->state = AC_READY;
  }

  void pushfly(char *s, bool b)
  {
    movieactorcue* cue = getcue(s);
    if ( !cue ) { conoutf ("\f2error actorfly: actor key not found or you are not the owner of %s", s);  return; }
    movieaction &a = cue->push();
    a.type = MA_FLY;
    a.action.fly.on = b;
    cue->state = AC_READY;
  }
  
  void pushmodel(const char *s, char *m) 
  {
    movieactorcue* cue = getcue(s);
    if ( !cue ) { conoutf ("\f2error actormodel: actor key not found or you are not the owner of %s", s);  return; }
    movieaction &a = cue->push();
    a.type = MA_MODEL;
    strncpy(a.action.mdl.name, m, MAXMODELLEN);
    cue->state = AC_READY;
  }

  void pushanim(const char *s, int d) 
  {
    movieactorcue* cue = getcue(s);
    if ( !cue ) { conoutf ("\f2error actormodel: actor key not found or you are not the owner of %s", s);  return; }
    movieaction &a = cue->push();
    a.type = MA_ANIM;
    a.action.anim.idx = d;
    cue->state = AC_READY;
  }

  void pushscript(char *s, char *t)
  {
    movieactorcue* cue = getcue(s);
    if ( !cue ) { conoutf ("\f2error actorscript: actor key not found or you are not the owner of %s", s); return; }
    movieaction &a = cue->push();
    a.type = MA_RUNSCRIPT;
    a.action.script.str = newstring(t);
    cue->state = AC_READY;
  }
  
  void listcue(const char *s) 
  {
    movieactorcue* cue = getcue(s);
    if ( !cue ) { conoutf ("\f2error listcue: actor key not found or you are not the owner of %s", s); return; }
    conoutf("actor: %s owned by client: %d", s, cue->owner);
    conoutf("--------------------------------------------");
    loopv(cue->actions) {
      switch (cue->actions[i].type)
      {
        case MA_MOVE:
          conoutf("move to waypoint %d", cue->actions[i].action.move.waypoint); break;
        case MA_IDLE:
          conoutf("wait for %d milliseconds", cue->actions[i].action.idle.millis); break;
        case MA_SPEED:
          conoutf("set speed to %d", cue->actions[i].action.speed.speed); break;
        case MA_FLY:
          conoutf("flying %d", cue->actions[i].action.fly.on); break;
        case MA_TP:
          conoutf("teleport to waypoint %d", cue->actions[i].action.tp.waypoint); break;
        case MA_LOOK:
          conoutf("look into direction of entity %d", cue->actions[i].action.look.waypoint); break;
        case MA_MODEL:
          conoutf("change model to %s", cue->actions[i].action.mdl.name); break;
        case MA_ANIM:
          conoutf("play pose animation %d", cue->actions[i].action.anim.idx); break;
        case MA_RUNSCRIPT:
          conoutf("runs %s as script", cue->actions[i].action.script.str); break;
        default:
          conoutf ("NOTHING"); break;
      }
    }
  }

  void go(const char *s) 
  {
    movieactorcue* cue = getcue(s);
    if ( !cue ) { conoutf ("\f2error go: actor key not found or you are not the owner of %s", s); return; }
    if (cue->state > AC_EMPTY) cue->state = AC_RUNNING;
  }
  
	//update all actors
  void update() 
  {
    loopv(cl.players) if(cl.players[i]) if( cl.players[i]->scripted == true )
    {
      movieent *d = cl.players[i];
      d->lastupdate = lastmillis;
      movieactorcue* cue = getcue(cl.players[i]->name);
      
      if (cue) 
      { 
        switch(cue->state) 
        {
          case AC_RUNNING:
          {
            process(d, cue);
            break;
          }
          case AC_READY:
          case AC_EMPTY:
          {
            d->move = d->strafe = 0;
            break;
          }
          case AC_RESET:
          {
            int e = cl.et.findent(ACTOR, ATTR1, cue->spawn);
						vec floor = vec(cl.et.ents[e]->o);
						droptofloor (floor, 1, 0);
						d->o = floor.add(vec(0,0,d->eyeheight));
            d->move = 0;
            cue->state = AC_EMPTY;
            break;
          }
        }
      }
    }
  }
  //ToDo Cuepause / Clock
  //process events/signal between actors
  void process (movieent* d, movieactorcue* cue) 
  {
    movieaction& a = cue->actions[0];
    switch (a.type)
    {
      case MA_IDLE:
      {
        if (a.action.idle.begin < 0) { a.action.idle.begin = lastmillis; break; }
        if ( (lastmillis - a.action.idle.begin) > a.action.idle.millis ) 
        { 
          cue->pop();
          a.action.idle.begin = -1;
        }
        break;
      }
      case MA_SPEED:
      {
        d->maxspeed = a.action.speed.speed;
        cue->pop();
        break;
      }
      case MA_ANIM:
      {
        d->animation = a.action.anim.idx;
        d->animate = true;
        cue->pop();
        break;
      }
      case MA_FLY:
      {
        conoutf("actor can fly %d", a.action.fly.on);
        d->flying = a.action.fly.on;
        cue->pop();
        break;
      }
      case MA_MOVE:
      {
        vec target = cl.et.ents[a.action.move.waypoint]->o;
        float dist = target.dist(vec(d->o));
        if (dist < 15) 
        {
          d->move = d->strafe = 0;
          cue->pop(); 
          break; 
        }
        vec dir(target);
        dir.sub(d->o);
        dir.normalize();
        d->move = 1;
        if (d->flying) { vectoyawpitch(dir, d->yaw, d->pitch); }
        else { float dummy; vectoyawpitch(dir, d->yaw, dummy); }
        //~ vectoyawpitch(dir, d->yaw, dummy);
        break;
      }
      case MA_TP:
			{
				vec floor = vec(cl.et.ents[a.action.move.waypoint]->o);
				droptofloor (floor, 1, 0);
				d->o = floor.add(vec(0,0,d->eyeheight));
        cue->pop();
        break;
			}
      case MA_LOOK:
      {
        vec target = cl.et.ents[a.action.look.waypoint]->o;
        vec dir (target);
        dir.sub(d->o);
        vectoyawpitch(dir, d->yaw, d->pitch);
        cue->pop();
        break;
      }
      case MA_MODEL:
      {
        //ToDo add send to server, preload model
        if(loadmodel (a.action.mdl.name))
        {
          strncpy(d->mdl, a.action.mdl.name, MAXMODELLEN+1);
					setbbfrommodel (d, d->mdl); 
        }
        else
        {
          conoutf ("could not load actormodel");
        }
        cue->pop();
        break;
      }
      case MA_RUNSCRIPT:
      {
        s_sprintfd(s)("%s %s", a.action.script.str, d->name);
        execute(s);
        cue->pop();
        break;
      }
      default:
        break;
    }
    if( cue->actions.empty()) { cue->state = AC_EMPTY; }
  }
};
