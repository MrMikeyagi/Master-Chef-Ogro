// main.cpp: initialisation & main loop

#include "pch.h"
#include "engine.h"

string version = "2.3.1";

void cleanup()
{
    cleanupserver();
    SDL_ShowCursor(1);
    freeocta(worldroot);
    extern void clear_command(); clear_command();
    extern void clear_console(); clear_console();
    extern void clear_mdls();    clear_mdls();
    extern void clear_sound();   clear_sound();
    SDL_Quit();
}

void quit()                     // normal exit
{
    extern void writeinitcfg();
    writeinitcfg();
    writeservercfg();
    abortconnect();
    disconnect(1);
    writecfg();
    cleanup();
    exit(EXIT_SUCCESS);
}

void fatal(const char *s, ...)    // failure exit
{
    static int errors = 0;
    errors++;

    if(errors <= 2) // print up to one extra recursive error
    {
        s_sprintfdlv(msg,s,s);
        puts(msg);

        if(errors <= 1) // avoid recursion
        {
            SDL_ShowCursor(1);
            #ifdef WIN32
                MessageBox(NULL, msg, "sandbox fatal error", MB_OK|MB_SYSTEMMODAL);
            #endif
            SDL_Quit();
        }
    }
    exit(EXIT_FAILURE);
}

SDL_Surface *screen = NULL;

int curtime;
int totalmillis = 0, lastmillis = 0;

dynent *player = NULL;

int initing = NOT_INITING;
static bool restoredinits = false;

bool initwarning(const char *desc, int level, int type)
{
    if(initing < level)
    {
        addchange(desc, type);
        return true;
    }
    return false;
}

SVAR(WARN1, "");
SVAR(WARN2, "");
SVAR(WARN3, "");
SVAR(WARN4, "");
SVAR(WARN5, "");
SVAR(WARN6, "");

VARF(scr_w, 320, 1024, 10000, initwarning(WARN1));
VARF(scr_h, 200, 768, 10000, initwarning(WARN1));
VARF(colorbits, 0, 0, 32, initwarning(WARN2));
VARF(depthbits, 0, 0, 32, initwarning(WARN3));
VARF(stencilbits, 0, 0, 32, initwarning(WARN4));
VARF(fsaa, -1, -1, 16, initwarning(WARN5));
VARF(vsync, -1, -1, 1, initwarning(WARN6));

void writeinitcfg()
{
    if(!restoredinits) return;
    FILE *f = openfile("init.cfg", "w");
    if(!f) return;
    fprintf(f, "// automatically written on exit, DO NOT MODIFY\n// modify settings in game\n");
    extern int fullscreen;
    fprintf(f, "fullscreen %d\n", fullscreen);
    fprintf(f, "scr_w %d\n", scr_w);
    fprintf(f, "scr_h %d\n", scr_h);
    fprintf(f, "colorbits %d\n", colorbits);
    fprintf(f, "depthbits %d\n", depthbits);
    fprintf(f, "stencilbits %d\n", stencilbits);
    fprintf(f, "fsaa %d\n", fsaa);
    fprintf(f, "vsync %d\n", vsync);
    extern int useshaders, shaderprecision;
    fprintf(f, "shaders %d\n", useshaders);
    fprintf(f, "shaderprecision %d\n", shaderprecision);
    extern int soundchans, soundfreq, soundbufferlen;
    fprintf(f, "soundchans %d\n", soundchans);
    fprintf(f, "soundfreq %d\n", soundfreq);
    fprintf(f, "soundbufferlen %d\n", soundbufferlen);
    extern int loadbacktype;
    fprintf(f, "loadbacktype %d\n", loadbacktype);
    fclose(f);
}

COMMAND(quit, "");

static void getcomputescreenres(int &w, int &h)
{
    float wk = 1, hk = 1;
    if(w < 1024) wk = 1024.0f/w;
    if(h < 768) hk = 768.0f/h;
    wk = hk = max(wk, hk);
    w = int(ceil(w*wk));
    h = int(ceil(h*hk));
}

extern void computescreen(const char *text, Texture *t, const char *overlaytext);

static void bar(float bar, int w, int o, float r, float g, float b)
{
    int side = 2*FONTH;
    float x1 = side, x2 = min(bar, 1.0f)*(w*3-2*side)+side;
    float y1 = o*FONTH;
#if 0 
    glColor3f(0.3f, 0.3f, 0.3f);
    glBegin(GL_TRIANGLE_STRIP);
    loopk(10)
    {
       float c = 1.2f*cosf(M_PI/2 + k/9.0f*M_PI), s = 1 + 1.2f*sinf(M_PI/2 + k/9.0f*M_PI); 
       glVertex2f(x2 - c*FONTH, y1 + s*FONTH);
       glVertex2f(x1 + c*FONTH, y1 + s*FONTH);
    }
    glEnd();
#endif

    glColor3f(r, g, b); 
    glBegin(GL_TRIANGLE_STRIP);
    loopk(10)
    {
       float c = cosf(M_PI/2 + k/9.0f*M_PI), s = 1 + sinf(M_PI/2 + k/9.0f*M_PI); 
       glVertex2f(x2 - c*FONTH, y1 + s*FONTH); 
       glVertex2f(x1 + c*FONTH, y1 + s*FONTH);
    }
    glEnd();
}

float loadprogress = 0;

void show_out_of_renderloop_progress(float bar1, const char *text1, float bar2, const char *text2, GLuint tex)   // also used during loading
{
    if(!inbetweenframes) return;

    clientkeepalive();      // make sure our connection doesn't time out while loading maps etc.
    
    #ifdef __APPLE__
    interceptkey(SDLK_UNKNOWN); // keep the event queue awake to avoid 'beachball' cursor
    #endif

    int w = screen->w, h = screen->h;
    getcomputescreenres(w, h);
    gettextres(w, h);

    glDisable(GL_DEPTH_TEST);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, w*3, h*3, 0, -1, 1);
    notextureshader->set();

    if(text1)
    {
        bar(1, w, 4, 0, 0, 0.8f);
        if(bar1>0) bar(bar1, w, 4, 0, 0.5f, 1);
    }

    if(bar2>0)
    {
        bar(1, w, 6, 0.5f, 0, 0);
        bar(bar2, w, 6, 0.75f, 0, 0);
    }

    glEnable(GL_BLEND);
    glEnable(GL_TEXTURE_2D);
    defaultshader->set();

    if(text1) draw_text(text1, 2*FONTH, 4*FONTH + FONTH/2);
    if(bar2>0) draw_text(text2, 2*FONTH, 6*FONTH + FONTH/2);

    glDisable(GL_BLEND);

    if(tex)
    {
        glBindTexture(GL_TEXTURE_2D, tex);
        int sz = 256, x = (w-sz)/2, y = min(384, h-256);
        sz *= 3;
        x *= 3;
        y *= 3;
        glBegin(GL_QUADS);
        glTexCoord2f(0, 0); glVertex2f(x,    y);
        glTexCoord2f(1, 0); glVertex2f(x+sz, y);
        glTexCoord2f(1, 1); glVertex2f(x+sz, y+sz);
        glTexCoord2f(0, 1); glVertex2f(x,    y+sz);
        glEnd();
    }

    glDisable(GL_TEXTURE_2D);

    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
    glEnable(GL_DEPTH_TEST);
    SDL_GL_SwapBuffers();
}

SVAR(WARN7, "");
SVAR(WARN8, "");

void setfullscreen(bool enable)
{
    if(!screen) return;
#if defined(WIN32) || defined(__APPLE__)
    initwarning(enable ? WARN8 : WARN7);
#else
    if(enable == !(screen->flags&SDL_FULLSCREEN))
    {
        SDL_WM_ToggleFullScreen(screen);
        SDL_WM_GrabInput((screen->flags&SDL_FULLSCREEN) ? SDL_GRAB_ON : SDL_GRAB_OFF);
    }
#endif
}

#ifdef _DEBUG
VARF(fullscreen, 0, 0, 1, setfullscreen(fullscreen!=0));
#else
VARF(fullscreen, 0, 1, 1, setfullscreen(fullscreen!=0));
#endif

void screenres(int *w, int *h)
{
#if !defined(WIN32) && !defined(__APPLE__)
    if(initing >= INIT_RESET)
    {
#endif
        scr_w = *w;
        scr_h = *h;
#if defined(WIN32) || defined(__APPLE__)
        initwarning(WARN1);
#else
        return;
    }
    SDL_Surface *surf = SDL_SetVideoMode(*w, *h, 0, SDL_OPENGL|SDL_RESIZABLE|(screen->flags&SDL_FULLSCREEN));
    if(!surf) return;
    screen = surf;
    scr_w = screen->w;
    scr_h = screen->h;
    glViewport(0, 0, scr_w, scr_h);
#endif
}

COMMAND(screenres, "ii");

SVAR(SV_MESG41, "");

VARFP(gamma, 30, 100, 300,

{
	float f = gamma/100.0f;
    if(SDL_SetGamma(f,f,f)==-1)
    {
        conoutf(CON_ERROR, "%s", SV_MESG41);
        conoutf(CON_ERROR, "sdl: %s", SDL_GetError());
    }
});

SVAR(SV_MESG42, "");
SVAR(SV_MESG43, "");
SVAR(SV_MESG44, "");
SVAR(SV_MESG45, "");

void resetgamma()
{
	float f = gamma/100.0f;
	if(f==1) return;
	SDL_SetGamma(1, 1, 1);
	SDL_SetGamma(f, f, f);
}

void setupscreen(int &usedcolorbits, int &useddepthbits, int &usedfsaa)
{
    int flags = SDL_RESIZABLE;
    #if defined(WIN32) || defined(__APPLE__)
    flags = 0;
    #endif
    if(fullscreen) flags |= SDL_FULLSCREEN;
    SDL_Rect **modes = SDL_ListModes(NULL, SDL_OPENGL|flags);
    if(modes && modes!=(SDL_Rect **)-1)
    {
        bool hasmode = false;
        for(int i = 0; modes[i]; i++)
        {
            if(scr_w <= modes[i]->w && scr_h <= modes[i]->h) { hasmode = true; break; }
        }
        if(!hasmode) { scr_w = modes[0]->w; scr_h = modes[0]->h; }
    }
    bool hasbpp = true;
    if(colorbits && modes)
        hasbpp = SDL_VideoModeOK(modes!=(SDL_Rect **)-1 ? modes[0]->w : scr_w, modes!=(SDL_Rect **)-1 ? modes[0]->h : scr_h, colorbits, SDL_OPENGL|flags)==colorbits;

    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
#if SDL_VERSION_ATLEAST(1, 2, 11)
    if(vsync>=0) SDL_GL_SetAttribute(SDL_GL_SWAP_CONTROL, vsync);
#endif
    static int configs[] =
    {
        0x7, /* try everything */
        0x6, 0x5, 0x3, /* try disabling one at a time */
        0x4, 0x2, 0x1, /* try disabling two at a time */
        0 /* try disabling everything */
    };
    int config = 0;
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 0);
    if(!depthbits) SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 16);
    if(!fsaa)
    {
        SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 0);
        SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 0);
    }
    loopi(sizeof(configs)/sizeof(configs[0]))
    {
        config = configs[i];
        if(!depthbits && config&1) continue;
        if(!stencilbits && config&2) continue;
        if(fsaa<=0 && config&4) continue;
        if(depthbits) SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, config&1 ? depthbits : 16);
        if(stencilbits)
        {
            hasstencil = config&2 ? stencilbits : 0;
            SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, hasstencil);
        }
        else hasstencil = 0;
        if(fsaa>0)
        {
            SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, config&4 ? 1 : 0);
            SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, config&4 ? fsaa : 0);
        }
        screen = SDL_SetVideoMode(scr_w, scr_h, hasbpp ? colorbits : 0, SDL_OPENGL|flags);
        if(screen) break;
    }
    if(!screen) fatal("Unable to create OpenGL screen: %s", SDL_GetError());
    else
    {
        if(!hasbpp) conoutf(CON_WARN, "%d %s", colorbits, SV_MESG42);
        if(depthbits && (config&1)==0) conoutf(CON_WARN, "%d %s", depthbits, SV_MESG43);
        if(stencilbits && (config&2)==0) conoutf(CON_WARN, "%s", SV_MESG44);
        if(fsaa>0 && (config&4)==0) conoutf(CON_WARN, "%dx %s", fsaa, SV_MESG45);
    }

    scr_w = screen->w;
    scr_h = screen->h;

    #ifdef WIN32
    SDL_WM_GrabInput(SDL_GRAB_ON);
    #else
    SDL_WM_GrabInput(fullscreen ? SDL_GRAB_ON : SDL_GRAB_OFF);
    #endif

    usedcolorbits = hasbpp ? colorbits : 0;
    useddepthbits = config&1 ? depthbits : 0;
    usedfsaa = config&4 ? fsaa : 0;
}

extern string CALC6, CALC7;

extern void resetgl();

COMMAND(resetgl, "");

void keyrepeat(bool on)
{
    SDL_EnableKeyRepeat(on ? SDL_DEFAULT_REPEAT_DELAY : 0,
                             SDL_DEFAULT_REPEAT_INTERVAL);
}

static int ignoremouse = 5, grabmouse = 0;

vector<SDL_Event> events;

void pushevent(const SDL_Event &e)
{
    events.add(e);
}

bool interceptkey(int sym)
{
    SDL_Event event;
    while(SDL_PollEvent(&event))
    {
        switch(event.type)
        {
        case SDL_KEYDOWN:
            if(event.key.keysym.sym == sym)
                return true;

        default:
            pushevent(event);
            break;
        }
    }
    return false;
}

void checkinput()
{
    SDL_Event event;
    int lasttype = 0, lastbut = 0;
    while(events.length() || SDL_PollEvent(&event))
    {
        if(events.length()) event = events.remove(0);

        switch(event.type)
        {
            case SDL_QUIT:
                quit();
                break;

            #if !defined(WIN32) && !defined(__APPLE__)
            case SDL_VIDEORESIZE:
                screenres(&event.resize.w, &event.resize.h);
                break;
            #endif

            case SDL_KEYDOWN:
            case SDL_KEYUP:
                keypress(event.key.keysym.sym, event.key.state==SDL_PRESSED, event.key.keysym.unicode);
                break;

            case SDL_ACTIVEEVENT:
                if(event.active.state & SDL_APPINPUTFOCUS)
                    grabmouse = event.active.gain;
                else
                if(event.active.gain)
                    grabmouse = 1;
                break;

            case SDL_MOUSEMOTION:
                if(ignoremouse) { ignoremouse--; break; }
                #ifndef WIN32
                if(!(screen->flags&SDL_FULLSCREEN) && grabmouse)
                {
                    #ifdef __APPLE__
                    if(event.motion.y == 0) break;  //let mac users drag windows via the title bar
                    #endif
                    if(event.motion.x == screen->w / 2 && event.motion.y == screen->h / 2) break;
                    SDL_WarpMouse(screen->w / 2, screen->h / 2);
                }
                if((screen->flags&SDL_FULLSCREEN) || grabmouse)
                #endif
                
                #ifndef NEWGUI
                if(!g3d_movecursor(event.motion.xrel, event.motion.yrel))
		#else
		if(!UI::movecursor(event.motion.xrel, event.motion.yrel))
                #endif
                    mousemove(event.motion.xrel, event.motion.yrel);
                break;

            case SDL_MOUSEBUTTONDOWN:
            case SDL_MOUSEBUTTONUP:
                if(lasttype==event.type && lastbut==event.button.button) break; // why?? get event twice without it
                keypress(-event.button.button, event.button.state!=0, 0);
                lasttype = event.type;
                lastbut = event.button.button;
                break;
        }
    }
}

VARF(gamespeed, 10, 100, 1000, if(multiplayer()) gamespeed = 100);

VARF(paused, 0, 0, 1, if(multiplayer()) paused = 0);

VARP(maxfps, 0, 200, 1000);

void limitfps(int &millis, int curmillis)
{
    if(!maxfps) return;
    static int fpserror = 0;
    int delay = 1000/maxfps - (millis-curmillis);
    if(delay < 0) fpserror = 0;
    else
    {
        fpserror += 1000%maxfps;
        if(fpserror >= maxfps)
        {
            ++delay;
            fpserror -= maxfps;
        }
        if(delay > 0)
        {
            SDL_Delay(delay);
            millis += delay;
        }
    }
}

#if defined(WIN32) && !defined(_DEBUG) && !defined(__GNUC__)
void stackdumper(unsigned int type, EXCEPTION_POINTERS *ep)
{
    if(!ep) fatal("unknown type");
    EXCEPTION_RECORD *er = ep->ExceptionRecord;
    CONTEXT *context = ep->ContextRecord;
    string out, t;
    s_sprintf(out)("Sandbox Win32 Exception: 0x%x [0x%x]\n\n", er->ExceptionCode, er->ExceptionCode==EXCEPTION_ACCESS_VIOLATION ? er->ExceptionInformation[1] : -1);
    STACKFRAME sf = {{context->Eip, 0, AddrModeFlat}, {}, {context->Ebp, 0, AddrModeFlat}, {context->Esp, 0, AddrModeFlat}, 0};
    SymInitialize(GetCurrentProcess(), NULL, TRUE);

    while(::StackWalk(IMAGE_FILE_MACHINE_I386, GetCurrentProcess(), GetCurrentThread(), &sf, context, NULL, ::SymFunctionTableAccess, ::SymGetModuleBase, NULL))
    {
        struct { IMAGEHLP_SYMBOL sym; string n; } si = { { sizeof( IMAGEHLP_SYMBOL ), 0, 0, 0, sizeof(string) } };
        IMAGEHLP_LINE li = { sizeof( IMAGEHLP_LINE ) };
        DWORD off;
        if(SymGetSymFromAddr(GetCurrentProcess(), (DWORD)sf.AddrPC.Offset, &off, &si.sym) && SymGetLineFromAddr(GetCurrentProcess(), (DWORD)sf.AddrPC.Offset, &off, &li))
        {
            char *del = strrchr(li.FileName, '\\');
            s_sprintf(t)("%s - %s [%d]\n", si.sym.Name, del ? del + 1 : li.FileName, li.LineNumber);
            s_strcat(out, t);
        }
    }
    fatal(out);
}
#endif

#define MAXFPSHISTORY 60

int fpspos = 0, fpshistory[MAXFPSHISTORY];

void resetfpshistory()
{
    loopi(MAXFPSHISTORY) fpshistory[i] = 1;
    fpspos = 0;
}

void updatefpshistory(int millis)
{
    fpshistory[fpspos++] = max(1, min(1000, millis));
    if(fpspos>=MAXFPSHISTORY) fpspos = 0;
}

void getfps(int &fps, int &bestdiff, int &worstdiff)
{
    int total = fpshistory[MAXFPSHISTORY-1], best = total, worst = total;
    loopi(MAXFPSHISTORY-1)
    {
        int millis = fpshistory[i];
        total += millis;
        if(millis < best) best = millis;
        if(millis > worst) worst = millis;
    }

    fps = (1000*MAXFPSHISTORY)/total;
    bestdiff = 1000/best-fps;
    worstdiff = fps-1000/worst;
}

void getfps_(int *raw)
{
    int fps, bestdiff, worstdiff;
    if(*raw) fps = 1000/fpshistory[(fpspos+MAXFPSHISTORY-1)%MAXFPSHISTORY];
    else getfps(fps, bestdiff, worstdiff);
    intret(fps);
}

COMMANDN(getfps, getfps_, "i");

bool inbetweenframes = false;

static bool findarg(int argc, char **argv, const char *str)
{
    for(int i = 1; i<argc; i++) if(strstr(argv[i], str)==argv[i]) return true;
    return false;
}

static int clockrealbase = 0, clockvirtbase = 0;
static void clockreset() { clockrealbase = SDL_GetTicks(); clockvirtbase = totalmillis; }
VARFP(clockerror, 990000, 1000000, 1010000, clockreset());
VARFP(clockfix, 0, 0, 1, clockreset());

string languagepackmenus, languagepackstrings, languagepackgamemode;

SVARFP(language, "en",
	execfile("data/lang/menus.cfg");
	execfile("data/lang/lang.cfg");
	s_sprintf(languagepackmenus)("data/lang/%s/menus.cfg", language);
	s_sprintf(languagepackstrings)("data/lang/%s/lang.cfg", language);
	s_sprintf(languagepackgamemode)("data/lang/%s/game_%s.cfg", language, cl->gameident());
	execfile(languagepackmenus);
	execfile(languagepackstrings);
	execfile(languagepackgamemode);
);

int main(int argc, char **argv)
{
    #ifdef WIN32
    //atexit((void (__cdecl *)(void))_CrtDumpMemoryLeaks);
    #ifndef _DEBUG
    #ifndef __GNUC__
    __try {
    #endif
    #endif
    #endif

    int dedicated = 0;
    char *load = NULL, *initscript = NULL;

    #define log(s) puts("init: " s)

    initing = INIT_RESET;
    for(int i = 1; i<argc; i++)

   {
        if(argv[i][0]=='-') switch(argv[i][1])
        {
            case 'q': printf("Using home directory: %s\n", &argv[i][2]); sethomedir(&argv[i][2]); break;
            case 'k': printf("Adding package directory: %s\n", &argv[i][2]); addpackagedir(&argv[i][2]); break;
            case 'r': execfile(argv[i][2] ? &argv[i][2] : "init.cfg"); restoredinits = true; break;
            case 'd': dedicated = atoi(&argv[i][2]); if(dedicated<=0) dedicated = 2; break;
            case 'w': scr_w = atoi(&argv[i][2]); if(scr_w<320) scr_w = 320; if(!findarg(argc, argv, "-h")) scr_h = (scr_w*3)/4; break;
            case 'h': scr_h = atoi(&argv[i][2]); if(scr_h<200) scr_h = 200; if(!findarg(argc, argv, "-w")) scr_w = (scr_h*4)/3; break;
            case 'z': depthbits = atoi(&argv[i][2]); break;
            case 'b': colorbits = atoi(&argv[i][2]); break;
            case 'a': fsaa = atoi(&argv[i][2]); break;
            case 'v': vsync = atoi(&argv[i][2]); break;
            case 't': fullscreen = atoi(&argv[i][2]); break;
            case 's': stencilbits = atoi(&argv[i][2]); break;
            case 'f':
            {
                extern int useshaders, shaderprecision;
                int n = atoi(&argv[i][2]);
                useshaders = n ? 1 : 0;
                shaderprecision = min(max(n - 1, 0), 3);
                break;
            }
            case 'l':
            {
                char pkgdir[] = "packages/";
                load = strstr(path(&argv[i][2]), path(pkgdir));
                if(load) load += sizeof(pkgdir)-1;
                else load = &argv[i][2];
                break;
            }
            case 'x': initscript = &argv[i][2]; break;
            default: if(!serveroption(argv[i])) gameargs.add(argv[i]); break;
        }
        else gameargs.add(argv[i]);
    }
    initing = NOT_INITING;

    log("sdl");

    int par = 0;
    #ifdef _DEBUG
    par = SDL_INIT_NOPARACHUTE;
    #ifdef WIN32
    SetEnvironmentVariable("SDL_DEBUG", "1");
    #endif
    #endif

    //#ifdef WIN32
    //SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);
    //#endif

    log("default language strings");
    exec("data/lang/lang.cfg");

    if(SDL_Init(SDL_INIT_TIMER|SDL_INIT_VIDEO|SDL_INIT_AUDIO|par)<0) fatal("Unable to initialize SDL: %s", SDL_GetError());

    log("net");
    initserver(dedicated>0, dedicated>1);  // never returns if dedicated

    log("video: mode");
    int usedcolorbits = 0, useddepthbits = 0, usedfsaa = 0;
    setupscreen(usedcolorbits, useddepthbits, usedfsaa);

    log("video: misc");
    s_sprintfd(capt)("Sandbox Engine %s", version);
    SDL_WM_SetCaption(capt, NULL);
    keyrepeat(false);
    SDL_ShowCursor(0);

    log("gl");
    gl_checkextensions();
    gl_init(scr_w, scr_h, usedcolorbits, useddepthbits, usedfsaa);
    notexture = textureload("data/notexture");
    if(!notexture) fatal("could not find core textures");
    #ifdef NEWGUI
    UI::setup();
    #endif

    log("console");
    persistidents = false;
    if(!execfile("data/stdlib.cfg")) fatal("cannot find data files (you are running from the wrong folder, try .bat file in the main folder)");   // this is the first file we load.
    if(!execfile("data/font.cfg")) fatal("cannot find font definitions");
    if(!setfont("default")) fatal("no default font specified");

    computescreen("initialising...");
    inbetweenframes = true;

    log("gl: effects");
    loadshaders();
    particleinit();
    initdecals();

    log("world");
    exec("data/keymap.cfg");
    exec("data/menus.cfg");
    camera1 = player = cl->iterdynents(0);
    emptymap(0, true, "", false);

    log("sound");
    initsound();

    log("cfg");
    exec("data/stdedit.cfg");
    exec("data/sounds.cfg");
    exec("data/brush.cfg");
    execfile("mybrushes.cfg");

    #ifdef NEWGUI
    execfile("data/newui.cfg");
    #else
    execfile("data/lang/menus.cfg");
    #endif

    if(cl->savedservers()) execfile(cl->savedservers());

    persistidents = true;

    initing = INIT_LOAD;
    s_sprintfd(confname)("config_%s.cfg", cl->gameident());
    if(!execfile(confname)) exec(cl->defaultconfig());
    execfile(cl->autoexec());
    initing = NOT_INITING;

    persistidents = false;

    s_sprintfd(gamecfgname)("data/game_%s.cfg", cl->gameident());
    exec(gamecfgname);

	exec("loadback/loadback.cfg");

    log("language strings");

    //For translations, this'll load the files.
    
	s_sprintf(languagepackmenus)("data/lang/%s/menus.cfg", language);
	s_sprintf(languagepackstrings)("data/lang/%s/lang.cfg", language);
	s_sprintf(languagepackgamemode)("data/lang/%s/game_%s.cfg", language, cl->gameident());
	execfile(languagepackmenus);
	execfile(languagepackstrings);
	execfile(languagepackgamemode);

    persistidents = true;

    log("localconnect");
    localconnect();
    cc->gameconnect(false);
    cc->changemap(load ? load : cl->defaultmap());

    if(initscript) execute(initscript);
    

    log("mainloop");

    initmumble();
    resetfpshistory();

    for(;;)
    {
        static int frames = 0;
        int millis = SDL_GetTicks() - clockrealbase;
        if(clockfix) millis = int(millis*(double(clockerror)/1000000));
        millis += clockvirtbase;
        if(millis<totalmillis) millis = totalmillis;
        limitfps(millis, totalmillis);
        int elapsed = millis-totalmillis;
        if(multiplayer(false)) curtime = elapsed;
        else
        {
            static int timeerr = 0;
            int scaledtime = elapsed*gamespeed + timeerr;
            curtime = scaledtime/100;
            timeerr = scaledtime%100;
            if(curtime>200) curtime = 200;
            if(paused) curtime = 0;
        }
        lastmillis += curtime;
        totalmillis = millis;

        checkinput();
        #ifndef NEWGUI
        menuprocess();
        #else
        UI::update();
        #endif

        if(lastmillis) cl->updateworld();

        checksleep(lastmillis);

        serverslice(false, 0);

        if(frames) updatefpshistory(elapsed);
        frames++;

        // miscellaneous general game effects
        recomputecamera();
        entity_particles();
        updatevol();
        checkmapsounds();

        inbetweenframes = false;
        if(frames>2) gl_drawframe(screen->w, screen->h);
        SDL_GL_SwapBuffers();
        inbetweenframes = true;
    }

    ASSERT(0);
    return EXIT_FAILURE;

    #if defined(WIN32) && !defined(_DEBUG) && !defined(__GNUC__)
    } __except(stackdumper(0, GetExceptionInformation()), EXCEPTION_CONTINUE_SEARCH) { return 0; }
    #endif
}
