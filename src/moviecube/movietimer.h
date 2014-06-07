enum { TIMER_NONE, TIMER_RUNNING, TIMER_PAUSE };

struct movietimer
{
  struct timestamp
  {
    int millis; //time between scripts
    string command; //command to execute
  };

  movieclient &cl;
  
  vector<timestamp> tl;
  int idx;
  int lastidxtime;
  int status;
  int starttime;
	
  movietimer(movieclient &_cl) : cl(_cl), idx(-1), lastidxtime(0), status(TIMER_NONE), starttime(0)
  {
    CCOMMAND(timerstart, "", (movietimer *self), self->start() );    
    CCOMMAND(timerstop, "", (movietimer *self), self->stop() );    
    CCOMMAND(timerappend, "si", (movietimer *self, char* s, int *millis), self->append(s, *millis) );    
    CCOMMAND(timeroverwrite, "si", (movietimer *self, char* s, int *millis), self->overwrite(s, *millis) );    
    CCOMMAND(timersplit, "si", (movietimer *self, char* s, int *millis), self->split(s, *millis) );    
    CCOMMAND(timerremove, "i", (movietimer *self, int *millis), self->remove(*millis) );    
    CCOMMAND(timercut, "i", (movietimer *self, int *millis), self->cut(*millis) );
    CCOMMAND(timerclear, "i", (movietimer *self), self->clear() );        
  }
  
  void start() { status = TIMER_RUNNING; idx = 0; lastidxtime = starttime = lastmillis;}
  void stop() {}
  void reset() {}
	
  void append(char *s, int millis) 
  {
    totalmillis += millis;
    timestamp* stamp = &tl.add();
    stamp->millis = millis;
    s_strcpy (stamp->command, s);
  }
  
  void overwrite(char *s, int millis) {}
  void split(char *s, int millis) {}
  void remove(int millis) {}
  void cut(int millis) {}
  void clear () { tl.setsize(0); };
	int getcurtime() { return curtime; }
  void debug(bool on) {}
  
  void update()
  {
    if (status == TIMER_RUNNING) 
    {
			curtime = lastmillis - starttime;
      if ( tl[idx].millis <= (lastmillis - lastidxtime) )
      {
        execute(tl[idx].command);
        if (tl.length() <= idx) { status = TIMER_NONE; return; }
        idx++;
        lastidxtime = lastmillis;
      }
    }
		if (status == TIMER_PAUSE)
		{
			//ToDo: Pause
		}
  }
};
