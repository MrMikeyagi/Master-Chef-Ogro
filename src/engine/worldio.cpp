 // worldio.cpp: loading & saving of maps and savegames

#include "pch.h"
#include "engine.h"

void backup(char *name, char *backupname)
{
    string backupfile;
    s_strcpy(backupfile, findfile(backupname, "wb"));
    remove(backupfile);
    rename(findfile(name, "wb"), backupfile);
}

string ogzname, bakname, pcfname, mcfname, picname, mname;

VARP(savebak, 0, 2, 2);

void cutogz(char *s)
{
    char *ogzp = strstr(s, ".ogz");
    if(ogzp) *ogzp = '\0';
}

void getnames(const char *fname, const char *cname, char *pakname, char *mapname, char *cfgname)
{
    if(!cname) cname = fname;
    string name;
    s_strncpy(name, cname, 100);
    cutogz(name);
    char *slash = strpbrk(name, "/\\");
    if(slash)
    {
        s_strncpy(pakname, name, slash-name+1);
        s_strcpy(cfgname, slash+1);
    }
    else
    {
        s_strcpy(pakname, "base");
        s_strcpy(cfgname, name);
    }
    if(strpbrk(fname, "/\\")) s_strcpy(mapname, fname);
    else s_sprintf(mapname)("base/%s", fname);
    cutogz(mapname);
}

void setnames(const char *fname, const char *cname = 0)
{
    string pakname, cfgname;
    getnames(fname, cname, pakname, mname, cfgname);

    s_sprintf(ogzname)("packages/%s.ogz", mname);
    if(savebak==1) s_sprintf(bakname)("packages/%s.BAK", mname);
    else s_sprintf(bakname)("packages/%s_%d.BAK", mname, totalmillis);
    s_sprintf(pcfname)("packages/%s/package.cfg", pakname);
    s_sprintf(mcfname)("packages/%s/%s.cfg", pakname, cfgname);
    s_sprintf(picname)("packages/%s", mname);

    path(ogzname);
    path(bakname);
    path(picname);
}

void mapcfgname()
{
    const char *mname = cl->getclientmap();
    if(!*mname) mname = "untitled";

    string pakname, mapname, cfgname;
    getnames(mname, NULL, pakname, mapname, cfgname);
    s_sprintfd(mcfname)("packages/%s/%s.cfg", pakname, cfgname);
    path(mcfname);
    result(mcfname);
}

COMMAND(mapcfgname, "");

ushort readushort(gzFile f)
{
    ushort t;
    gzread(f, &t, sizeof(ushort));
    endianswap(&t, sizeof(ushort), 1);
    return t;
}

void writeushort(gzFile f, ushort u)
{
    endianswap(&u, sizeof(ushort), 1);
    gzwrite(f, &u, sizeof(ushort));
}

enum { OCTSAV_CHILDREN = 0, OCTSAV_EMPTY, OCTSAV_SOLID, OCTSAV_NORMAL, OCTSAV_LODCUBE };

void savec(cube *c, gzFile f, bool nolms)
{
    loopi(8)
    {
        if(c[i].children && (!c[i].ext || !c[i].ext->surfaces))
        {
            gzputc(f, OCTSAV_CHILDREN);
            savec(c[i].children, f, nolms);
        }
        else
        {
            int oflags = 0;
            if(c[i].ext && c[i].ext->merged) oflags |= 0x80;
            if(c[i].children) gzputc(f, oflags | OCTSAV_LODCUBE);
            else if(isempty(c[i])) gzputc(f, oflags | OCTSAV_EMPTY);
            else if(isentirelysolid(c[i])) gzputc(f, oflags | OCTSAV_SOLID);
            else
            {
                gzputc(f, oflags | OCTSAV_NORMAL);
                gzwrite(f, c[i].edges, 12);
            }
            loopj(6) writeushort(f, c[i].texture[j]);
            uchar mask = 0;
            if(c[i].ext)
            {
                if(c[i].ext->material != MAT_AIR) mask |= 0x80;
                if(c[i].ext->normals && !nolms)
                {
                    mask |= 0x40;
                    loopj(6) if(c[i].ext->normals[j].normals[0] != bvec(128, 128, 128)) mask |= 1 << j;
                }
            }
            // save surface info for lighting
            if(!c[i].ext || !c[i].ext->surfaces || nolms)
            {
                gzputc(f, mask);
                if(c[i].ext)
                {
                    if(c[i].ext->material != MAT_AIR) gzputc(f, c[i].ext->material);
                    if(c[i].ext->normals && !nolms) loopj(6) if(mask & (1 << j))
                    {
                        loopk(sizeof(surfaceinfo)) gzputc(f, 0);
                        gzwrite(f, &c[i].ext->normals[j], sizeof(surfacenormals));
                    } 
                }
            }
            else
            {
                int numsurfs = 6;
                loopj(6) 
                {
                    surfaceinfo &surface = c[i].ext->surfaces[j];
                    if(surface.lmid >= LMID_RESERVED || surface.layer!=LAYER_TOP) 
                    {
                        mask |= 1 << j;
                        if(surface.layer&LAYER_BLEND) numsurfs++;
                    }
                }
                gzputc(f, mask);
                if(c[i].ext->material != MAT_AIR) gzputc(f, c[i].ext->material);
                loopj(numsurfs) if(j >= 6 || mask & (1 << j))
                {
                    surfaceinfo tmp = c[i].ext->surfaces[j];
                    endianswap(&tmp.x, sizeof(ushort), 2);
                    gzwrite(f, &tmp, sizeof(surfaceinfo));
                    if(j < 6 && c[i].ext->normals) gzwrite(f, &c[i].ext->normals[j], sizeof(surfacenormals));
                }
            }
            if(c[i].ext && c[i].ext->merged)
            {
                gzputc(f, c[i].ext->merged | (c[i].ext->mergeorigin ? 0x80 : 0));
                if(c[i].ext->mergeorigin)
                {
                    gzputc(f, c[i].ext->mergeorigin);
                    int index = 0;
                    loopj(6) if(c[i].ext->mergeorigin&(1<<j))
                    {
                        mergeinfo tmp = c[i].ext->merges[index++];
                        endianswap(&tmp, sizeof(ushort), 4);
                        gzwrite(f, &tmp, sizeof(mergeinfo));
                    }
                }
            }
            if(c[i].children) savec(c[i].children, f, nolms);
        }
    }
}

cube *loadchildren(gzFile f);

void loadc(gzFile f, cube &c)
{
    bool haschildren = false;
    int octsav = gzgetc(f);
    switch(octsav&0x7)
    {
        case OCTSAV_CHILDREN:
            c.children = loadchildren(f);
            return;

        case OCTSAV_LODCUBE: haschildren = true;    break;
        case OCTSAV_EMPTY:  emptyfaces(c);          break;
        case OCTSAV_SOLID:  solidfaces(c);          break;
        case OCTSAV_NORMAL: gzread(f, c.edges, 12); break;

        default:
            fatal("garbage in map");
    }
    loopi(6) c.texture[i] = hdr.version<14 ? gzgetc(f) : readushort(f);
    if(hdr.version < 7) loopi(3) gzgetc(f); //gzread(f, c.colour, 3);
    else
    {
        uchar mask = gzgetc(f);
        if(mask & 0x80) 
        {
            int mat = gzgetc(f);
            if(hdr.version < 27)
            {
                static uchar matconv[] = { MAT_AIR, MAT_WATER, MAT_CLIP, MAT_GLASS|MAT_CLIP, MAT_NOCLIP, MAT_LAVA|MAT_DEATH, MAT_AICLIP, MAT_DEATH };
                mat = size_t(mat) < sizeof(matconv)/sizeof(matconv[0]) ? matconv[mat] : MAT_AIR;
            }
            ext(c).material = mat;
        }
        if(mask & 0x3F)
        {
            uchar lit = 0, bright = 0;
            static surfaceinfo surfaces[12];
            memset(surfaces, 0, 6*sizeof(surfaceinfo));
            if(mask & 0x40) newnormals(c);
            int numsurfs = 6;
            loopi(numsurfs)
            {
                if(i >= 6 || mask & (1 << i))
                {
                    gzread(f, &surfaces[i], sizeof(surfaceinfo));
                    endianswap(&surfaces[i].x, sizeof(ushort), 2);
                    if(hdr.version < 10) ++surfaces[i].lmid;
                    if(hdr.version < 18)
                    {
                        if(surfaces[i].lmid >= LMID_AMBIENT1) ++surfaces[i].lmid;
                        if(surfaces[i].lmid >= LMID_BRIGHT1) ++surfaces[i].lmid;
                    }
                    if(hdr.version < 19)
                    {
                        if(surfaces[i].lmid >= LMID_DARK) surfaces[i].lmid += 2;
                    }
                    if(i < 6)
                    {
                        if(mask & 0x40) gzread(f, &c.ext->normals[i], sizeof(surfacenormals));
                        if(surfaces[i].layer != LAYER_TOP) lit |= 1 << i;
                        else if(surfaces[i].lmid == LMID_BRIGHT) bright |= 1 << i;
                        else if(surfaces[i].lmid != LMID_AMBIENT) lit |= 1 << i;
                        if(surfaces[i].layer&LAYER_BLEND) numsurfs++;
                    }
                }
                else surfaces[i].lmid = LMID_AMBIENT;
            }
            if(lit) newsurfaces(c, surfaces, numsurfs);
            else if(bright) brightencube(c);
        }
        if(hdr.version >= 20)
        {
            if(octsav&0x80)
            {
                int merged = gzgetc(f);
                ext(c).merged = merged&0x3F;
                if(merged&0x80)
                {
                    c.ext->mergeorigin = gzgetc(f);
                    int nummerges = 0;
                    loopi(6) if(c.ext->mergeorigin&(1<<i)) nummerges++;
                    if(nummerges)
                    {
                        c.ext->merges = new mergeinfo[nummerges];
                        loopi(nummerges)
                        {
                            mergeinfo *m = &c.ext->merges[i];
                            gzread(f, m, sizeof(mergeinfo));
                            endianswap(m, sizeof(ushort), 4);
                            if(hdr.version <= 25)
                            {
                                int uorigin = m->u1 & 0xE000, vorigin = m->v1 & 0xE000;
                                m->u1 = (m->u1 - uorigin) << 2;
                                m->u2 = (m->u2 - uorigin) << 2;
                                m->v1 = (m->v1 - vorigin) << 2;
                                m->v2 = (m->v2 - vorigin) << 2;
                            }
                        }
                    }
                }
            }    
        }                
    }
    c.children = (haschildren ? loadchildren(f) : NULL);
}

cube *loadchildren(gzFile f)
{
    cube *c = newcubes();
    loopi(8) loadc(f, c[i]);
    // TODO: remip c from children here
    return c;
}

SVAR(SV_MESG99, "");
SVAR(SV_MESG100, "");

void writemapcfg(const char *a)
{
	s_sprintfd(b)("packages/base/%s.cfg", strlen(a) ? a : cl->getclientmap());
	s_sprintfd(c)("packages/base/%s-art.cfg", strlen(a) ? a : cl->getclientmap());
	
	if (savebak)
	{
		s_sprintfd(bb)("packages/base/%s_%d.cfg", strlen(a) ? a : cl->getclientmap(), totalmillis);
		s_sprintfd(bc)("packages/base/%s-art_%d.cfg", strlen(a) ? a : cl->getclientmap(), totalmillis);
	
		backup(b, bb);
		backup(c, bc);
	}
	FILE *f = openfile(path(b, true), "w");
	
	extern string version;
	fprintf(f, "//Configuration generated by Platinum Arts Sandbox %s, modify with caution\n//to execute commands on a mapstart, please create an on_start alias with said commands\n//to add anything, add them onto the end of the file or it's given section.\n//for further information, see our wiki or catch us on IRC\n\n", version);
	
	fprintf(f, "exec %s //load saved art assets\n\n", c);
	
	fprintf(f, "//world variables\n\n");
	
	extern identtable *idents;
	
	enumerate(*idents, ident, id,
		if(id.flags&IDF_WORLD) switch(id.type)
		{
			case ID_VAR: fprintf(f, "%s %d\n", id.name, *id.storage.i); break;
			case ID_FVAR: fprintf(f, "%s %s\n", id.name, floatstr(*id.storage.f)); break;
			case ID_SVAR: fprintf(f, "%s [%s]\n", id.name, *id.storage.s); break;
		}
	);
	
	fprintf(f, "\n//mapsounds\n\nmapsoundreset\n\n");
	
	writemapsounds(f);
	
	fprintf(f, "\n//aliases\n\n");
	enumerate(*idents, ident, id,
		if(id.flags&IDF_WORLD) switch(id.type)
		{
			case ID_ALIAS: if(strlen(id.action)) fprintf(f, "\"%s\" = [%s]\n", id.name, id.action); break;
		}
	);
	
	fprintf(f, "\n//game data\n\n");
	cl->writemapdata(f);
	fprintf(f, "\n\n");	
	
	fclose(f);
	conoutf("successfully generated mapcfg: %s", b);
	
	
	
	f = openfile(path(c, true), "w"); //store art in another file... make it look cleaner
	
	fprintf(f, "//This file contains all the art definitions for the map named, %s\n//modify at own risk\n\n", strlen(a) ? a : cl->getclientmap());
	
	fprintf(f, "//map models\n\nmapmodelreset\n\n");
	extern vector<mapmodelinfo> mapmodels;
	
	loopv(mapmodels)
	{
		fprintf(f, "mmodel [%s] //%i\n", mapmodels[i].name, i);
	}
	
	fprintf(f, "\n//Textures\n\ntexturereset\n\n");
	
	extern vector<Slot> slots;
	//Slot materialslots[MATF_VOLUME+1];
	
	//copied from Bloodfrontier, thanks quin!
	#define saveslot(s) \
		if(s.shader) \
		{ \
			fprintf(f, "setshader %s\n", s.shader->name); \
		} \
		loopvj(s.params) \
		{ \
			fprintf(f, "set%sparam", s.params[j].type == SHPARAM_UNIFORM ? "uniform" : (s.params[j].type == SHPARAM_PIXEL ? "pixel" : "vertex")); \
			if(s.params[j].type == SHPARAM_UNIFORM) fprintf(f, " \"%s\"", s.params[j].name); \
			else fprintf(f, " %d", s.params[j].index);\
			loopk(4) fprintf(f, " %f", s.params[j].val[k]); \
				fprintf(f, "\n"); \
		} \
		loopvj(s.sts) \
		{ \
			fprintf(f, "texture"); \
			fprintf(f, " %s", textypename(s.sts[j].type)); \
			fprintf(f, " \"%s\"", s.sts[j].name); \
			if(!j) \
			{ \
				fprintf(f, " %d %d %d %f", \
				s.rotation, s.xoffset, s.yoffset, s.scale); \
				fprintf(f, " // %d", i); \
			} \
			fprintf(f, "\n"); \
		} \
		if(s.scrollS != 0.f || s.scrollT != 0.f) \
			fprintf(f, "texscroll %f %f\n", s.scrollS * 1000.0f, s.scrollT * 1000.0f); \
		if(s.layer != 0) \
			fprintf(f, "texlayer %d\n", s.layer); \
		if(s.autograss) \
			fprintf(f, "autograss \"%s\"\n", s.autograss); \
		fprintf(f, "\n");
	//end copy
	
	loopv(slots)
	{
		saveslot(slots[i]);
	}
	
	fprintf(f, "\n\n");
	fclose(f);
	
	conoutf("successfully generated linked mapcfg: %s", c);
}

COMMAND(writemapcfg, "s");

bool save_world(const char *mname, bool nolms)
{
    if(!*mname) mname = cl->getclientmap();
    setnames(*mname ? mname : "untitled");
    if(savebak) backup(ogzname, bakname);
    
    gzFile f = opengzfile(ogzname, "wb9");
    if(!f) { conoutf(CON_WARN, "%s %s", SV_MESG99, ogzname); return false; }
    hdr.version = MAPVERSION;
    hdr.numents = 0;
    const vector<extentity *> &ents = et->getents();
    loopv(ents) if(ents[i]->type!=ET_EMPTY || nolms) hdr.numents++;
    hdr.numpvs = nolms ? 0 : getnumviewcells();
    hdr.blendmap = nolms ? 0 : shouldsaveblendmap();
    hdr.lightmaps = nolms ? 0 : lightmaps.length();
    header tmp = hdr;
    endianswap(&tmp.version, sizeof(int), 9);
    gzwrite(f, &tmp, sizeof(header));
    
    gzputc(f, (int)strlen(cl->gameident()));
    gzwrite(f, cl->gameident(), (int)strlen(cl->gameident())+1);
    writeushort(f, et->extraentinfosize());
    vector<char> extras;
    cl->writegamedata(extras);
    writeushort(f, extras.length());
    gzwrite(f, extras.getbuf(), extras.length());
    
    
    writeushort(f, texmru.length());
    loopv(texmru) writeushort(f, texmru[i]);
    char *ebuf = new char[et->extraentinfosize()];
    loopv(ents)
    {
        if(ents[i]->type!=ET_EMPTY || nolms)
        {
            entity tmp = *ents[i];
            endianswap(&tmp.o, sizeof(int), 3);
            endianswap(&tmp.attr1, sizeof(short), 5);
            gzwrite(f, &tmp, sizeof(entity));
            et->writeent(*ents[i], ebuf);
            if(et->extraentinfosize()) gzwrite(f, ebuf, et->extraentinfosize());
        }
    }
    delete[] ebuf;

    savec(worldroot, f, nolms);
    if(!nolms) 
    {
        loopv(lightmaps)
        {
            LightMap &lm = lightmaps[i];
            gzputc(f, lm.type | (lm.unlitx>=0 ? 0x80 : 0));
            if(lm.unlitx>=0)
            {
                writeushort(f, ushort(lm.unlitx));
                writeushort(f, ushort(lm.unlity));
            }
            gzwrite(f, lm.data, lm.bpp*LM_PACKW*LM_PACKH);
        }
        if(getnumviewcells()>0) savepvs(f);
        if(shouldsaveblendmap()) saveblendmap(f);
    }

    gzclose(f);
    conoutf("%s %s", SV_MESG100, ogzname);
    writemapcfg(mname);
    return true;
}

static void swapXZ(cube *c)
{	
	loopi(8) 
	{
		swap(c[i].faces[0],   c[i].faces[2]);
		swap(c[i].texture[0], c[i].texture[4]);
		swap(c[i].texture[1], c[i].texture[5]);
		if(c[i].ext && c[i].ext->surfaces)
		{
			swap(c[i].ext->surfaces[0], c[i].ext->surfaces[4]);
			swap(c[i].ext->surfaces[1], c[i].ext->surfaces[5]);
		}
		if(c[i].children) swapXZ(c[i].children);
	}
}

SVAR(SV_MESG101, "");
SVAR(SV_MESG102, "");
SVAR(SV_MESG103, "");
SVAR(SV_MESG104, "");
SVAR(SV_MESG105_1, "");
SVAR(SV_MESG105_2, "");
SVAR(SV_MESG106, "");
SVAR(SV_MESG107, "");
SVAR(SV_MESG108, "");

static void fixoversizedcubes(cube *c, int size)
{
    if(size <= VVEC_INT_MASK+1) return;
    loopi(8)
    {
        if(!c[i].children) subdividecube(c[i], true, false);
        fixoversizedcubes(c[i].children, size>>1);
    }
}

extern int loadbacktype;

bool load_world(const char *mname, const char *cname)        // still supports all map formats that have existed since the earliest cube betas!
{
    int loadingstart = SDL_GetTicks();
    setnames(mname, cname);
    gzFile f = opengzfile(ogzname, "rb9");
	extern string version;
	s_sprintfd(capt)("Sandbox Engine %s: %s - loading...", version, mname);
	SDL_WM_SetCaption(capt, NULL);
    if(!f) { conoutf(CON_ERROR,"%s %s", SV_MESG101, ogzname); return false; }
    header newhdr;
    if(gzread(f, &newhdr, sizeof(header))!=sizeof(header)) { conoutf(CON_ERROR, "map %s has malformatted header", ogzname); gzclose(f); return false; }
    endianswap(&newhdr.version, sizeof(int), 9);
    if(strncmp(newhdr.head, "OCTA", 4)!=0) { conoutf(CON_ERROR,"%s : %s", SV_MESG102, ogzname); gzclose(f); return false; }
    if(newhdr.version>MAPVERSION) { conoutf(CON_ERROR,"%s : %s", SV_MESG103, ogzname); gzclose(f); return false; }
    hdr = newhdr;
    resetmap();
    extern int loadbacktype;
    Texture *mapshot;
    if(loadbacktype!=2)
    	mapshot = textureload(picname, 0, true, false);
    else
    	mapshot = notexture;
    computescreen("loading...", mapshot!=notexture ? mapshot : NULL, mname);
    if(hdr.version<=20) conoutf(CON_WARN, "%s", SV_MESG104);
    if(!hdr.ambient) hdr.ambient = 25;
    if(!hdr.lerpsubdivsize)
    {
        if(!hdr.lerpangle) hdr.lerpangle = 44;
        hdr.lerpsubdiv = 2;
        hdr.lerpsubdivsize = 4;
    }
    setvar("lightprecision", hdr.mapprec ? hdr.mapprec : 32);
    setvar("lighterror", hdr.maple ? hdr.maple : 8);
    setvar("bumperror", hdr.mapbe ? hdr.mapbe : 3);
    setvar("lightlod", hdr.mapllod);
    setvar("ambient", hdr.ambient);
    setvar("fullbright", 0);
    setvar("lerpangle", hdr.lerpangle);
    setvar("lerpsubdiv", hdr.lerpsubdiv);
    setvar("lerpsubdivsize", hdr.lerpsubdivsize);
    
    string gametype;
    s_strcpy(gametype, "fps");
    bool samegame = true;
    int eif = 0;
    if(hdr.version>=16)
    {
        int len = gzgetc(f);
        gzread(f, gametype, len+1);
    }
    if(strcmp(gametype, cl->gameident())!=0)
    {
        samegame = false;
        conoutf(CON_WARN, "%s %s %s", SV_MESG105_1, gametype, SV_MESG105_2);
    }
    if(hdr.version>=16)
    {
        eif = readushort(f);
        int extrasize = readushort(f);
        vector<char> extras;
        loopj(extrasize) extras.add(gzgetc(f));
        if(samegame) cl->readgamedata(extras);
    }
    
    show_out_of_renderloop_progress(0, "clearing world...");

    texmru.setsize(0);
    if(hdr.version<14)
    {
        uchar oldtl[256];
        gzread(f, oldtl, sizeof(oldtl));
        loopi(256) texmru.add(oldtl[i]);
    }
    else
    {
        ushort nummru = readushort(f);
        loopi(nummru) texmru.add(readushort(f));
    }

    freeocta(worldroot);
    worldroot = NULL;

    show_out_of_renderloop_progress(0, "loading entities...");

    vector<extentity *> &ents = et->getents();
    int einfosize = et->extraentinfosize();
    char *ebuf = einfosize > 0 ? new char[einfosize] : NULL;
    loopi(min(hdr.numents, MAXENTS))
    {
        extentity &e = *et->newentity();
        ents.add(&e);
        gzread(f, &e, sizeof(entity));
        endianswap(&e.o, sizeof(int), 3);
        endianswap(&e.attr1, sizeof(short), 5);
        e.spawned = false;
        e.inoctanode = false;
        if(samegame)
        {
            if(einfosize > 0) gzread(f, ebuf, einfosize);
            et->readent(e, ebuf); 
        }
        else
        {
            loopj(eif) gzgetc(f);
        }
        if(hdr.version <= 14 && e.type >= ET_MAPMODEL && e.type <= 16)
        {
            if(e.type == 16) e.type = ET_MAPMODEL;
            else e.type++;
        }
        if(hdr.version <= 20 && e.type >= ET_ENVMAP) e.type++;
        if(hdr.version <= 21 && e.type >= ET_PARTICLES) e.type++;
        if(hdr.version <= 22 && e.type >= ET_SOUND) e.type++;
        if(hdr.version <= 23 && e.type >= ET_SPOTLIGHT) e.type++;
        if(!samegame)
        {
            if(e.type>=ET_GAMESPECIFIC || hdr.version<=14)
            {
                et->deleteentity(ents.pop());
                continue;
            }
        }
        if(!insideworld(e.o))
        {
            if(e.type != ET_LIGHT && e.type != ET_SPOTLIGHT)
            {
                conoutf(CON_WARN, "%s enttype[%s] index %d (%f, %f, %f)", SV_MESG106, et->entname(e.type), i, e.o.x, e.o.y, e.o.z);
            }
        }
        if(hdr.version <= 14 && e.type == ET_MAPMODEL)
        {
            e.o.z += e.attr3;
            if(e.attr4) conoutf(CON_WARN, "warning: mapmodel ent (index %d) uses texture slot %d", i, e.attr4);
            e.attr3 = e.attr4 = 0;
        }
    }
    if(ebuf) delete[] ebuf;

    if(hdr.numents > MAXENTS) 
    {
        conoutf(CON_WARN, "warning: map has %d entities", hdr.numents);
        gzseek(f, (hdr.numents-MAXENTS)*(sizeof(entity) + einfosize), SEEK_CUR);
    }

    show_out_of_renderloop_progress(0, "loading octree...");
    worldroot = loadchildren(f);

	if(hdr.version <= 11)
		swapXZ(worldroot);

    if(hdr.version <= 8)
        converttovectorworld();

    if(hdr.version <= 25 && hdr.worldsize > VVEC_INT_MASK+1)
        fixoversizedcubes(worldroot, hdr.worldsize>>1);

    show_out_of_renderloop_progress(0, "validating...");
    validatec(worldroot, hdr.worldsize>>1);

    worldscale = 0;
    while(1<<worldscale < hdr.worldsize) worldscale++;

    if(hdr.version >= 7) loopi(hdr.lightmaps)
    {
        show_out_of_renderloop_progress(i/(float)hdr.lightmaps, "loading lightmaps...");
        LightMap &lm = lightmaps.add();
        if(hdr.version >= 17)
        {
            int type = gzgetc(f);
            lm.type = type&0x7F;
            if(hdr.version >= 20 && type&0x80)
            {
                lm.unlitx = readushort(f);
                lm.unlity = readushort(f);
            }
        }
        if(lm.type&LM_ALPHA && (lm.type&LM_TYPE)!=LM_BUMPMAP1) lm.bpp = 4;
        lm.data = new uchar[lm.bpp*LM_PACKW*LM_PACKH];
        gzread(f, lm.data, lm.bpp * LM_PACKW * LM_PACKH);
        lm.finalize();
    }

    if(hdr.version >= 25 && hdr.numpvs > 0) loadpvs(f);
    if(hdr.version >= 28 && hdr.blendmap) loadblendmap(f);

    gzclose(f);

    conoutf("read map %s (%.1f seconds)", ogzname, (SDL_GetTicks()-loadingstart)/1000.0f);
    if(hdr.maptitle[0]) conoutf(CON_ECHO, "%s", hdr.maptitle);

    overrideidents = true;
        
    execfile("data/default_map_settings.cfg");
    
    worldidents = true; //ignore playasong... etc
    
    execfile(pcfname);
    execfile(mcfname);
    if(identexists("on_start")) execute("on_start");
    worldidents = false;
    overrideidents = false;
   
    extern void fixlightmapnormals();
    if(hdr.version <= 25) fixlightmapnormals();

    vector<int> mapmodels;
    loopv(ents)
    {
        extentity &e = *ents[i];
        if(e.type==ET_MAPMODEL && e.attr2 >= 0)
        {
            if(mapmodels.find(e.attr2) < 0) mapmodels.add(e.attr2);
        }
    }

    loopv(mapmodels)
    {
        loadprogress = float(i+1)/mapmodels.length();
        int mmindex = mapmodels[i];
        mapmodelinfo &mmi = getmminfo(mmindex);
        if(!&mmi) conoutf(CON_WARN, "%s %d", SV_MESG107, mmindex);
        else if(!loadmodel(NULL, mmindex, true)) conoutf(CON_WARN, "%s %s", SV_MESG108, mmi.name);
    }
    loadprogress = 0;

    cl->preload();

    entitiesinoctanodes();
    attachentities();
    initlights();
    allchanged(true);

    computescreen("loading...", loadbacktype!=2 ? mapshot!=notexture ? mapshot : NULL : NULL, mname);

    startmap(cname ? cname : mname);
    return true;
}

void savecurrentmap() { save_world(cl->getclientmap()); }
void savemap(char *mname) { save_world(mname); }

COMMAND(savemap, "s");
COMMAND(savecurrentmap, "");

void writeobj(char *name)
{
    s_sprintfd(fname)("%s.obj", name);
    FILE *f = openfile(path(fname), "w"); 
    if(!f) return;
    fprintf(f, "# obj file of sandbox level\n");
    extern vector<vtxarray *> valist;
    loopv(valist)
    {
        vtxarray &va = *valist[i];
        ushort *edata = NULL;
        uchar *vdata = NULL;
        if(!readva(&va, edata, vdata)) continue;
        int vtxsize = VTXSIZE;
        uchar *vert = vdata;
        loopj(va.verts) 
        {
            vec v;
            if(floatvtx) (v = *(vec *)vert).div(1<<VVEC_FRAC); 
            else v = ((vvec *)vert)->tovec(va.o).add(0x8000>>VVEC_FRAC);
            if(v.x != floor(v.x)) fprintf(f, "v %.3f ", v.x); else fprintf(f, "v %d ", int(v.x));
            if(v.y != floor(v.y)) fprintf(f, "%.3f ", v.y); else fprintf(f, "%d ", int(v.y));
            if(v.z != floor(v.z)) fprintf(f, "%.3f\n", v.z); else fprintf(f, "%d\n", int(v.z));
            vert += vtxsize;
        }
        ushort *tri = edata;
        loopi(va.tris)
        {
            fprintf(f, "f");
            for(int k = 0; k<3; k++) fprintf(f, " %d", tri[k]-va.verts-va.voffset);
            tri += 3;
            fprintf(f, "\n");
        }
        delete[] edata;
        delete[] vdata;
    }
    fclose(f);
}  
    
COMMAND(writeobj, "s"); 

