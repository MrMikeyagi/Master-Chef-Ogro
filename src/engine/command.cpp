// command.cpp: implements the parsing and execution of a tiny script language which
// is largely backwards compatible with the quake console language.

#include "pch.h"
#ifdef STANDALONE
#include "cube.h"
#include "iengine.h"
#else
#include "engine.h"
#endif

VAR(confver, 1, CONFVERSION, -1);
VAR(ignoreconfver, 0, 0, 1);
char *exchangestr(char *o, const char *n) { delete[] o; return newstring(n); }

identtable *idents = NULL;        // contains ALL vars/commands/aliases

bool overrideidents = false, persistidents = true, worldidents = false;

void clearstack(ident &id)
{
    identstack *stack = id.stack;
    while(stack)
    {
        delete[] stack->action;
        identstack *tmp = stack;
        stack = stack->next;
        delete tmp;
    }
    id.stack = NULL;
}

void clear_command()
{
    enumerate(*idents, ident, i, if(i.type==ID_ALIAS) { DELETEA(i.name); DELETEA(i.action); if(i.stack) clearstack(i); });
    if(idents) idents->clear();
}

void clearoverrides()
{
    enumerate(*idents, ident, i,
        if(i.override!=NO_OVERRIDE)
        {
            switch(i.type)
            {
                case ID_ALIAS: 
                    if(i.action[0])
                    {
                        if(i.action != i.isexecuting) delete[] i.action;
                        i.action = newstring("");
                    }
                    break;
                case ID_VAR: 
                    *i.storage.i = i.overrideval.i;
                    i.changed();
                    break;
                case ID_FVAR:
                    *i.storage.f = i.overrideval.f;
                    i.changed();
                    break;
                case ID_SVAR:
                    delete[] *i.storage.s;
                    *i.storage.s = i.overrideval.s;
                    i.changed();
                    break;
            }
            i.override = NO_OVERRIDE;
        });
}

void pushident(ident &id, char *val)
{
    if(id.type != ID_ALIAS) return;
    identstack *stack = new identstack;
    stack->action = id.isexecuting==id.action ? newstring(id.action) : id.action;
    stack->next = id.stack;
    id.stack = stack;
    id.action = val;
}

void popident(ident &id)
{
    if(id.type != ID_ALIAS || !id.stack) return;
    if(id.action != id.isexecuting) delete[] id.action;
    identstack *stack = id.stack;
    id.action = stack->action;
    id.stack = stack->next;
    delete stack;
}

ident *newident(const char *name)
{
    ident *id = idents->access(name);
    if(!id)
    {
        ident init(ID_ALIAS, newstring(name), newstring(""), persistidents ? IDF_PERSIST : 0);
        id = &idents->access(init.name, init);
    }
    return id;
}

void pusha(const char *name, char *action)
{
    pushident(*newident(name), action);
}

void push(char *name, char *action)
{
    pusha(name, newstring(action));
}

void pop(char *name)
{
    ident *id = idents->access(name);
    if(id) popident(*id);
}

COMMAND(push, "ss");
COMMAND(pop, "s");

SVAR(SV_MESG19, "");

void aliasa(const char *name, char *action, int type = 0)
{
    ident *b = idents->access(name);
    if(!b) 
    {
        ident b(ID_ALIAS, newstring(name), action, persistidents ? (worldidents ? IDF_PERSIST|IDF_WORLD : IDF_PERSIST) : type);
        if(overrideidents || worldidents || b.flags&IDF_WORLD) b.override = OVERRIDDEN;
        idents->access(b.name, b);
    }
    else if(b->type != ID_ALIAS)
    {
        conoutf(CON_ERROR, "%s (%s)", SV_MESG19, name);
        delete[] action;
    }
    else 
    {
        if(b->action != b->isexecuting) delete[] b->action;
        b->action = action;
        if(overrideidents || worldidents || b->flags&IDF_WORLD) b->override = OVERRIDDEN;
        else 
        {
            if(b->override != NO_OVERRIDE) b->override = NO_OVERRIDE;
            if(persistidents)
            {
                if(!(b->flags & IDF_PERSIST)) b->flags |= IDF_PERSIST;
            }
            else if(b->flags & IDF_PERSIST) b->flags &= ~IDF_PERSIST;
        }
    }
}

void alias(const char *name, const char *action) { aliasa(name, newstring(action)); }
void walias(const char *name, const char *action) { aliasa(name, newstring(action), IDF_WORLD|IDF_PERSIST); }

COMMAND(alias, "ss");
COMMAND(walias, "ss");

// variable's and commands are registered through globals, see cube.h

int variable(const char *name, int min, int cur, int max, int *storage, void (*fun)(), int flags)
{
    if(!idents) idents = new identtable;
    ident v(ID_VAR, name, min, cur, max, storage, (void *)fun, flags);
    idents->access(name, v);
    return cur;
}

float fvariable(const char *name, float min, float cur, float max, float *storage, void (*fun)(), int flags)
{
    if(!idents) idents = new identtable;
    ident v(ID_FVAR, name, min, cur, max, storage, (void *)fun, flags);
    idents->access(name, v);
    return cur;
}

char *svariable(const char *name, const char *cur, char **storage, void (*fun)(), int flags)
{
    if(!idents) idents = new identtable;
    ident v(ID_SVAR, name, newstring(cur), storage, (void *)fun, flags);
    idents->access(name, v);
    return v.val.s;
}

#define _GETVAR(id, vartype, name, retval) \
    ident *id = idents->access(name); \
    if(!id || id->type!=vartype) return retval;
#define GETVAR(id, name, retval) _GETVAR(id, ID_VAR, name, retval)
void setvar(const char *name, int i, bool dofunc) 
{ 
    GETVAR(id, name, );
    *id->storage.i = clamp(i, id->minval, id->maxval); 
    if(dofunc) id->changed();
} 
void setfvar(const char *name, float f, bool dofunc)
{
    _GETVAR(id, ID_FVAR, name, );
    *id->storage.f = clamp(f, id->minvalf, id->maxvalf);
    if(dofunc) id->changed();
}
void setsvar(const char *name, const char *str, bool dofunc)
{
    _GETVAR(id, ID_SVAR, name, );
    *id->storage.s = exchangestr(*id->storage.s, str);
    if(dofunc) id->changed();
}
int getvar(const char *name) 
{ 
    GETVAR(id, name, 0);
    return *id->storage.i;
}
int getvarmin(const char *name) 
{ 
    GETVAR(id, name, 0);
    return id->minval;
}
int getvarmax(const char *name) 
{ 
    GETVAR(id, name, 0);
    return id->maxval;
}
bool identexists(const char *name) { return idents->access(name)!=NULL; }
ident *getident(const char *name) { return idents->access(name); }

void touchvar(const char *name)
{
    ident *id = idents->access(name);
    if(id) switch(id->type)
    {
        case ID_VAR:
        case ID_FVAR:
        case ID_SVAR:
            id->changed();
            break;
    }
}

const char *getalias(const char *name)
{
    ident *i = idents->access(name);
    return i && i->type==ID_ALIAS ? i->action : "";
}

bool addcommand(const char *name, void (*fun)(), const char *narg)
{
    if(!idents) idents = new identtable;
    ident c(ID_COMMAND, name, narg, (void *)fun);
    idents->access(name, c);
    return false;
}

void addident(const char *name, ident *id)
{
    if(!idents) idents = new identtable;
    idents->access(name, *id);
}

static vector<vector<char> *> wordbufs;
static int bufnest = 0;

char *parseexp(const char *&p, int right);

void parsemacro(const char *&p, int level, vector<char> &wordbuf)
{
    int escape = 1;
    while(*p=='@') p++, escape++;
    if(level > escape)
    {
        while(escape--) wordbuf.add('@');
        return;
    }
    if(*p=='(')
    {
        char *ret = parseexp(p, ')');
        if(ret)
        {
            for(char *sub = ret; *sub; ) wordbuf.add(*sub++);
            delete[] ret;
        }
        return;
    }
    static vector<char> ident;
    ident.setsizenodelete(0);
    while(isalnum(*p) || *p=='_') ident.add(*p++);
    ident.add(0);
    const char *alias = getalias(ident.getbuf());
    while(*alias) wordbuf.add(*alias++);
}

SVAR(SV_MESG20, "");

char *parseexp(const char *&p, int right)          // parse any nested set of () or []
{
    if(bufnest++>=wordbufs.length()) wordbufs.add(new vector<char>);
    vector<char> &wordbuf = *wordbufs[bufnest-1];
    int left = *p++;
    for(int brak = 1; brak; )
    {
        int c = *p++;
        if(c=='\r') continue;               // hack
        if(left=='[' && c=='@')
        {
            parsemacro(p, brak, wordbuf);
            continue;
        }
        if(c=='\"')
        {
            wordbuf.add(c);
            const char *end = p+strcspn(p, "\"\r\n\0");
            while(p < end) wordbuf.add(*p++);
            if(*p=='\"') wordbuf.add(*p++);
            continue;
        }
        if(c=='/' && *p=='/')
        {
            p += strcspn(p, "\n\0");
            continue;
        }
            
        if(c==left) brak++;
        else if(c==right) brak--;
        else if(!c) 
        { 
            p--;
            conoutf(CON_ERROR, "%s \"%c\"", SV_MESG20, right);
            wordbuf.setsize(0); 
            bufnest--;
            return NULL; 
        }
        wordbuf.add(c);
    }
    wordbuf.pop();
    char *s;
    if(left=='(')
    {
        wordbuf.add(0);
        char *ret = executeret(wordbuf.getbuf());                    // evaluate () exps directly, and substitute result
        wordbuf.pop();
        s = ret ? ret : newstring("");
    }
    else
    {
        s = newstring(wordbuf.getbuf(), wordbuf.length());
    }
    wordbuf.setsize(0);
    bufnest--;
    return s;
}

SVAR(SV_MESG21, "");

char *lookup(char *n)                           // find value of ident referenced with $ in exp
{
    ident *id = idents->access(n+1);
    if(id) switch(id->type)
    {
        case ID_VAR: { s_sprintfd(t)("%d", *id->storage.i); return exchangestr(n, t); }
        case ID_FVAR: return exchangestr(n, floatstr(*id->storage.f));
        case ID_SVAR: return exchangestr(n, *id->storage.s);
        case ID_ALIAS: return exchangestr(n, id->action);
    }
    conoutf(CON_ERROR, "%s: %s", SV_MESG21,  n+1);
    return n;
}

char *parseword(const char *&p, int arg, int &infix)                       // parse single argument, including expressions
{
    for(;;)
    {
        p += strspn(p, " \t\r");
        if(p[0]!='/' || p[1]!='/') break;
        p += strcspn(p, "\n\0");  
    }
    if(*p=='\"')
    {
        p++;
        const char *word = p;
        p += strcspn(p, "\"\r\n\0");
        char *s = newstring(word, p-word);
        if(*p=='\"') p++;
        return s;
    }
    if(*p=='(') return parseexp(p, ')');
    if(*p=='[') return parseexp(p, ']');
    const char *word = p;
    for(;;)
    {
        p += strcspn(p, "/; \t\r\n\0");
        if(p[0]!='/' || p[1]=='/') break;
        else if(p[1]=='\0') { p++; break; }
        p += 2;
    }
    if(p-word==0) return NULL;
    if(arg==1 && p-word==1) switch(*word)
    {
        case '=': infix = *word; break;
    }
    char *s = newstring(word, p-word);
    if(*s=='$') return lookup(s);               // substitute variables
    return s;
}

char *conc(char **w, int n, bool space)
{
    int len = space ? max(n-1, 0) : 0;
    loopj(n) len += (int)strlen(w[j]);
    char *r = newstring("", len);
    loopi(n)       
    {
        strcat(r, w[i]);  // make string-list out of all arguments
        if(i==n-1) break;
        if(space) strcat(r, " ");
    }
    return r;
}

VARN(numargs, _numargs, 0, 0, 25);

#define parseint(s) strtol((s), NULL, 0)

char *commandret = NULL;

SVAR(SV_MESG22, "");
SVAR(SV_MESG23, "");
SVAR(SV_MESG24, "");
SVAR(SV_MESG25, "");
SVAR(SV_MESG26, "");
extern const char *addreleaseaction(const char *s);

char *executeret(const char *p)               // all evaluation happens here, recursively
{
    const int MAXWORDS = 25;                    // limit, remove
    char *w[MAXWORDS];
    char *retval = NULL;
    #define setretval(v) { char *rv = v; if(rv) retval = rv; }
    for(bool cont = true; cont;)                // for each ; seperated statement
    {
        int numargs = MAXWORDS, infix = 0;
        loopi(MAXWORDS)                         // collect all argument values
        {
            w[i] = (char *)"";
            if(i>numargs) continue;
            char *s = parseword(p, i, infix);   // parse and evaluate exps
            if(s) w[i] = s;
            else numargs = i;
        }
        
        p += strcspn(p, ";\n\0");
        cont = *p++!=0;                         // more statements if this isn't the end of the string
        char *c = w[0];
        if(!*c) continue;                       // empty statement
        
        DELETEA(retval);

        if(infix)
        {
            switch(infix)
            {
                case '=':    
                    aliasa(c, numargs>2 ? w[2] : newstring(""));
                    w[2] = NULL;
                    break;
            }
        }
        else
        {     
            ident *id = idents->access(c);
            if(!id)
            {
                if(!isdigit(*c) && ((*c!='+' && *c!='-' && *c!='.') || !isdigit(c[1]))) 
                    conoutf(CON_ERROR, "%s: %s", SV_MESG22, c);
                setretval(newstring(c));
            }
            else switch(id->type)
            {
                case ID_CCOMMAND:
                case ID_COMMAND:                     // game defined commands
                {   
                    void *v[MAXWORDS];
                    union
                    {
                        int i;
                        float f;
                    } nstor[MAXWORDS];
                    int n = 0, wn = 0;
                    char *cargs = NULL;
                    if(id->type==ID_CCOMMAND) v[n++] = id->self;
                    for(const char *a = id->narg; *a; a++) switch(*a)
                    {
                        case 's':                                 v[n] = w[++wn];     n++; break;
                        case 'i': nstor[n].i = parseint(w[++wn]); v[n] = &nstor[n].i; n++; break;
                        case 'f': nstor[n].f = atof(w[++wn]);     v[n] = &nstor[n].f; n++; break;
#ifndef STANDALONE
                        case 'D': nstor[n].i = addreleaseaction(id->name) ? 1 : 0; v[n] = &nstor[n].i; n++; break;
#endif
                        case 'V': v[n++] = w+1; nstor[n].i = numargs-1; v[n] = &nstor[n].i; n++; break;
                        case 'C': if(!cargs) cargs = conc(w+1, numargs-1, true); v[n++] = cargs; break;
                        default: fatal("%s", SV_MESG23);
                    }
                    switch(n)
                    {
                        case 0: ((void (__cdecl *)()                                      )id->fun)();                             break;
                        case 1: ((void (__cdecl *)(void *)                                )id->fun)(v[0]);                         break;
                        case 2: ((void (__cdecl *)(void *, void *)                        )id->fun)(v[0], v[1]);                   break;
                        case 3: ((void (__cdecl *)(void *, void *, void *)                )id->fun)(v[0], v[1], v[2]);             break;
                        case 4: ((void (__cdecl *)(void *, void *, void *, void *)        )id->fun)(v[0], v[1], v[2], v[3]);       break;
                        case 5: ((void (__cdecl *)(void *, void *, void *, void *, void *))id->fun)(v[0], v[1], v[2], v[3], v[4]); break;
                        case 6: ((void (__cdecl *)(void *, void *, void *, void *, void *, void *))id->fun)(v[0], v[1], v[2], v[3], v[4], v[5]); break;
                        case 7: ((void (__cdecl *)(void *, void *, void *, void *, void *, void *, void *))id->fun)(v[0], v[1], v[2], v[3], v[4], v[5], v[6]); break;
                        case 8: ((void (__cdecl *)(void *, void *, void *, void *, void *, void *, void *, void *))id->fun)(v[0], v[1], v[2], v[3], v[4], v[5], v[6], v[7]); break;
                        default: fatal("%s", SV_MESG24);
                    }
                    if(cargs) delete[] cargs;
                    setretval(commandret);
                    commandret = NULL;
                    break;
                }

                case ID_VAR:                        // game defined variables
                    if(numargs <= 1) conoutf("%s = %d", c, *id->storage.i);      // var with no value just prints its current value
                    else if(id->minval>id->maxval) conoutf(CON_ERROR, "%s: %s", SV_MESG25, id->name);
                    else
                    {
                    	if(id->flags&IDF_WORLD && !editmode && !overrideidents) { conoutf("This variable can only be changed in editmode: %s", id->name); break; }
                        #define OVERRIDEVAR(saveval, resetval, clearval) \
                            if(overrideidents || id->flags&IDF_OVERRIDE) \
                            { \
                                if(id->flags&IDF_PERSIST) \
                                { \
                                    conoutf(CON_ERROR, "cannot override persistent variable %s", id->name); \
                                    break; \
                                } \
                                if(id->override==NO_OVERRIDE) { saveval; id->override = OVERRIDDEN; } \
                                else { clearval; } \
                            } \
                            else \
                            { \
                                if(id->override!=NO_OVERRIDE) { resetval; id->override = NO_OVERRIDE; } \
                                clearval; \
                            }
                        OVERRIDEVAR(id->overrideval.i = *id->storage.i, , )
                        int i1 = parseint(w[1]);
                        if(i1<id->minval || i1>id->maxval)
                        {
                            i1 = i1<id->minval ? id->minval : id->maxval;                // clamp to valid range
                            conoutf(CON_ERROR, "%s %s: %d..%d", SV_MESG26, id->name, id->minval, id->maxval);
                        }
                        *id->storage.i = i1;
                        id->changed();                                             // call trigger function if available
                    }
                    break;
                  
                case ID_FVAR:
                    if(numargs <= 1) conoutf("%s = %s", c, floatstr(*id->storage.f));
                    else if(id->minvalf>id->maxvalf) conoutf(CON_ERROR, "variable %s is read-only", id->name);
                    else
                    {
                    	if(id->flags&IDF_WORLD && !editmode && !overrideidents) { conoutf("This variable can only be changed in editmode: %s", id->name); break; }
                        OVERRIDEVAR(id->overrideval.f = *id->storage.f, , );
                        float f1 = atof(w[1]);
                        if(f1<id->minvalf || f1>id->maxvalf)
                        {
                            f1 = f1<id->minvalf ? id->minvalf : id->maxvalf;                // clamp to valid range
                            conoutf(CON_ERROR, "valid range for %s is %s..%s", id->name, floatstr(id->minvalf), floatstr(id->maxvalf));
                        }
                        *id->storage.f = f1;
                        id->changed();
                    }
                    break;
 
                case ID_SVAR:
                    if(numargs <= 1) conoutf(strchr(*id->storage.s, '"') ? "%s = [%s]" : "%s = \"%s\"", c, *id->storage.s);
                    else
                    {
                    	if(id->flags&IDF_WORLD && !editmode && !overrideidents) { conoutf("This variable can only be changed in editmode: %s", id->name); break; }
                        OVERRIDEVAR(id->overrideval.s = *id->storage.s, delete[] id->overrideval.s, delete[] *id->storage.s);
                        *id->storage.s = newstring(w[1]);
                        id->changed();
                    }
                    break;
                        
                case ID_ALIAS:                              // alias, also used as functions and (global) variables
                {
                    static vector<ident *> argids;
                    for(int i = 1; i<numargs; i++)
                    {
                        if(i > argids.length())
                        {
                            s_sprintfd(argname)("arg%d", i);
                            argids.add(newident(argname));
                        }
                        pushident(*argids[i-1], w[i]); // set any arguments as (global) arg values so functions can access them
                    }
                    _numargs = numargs-1;
                    bool wasoverriding = overrideidents;
                    if(id->override!=NO_OVERRIDE) overrideidents = true;
                    char *wasexecuting = id->isexecuting;
                    id->isexecuting = id->action;
                    setretval(executeret(id->action));
                    if(id->isexecuting != id->action && id->isexecuting != wasexecuting) delete[] id->isexecuting;
                    id->isexecuting = wasexecuting;
                    overrideidents = wasoverriding;
                    for(int i = 1; i<numargs; i++) popident(*argids[i-1]);
                    continue;
                }
            }
        }
        loopj(numargs) if(w[j]) delete[] w[j];
    }
    return retval;
}

int execute(const char *p)
{
    char *ret = executeret(p);
    int i = 0; 
    if(ret) { i = parseint(ret); delete[] ret; }
    return i;
}

bool execfile(const char *cfgfile)
{
	const char *paths[] = {"", "data/"};
	string s;
	loopi(sizeof(paths)/sizeof(paths[0]))
	{
		s_sprintf(s)("%s%s", paths[i], cfgfile);
		char *buf = loadfile(path(s), NULL);
		if(buf)
		{
			execute(buf);
			delete[] buf;
			return true;
		}
	}
	return false;
}

SVAR(SV_MESG27, "");

void exec(const char *cfgfile)
{
    if(!execfile(cfgfile)) conoutf(CON_ERROR, "%s \"%s\"", SV_MESG27, cfgfile);
}

#ifndef STANDALONE
static int sortidents(ident **x, ident **y)
{
    return strcmp((*x)->name, (*y)->name);
}

void writecfg()
{
    s_sprintfd(confname)("config_%s.cfg", cl->gameident());
    FILE *f = openfile(path(confname, true), "w");
    if(!f) return;
    fprintf(f, "// automatically written on exit, DO NOT MODIFY\n// delete this file to have %s overwrite these settings\n// modify settings in game, or put settings in %s to override anything\n\n", cl->defaultconfig(), cl->autoexec());
    fprintf(f, "if (|| (= $ignoreconfver 1) (= $confver %i)) [\n", confver);
    fprintf(f, "\n");
    writecrosshairs(f);
    vector<ident *> ids;
    enumerate(*idents, ident, id, ids.add(&id));
    ids.sort(sortidents);
    loopv(ids)
    {
        ident &id = *ids[i];
        if(id.flags&IDF_PERSIST) switch(id.type)
        {
            case ID_VAR: fprintf(f, "%s %d\n", id.name, *id.storage.i); break;
            case ID_FVAR: fprintf(f, "%s %s\n", id.name, floatstr(*id.storage.f)); break;
            case ID_SVAR: fprintf(f, "%s [%s]\n", id.name, *id.storage.s); break;
        }
    }
    fprintf(f, "\n");
    writebinds(f);
    fprintf(f, "\n");
    loopv(ids)
    {
        ident &id = *ids[i];
        if(id.type==ID_ALIAS && id.flags&IDF_PERSIST && id.override==NO_OVERRIDE && !strstr(id.name, "nextmap_") && id.action[0])
        {
            fprintf(f, "\"%s\" = [%s]\n", id.name, id.action);
        }
    }
    fprintf(f, "\n");
    writecompletions(f);
    fprintf(f, "] [sleep 500 [showgui keepconf]; exec defaults.cfg]\n\n");
    cc->writeclientinfo(f);
    fclose(f);
}

COMMAND(writecfg, "");
#endif

// below the commands that implement a small imperative language. thanks to the semantics of
// () and [] expressions, any control construct can be defined trivially.

void intret(int v) { s_sprintfd(b)("%d", v); commandret = newstring(b); }

const char *floatstr(float v)
{
    static int n = 0;
    static string t[3];
    n = (n + 1)%3;
    s_sprintf(t[n])(v==int(v) ? "%.1f" : "%.7g", v);
    return t[n];
}

void floatret(float v)
{
    commandret = newstring(floatstr(v));
}

ICOMMAND(if, "sss", (char *cond, char *t, char *f), commandret = executeret(cond[0]!='0' ? t : f));
ICOMMAND(loop, "sis", (char *var, int *n, char *body), 
{
    if(*n<=0) return;
    ident *id = newident(var);
    if(id->type!=ID_ALIAS) return;
    loopi(*n)
    {
        if(i) sprintf(id->action, "%d", i);
        else pushident(*id, newstring("0", 16));
        execute(body); 
    } 
    popident(*id);
});
ICOMMAND(while, "ss", (char *cond, char *body), while(execute(cond)) execute(body));    // can't get any simpler than this :)

void concat(const char *s) { commandret = newstring(s); }
void result(const char *s) { commandret = newstring(s); }

void concatword(char **args, int *numargs)
{
    commandret = conc(args, *numargs, false);
}

void format(char **args, int *numargs)
{
    vector<char> s;
    char *f = args[0];
    while(*f)
    {
        int c = *f++;
        if(c == '%')
        {
            int i = *f++;
            if(i >= '1' && i <= '9')
            {
                i -= '0';
                const char *sub = i < *numargs ? args[i] : "";
                while(*sub) s.add(*sub++);
            }
            else s.add(i);
        }
        else s.add(c);
    }
    s.add('\0');
    result(s.getbuf());
}

#define whitespaceskip s += strspn(s, "\n\t ")
#define elementskip *s=='"' ? (++s, s += strcspn(s, "\"\n\0"), s += *s=='"') : s += strcspn(s, "\n\t \0")

void explodelist(const char *s, vector<char *> &elems)
{
    whitespaceskip;
    while(*s)
    {
        const char *elem = s;
        elementskip;
        elems.add(*elem=='"' ? newstring(elem+1, s-elem-(s[-1]=='"' ? 2 : 1)) : newstring(elem, s-elem));
        whitespaceskip;
    }
}

char *indexlist(const char *s, int pos)
{
    whitespaceskip;
    loopi(pos) elementskip, whitespaceskip;
    const char *e = s;
    elementskip;
    if(*e=='"')
    {
        e++;
        if(s[-1]=='"') --s;
    }
    return newstring(e, s-e);
}

void listlen(char *s)
{
    int n = 0;
    whitespaceskip;
    for(; *s; n++) elementskip, whitespaceskip;
    intret(n);
}

void at(char *s, int *pos)
{
    commandret = indexlist(s, *pos);
}

void substr(char *s, int *start, int *count)
{
    int len = strlen(s), offset = clamp(*start, 0, len);
    commandret = newstring(&s[offset], *count <= 0 ? len - offset : min(*count, len - offset));
}

void getalias_(char *s)
{
    result(getalias(s));
}

COMMAND(exec, "s");
COMMAND(concat, "C");
COMMAND(result, "s");
COMMAND(concatword, "V");
COMMAND(format, "V");
COMMAND(at, "si");
COMMAND(substr, "sii");
COMMAND(listlen, "s");
COMMANDN(getalias, getalias_, "s");

void looplist(const char *var, const char *list, const char *body, bool search) 
{ 
    ident *id = newident(var); 
    if(id->type!=ID_ALIAS) { if(search) intret(-1); return; } 
    int n = 0; 
    for(const char *s = list;;) 
    { 
        whitespaceskip; 
        if(!*s) { if(search) intret(-1); break; } 
        const char *start = s; 
        elementskip; 
        const char *end = s; 
        if(*start=='"') { start++; if(end[-1]=='"') --end; } 
        char *val = newstring(start, end-start); 
        if(n++) aliasa(id->name, val); 
        else pushident(*id, val); 
        if(execute(body) && search) { intret(n-1); break; } 
    } 
    if(n) popident(*id); 
} 
 
ICOMMAND(listfind, "sss", (char *var, char *list, char *body), looplist(var, list, body, true)); 
ICOMMAND(looplist, "sss", (char *var, char *list, char *body), looplist(var, list, body, false)); 

void add  (int *a, int *b) { intret(*a + *b); }          COMMANDN(+, add, "ii");
void mul  (int *a, int *b) { intret(*a * *b); }          COMMANDN(*, mul, "ii");
void sub  (int *a, int *b) { intret(*a - *b); }          COMMANDN(-, sub, "ii");
void div_ (int *a, int *b) { intret(*b ? *a / *b : 0); } COMMANDN(div, div_, "ii");
void mod_ (int *a, int *b) { intret(*b ? *a % *b : 0); } COMMANDN(mod, mod_, "ii");
void addf  (float *a, float *b) { floatret(*a + *b); }          COMMANDN(+f, addf, "ff");
void mulf  (float *a, float *b) { floatret(*a * *b); }          COMMANDN(*f, mulf, "ff");
void subf  (float *a, float *b) { floatret(*a - *b); }          COMMANDN(-f, subf, "ff");
void divf_ (float *a, float *b) { floatret(*b ? *a / *b : 0); } COMMANDN(divf, divf_, "ff");
void modf_ (float *a, float *b) { floatret(*b ? fmod(*a, *b) : 0); } COMMANDN(modf, modf_, "ff");
void equal(int *a, int *b) { intret((int)(*a == *b)); }  COMMANDN(=, equal, "ii");
void nequal(int *a, int *b) { intret((int)(*a != *b)); } COMMANDN(!=, nequal, "ii");
void lt   (int *a, int *b) { intret((int)(*a < *b)); }   COMMANDN(<, lt, "ii");
void gt   (int *a, int *b) { intret((int)(*a > *b)); }   COMMANDN(>, gt, "ii");
void lte  (int *a, int *b) { intret((int)(*a <= *b)); } COMMANDN(<=, lte, "ii");
void gte  (int *a, int *b) { intret((int)(*a >= *b)); } COMMANDN(>=, gte, "ii");
void equalf(float *a, float *b) { intret((int)(*a == *b)); }  COMMANDN(=f, equalf, "ff");
void nequalf(float *a, float *b) { intret((int)(*a != *b)); } COMMANDN(!=f, nequalf, "ff");
void ltf   (float *a, float *b) { intret((int)(*a < *b)); }   COMMANDN(<f, ltf, "ff");
void gtf   (float *a, float *b) { intret((int)(*a > *b)); }   COMMANDN(>f, gtf, "ff");
void ltef  (float *a, float *b) { intret((int)(*a <= *b)); } COMMANDN(<=f, ltef, "ff");
void gtef  (float *a, float *b) { intret((int)(*a >= *b)); } COMMANDN(>=f, gtef, "ff");
void xora (int *a, int *b) { intret(*a ^ *b); }          COMMANDN(^, xora, "ii");
void nota (int *a)         { intret(*a == 0); }          COMMANDN(!, nota, "i");
void mina (int *a, int *b) { intret(min(*a, *b)); }      COMMANDN(min, mina, "ii");
void maxa (int *a, int *b) { intret(max(*a, *b)); }      COMMANDN(max, maxa, "ii");

void anda (char *a, char *b) { intret(execute(a)!=0 && execute(b)!=0); }
void ora  (char *a, char *b) { intret(execute(a)!=0 || execute(b)!=0); }

COMMANDN(&&, anda, "ss");
COMMANDN(||, ora, "ss");

void rndn(int *a)          { intret(*a>0 ? rnd(*a) : 0); }  COMMANDN(rnd, rndn, "i");

void strcmpa(char *a, char *b) { intret(strcmp(a,b)==0); }  COMMANDN(strcmp, strcmpa, "ss");

ICOMMAND(echo, "C", (char *s), conoutf(CON_ECHO, "\f1%s", s));

void strstra(char *a, char *b) { char *s = strstr(a, b); intret(s ? s-a : -1); } COMMANDN(strstr, strstra, "ss");

char *strreplace(const char *s, const char *oldval, const char *newval)
{
    vector<char> buf;

    int oldlen = strlen(oldval);
    for(;;)
    {
        const char *found = strstr(s, oldval);
        if(found)
        {
            while(s < found) buf.add(*s++);
            for(const char *n = newval; *n; n++) buf.add(*n);
            s = found + oldlen;
        }
        else 
        {
            while(*s) buf.add(*s++);
            buf.add('\0');
            return newstring(buf.getbuf(), buf.length());
        }
    }
}

void strreplacea(char *s, char *o, char *n) { commandret = strreplace(s, o, n); } COMMANDN(strreplace, strreplacea, "sss");

#ifndef STANDALONE
struct sleepcmd
{
    int millis;
    char *command;
    bool override, persist;
};
vector<sleepcmd> sleepcmds;

void addsleep(int *msec, char *cmd)
{
    sleepcmd &s = sleepcmds.add();
    s.millis = *msec+lastmillis;
    s.command = newstring(cmd);
    s.override = overrideidents;
    s.persist = persistidents;
}

COMMANDN(sleep, addsleep, "is");

void checksleep(int millis)
{
    if(sleepcmds.empty()) return;
    loopv(sleepcmds)
    {
        sleepcmd &s = sleepcmds[i];
        if(s.millis && millis>s.millis)
        {
            char *cmd = s.command; // execute might create more sleep commands
            s.command = NULL;
            bool waspersisting = persistidents, wasoverriding = overrideidents;
            persistidents = s.persist;
            overrideidents = s.override;
            execute(cmd);
            persistidents = waspersisting;
            overrideidents = wasoverriding;
            delete[] cmd;
            if(sleepcmds.inrange(i) && !sleepcmds[i].command) sleepcmds.remove(i--);
        }
    }
}

void clearsleep(bool clearoverrides)
{
    int len = 0;
    loopv(sleepcmds) if(sleepcmds[i].command) 
    {
        if(clearoverrides && !sleepcmds[i].override) sleepcmds[len++] = sleepcmds[i];
        else delete[] sleepcmds[i].command;
    }
    sleepcmds.setsize(len);
}

void clearsleep_(int *clearoverrides)
{
    clearsleep(*clearoverrides!=0 || overrideidents);
}

COMMANDN(clearsleep, clearsleep_, "i");
#endif

