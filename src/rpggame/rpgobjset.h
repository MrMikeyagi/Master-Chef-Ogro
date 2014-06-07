struct rpgobjset
{
    rpgclient &cl;

    vector<rpgobj *> set, stack;
    hashtable<char *, char *> names;
    
    rpgobj *pointingat;
    rpgobj *playerobj;
    rpgobj *selected;
    
    rpgquest *quests;
    rpgquest *currentquest;
    
    rpgobjset(rpgclient &_cl) : cl(_cl), pointingat(NULL), playerobj(NULL), selected(NULL), quests(NULL), currentquest(NULL)
    {
        #define N(n) CCOMMAND(r_##n,     "i", (rpgobjset *self, int *val), { self->stack[0]->s_##n = *val; }); \
                     CCOMMAND(r_get_##n, "",  (rpgobjset *self), { intret(self->stack[0]->s_##n); }); 
                     
        RPGNAMES 
        #undef N
        #define N(n) CCOMMAND(r_def_##n, "ii", (rpgobjset *self, int *i1, int *i2), { stats::def_##n(*i1, *i2); }); \
                     CCOMMAND(r_eff_##n, "",   (rpgobjset *self), { intret(self->stack[0]->eff_##n()); }); 
        RPGSTATNAMES 
        #undef N
        
        CCOMMAND(r_model,       "s",   (rpgobjset *self, char *s), { self->stack[0]->model = self->stringpool(s); });
        CCOMMAND(r_spawn,       "s",   (rpgobjset *self, char *s), { self->spawn(self->stringpool(s)); });
        CCOMMAND(r_contain,     "s",   (rpgobjset *self, char *s), { self->stack[0]->decontain(); self->stack[1]->add(self->stack[0], atoi(s)); });
        CCOMMAND(r_pop,         "",    (rpgobjset *self), { self->popobj(); });
        CCOMMAND(r_swap,        "",    (rpgobjset *self), { swap(self->stack[0], self->stack[1]); });
        CCOMMAND(r_say,         "s",   (rpgobjset *self, char *s), { self->stack[0]->abovetext = self->stringpool(s); });
        CCOMMAND(r_quest,       "ss",  (rpgobjset *self, char *s, char *a), { self->stack[0]->addaction(self->stringpool(s), self->stringpool(a), true); });
        CCOMMAND(r_action,      "ss",  (rpgobjset *self, char *s, char *a), { self->stack[0]->addaction(self->stringpool(s), self->stringpool(a), false); });
        CCOMMAND(r_action_use,  "s",   (rpgobjset *self, char *s), { self->stack[0]->action_use.script = self->stringpool(s); });
        CCOMMAND(r_take,        "sss", (rpgobjset *self, char *name, char *ok, char *notok), { self->takefromplayer(name, ok, notok); });
        CCOMMAND(r_give,        "s",   (rpgobjset *self, char *s), { self->givetoplayer(s); });
        CCOMMAND(r_use,         "",    (rpgobjset *self), { self->stack[0]->selectuse(); });
	CCOMMAND(r_get_name,    "",    (rpgobjset *self), { if (self->stack[0]->name) result(self->stack[0]->name); });
	CCOMMAND(r_name,	"s",    (rpgobjset *self, char *a), { self->stack[0]->name = self->stringpool(a);});
        CCOMMAND(r_applydamage, "i",   (rpgobjset *self, int *d), { self->stack[0]->takedamage(*d, *self->stack[1]); });
        clearworld();
    }
    
    void resetstack()
    {
        stack.setsize(0);
        loopi(10) stack.add(playerobj);     // determines the stack depth    
    }
    
    void clearworld()
    {
        if(playerobj) { playerobj->ent = NULL; delete playerobj; }
        playerobj = new rpgobj("player", *this);
        playerobj->ent = &cl.player1;
        cl.player1.ro = playerobj;

        pointingat = NULL;
        set.deletecontentsp();
        resetstack();
        
        playerobj->scriptinit();            // will fail when this is called from emptymap(), which is ok
    }
    
    void removefromsystem(rpgobj *o)
    {
        removefromworld(o);
        o->decontain();
        if(pointingat==o) pointingat = NULL;
        if(selected==o) selected = NULL;
        resetstack();
        DELETEP(o);
    }
    
    void update()
    {
        extern vec worldpos;
        pointingat = NULL;
        loopv(set)
        {
            set[i]->update();

            float dist = cl.player1.o.dist(set[i]->ent->o);
            if(dist<50 && intersect(set[i]->ent, cl.player1.o, worldpos) && (!pointingat || cl.player1.o.dist(pointingat->ent->o)>dist))    
            {    
                pointingat = set[i]; 
            }
        }
    }
    
    void spawn(char *name)
    {
        rpgobj *o = new rpgobj(name, *this);
        pushobj(o);
        o->scriptinit();
    }
    
    void placeinworld(vec &pos, float yaw)
    {
        stack[0]->placeinworld(new rpgent(stack[0], cl, pos, yaw));
        set.add(stack[0]);
    }
    
    void pushobj(rpgobj *o) { stack.pop(); stack.insert(0, o); }       // never overflows, just removes bottom
    void popobj()           { stack.add(stack.remove(0)); }            // never underflows, just puts it at the bottom
    
    void removefromworld(rpgobj *worldobj)
    {
        set.removeobj(worldobj);
        DELETEP(worldobj->ent);    
    }
    
    void take(rpgobj *worldobj, rpgobj *newowner)
    {
        removefromworld(worldobj);
        newowner->add(worldobj, false);
    }
    
    void takefromplayer(char *name, char *ok, char *notok)
    {
        rpgobj *o = playerobj->take(name);
        if(o)
        {
            stack[0]->add(o, false);
            conoutf("\f2you hand over a %s", o->name);
            if(currentquest)
            {
                conoutf("\f2you finish a quest for %s", currentquest->npc); 
                currentquest->completed = true;           
            }
        }
        execute(o ? ok : notok);
    }
    
    void givetoplayer(char *name)
    {
        rpgobj *o = stack[0]->take(name);
        if(o)
        {
            conoutf("\f2you receive a %s", o->name);
            playerobj->add(o, false);
        }
    }
    
    void addquest(rpgaction *a, const char *questline, const char *npc)
    {
        a->q = quests = new rpgquest(quests, npc, questline);
        conoutf("\f2you have accepted a quest for %s", npc);
    }
    
    void listquests(bool completed, g3d_gui &g)
    {
        for(rpgquest *q = quests; q; q = q->next) if(q->completed==completed)
        {
            s_sprintfd(info)("%s: %s", q->npc, q->questline);
            g.text(info, 0xAAAAAA, "info");
        }
    }
    
    char *stringpool(char *name)
    {
        char **n = names.access(name);
        if(n) return *n;
        name = newstring(name);
        names[name] = name;
        return name;
    }
    
    void render()       { loopv(set) set[i]->render();   }
    void g3d_npcmenus() { loopv(set) set[i]->g3d_menu(); } 
};
