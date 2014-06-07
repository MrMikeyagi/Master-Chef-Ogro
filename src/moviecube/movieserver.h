#ifdef WIN32
#include <io.h>
#else
#include <unistd.h>
#define _dup    dup
#define _fileno fileno
#endif

struct movieserver : igameserver
{
	struct gamestate : moviestate
	{
		vec o;
		int state, editstate;
		
		gamestate() : state(CS_DEAD), editstate(CS_DEAD) {}
		
		void reset()
		{
			if(state!=CS_SPECTATOR) state = editstate = CS_DEAD;
			respawn();
		}
		
		void respawn()
		{
			moviestate::respawn();
			o = vec(-1e10f, -1e10f, -1e10f);
		}
	};
	
	struct clientinfo
	{
		int clientnum;
		string name;
    string mdl;
    bool scripted;
    int owner;
    string mapvote;
		int modevote;
		int privilege;
		bool spectator, local, wantsmaster;
		int gameoffset, lastevent;
		gamestate state;
		vector<uchar> position, messages;
		vector<clientinfo *> targets;
    bool flying;
		
    clientinfo() : clientnum(-1), scripted(false), owner(-1), flying(false) { reset(); }
		
		void mapchange()
		{
			mapvote[0] = 0;
			state.reset();
			lastevent = 0;
		}
		
		void reset()
		{
			name[0] = mdl[0] = 0;
			privilege = PRIV_NONE;
			spectator = local = wantsmaster = false;
			position.setsizenodelete(0);
			messages.setsizenodelete(0);
			mapchange();
		}
	};
	
	struct worldstate
	{
		int uses;
		vector<uchar> positions, messages;
	};
	
	struct ban
	{
		int time;
		uint ip;
	};
	
#define MM_MODE 0xF
#define MM_AUTOAPPROVE 0x1000
#define MM_DEFAULT (MM_MODE | MM_AUTOAPPROVE)
	
	enum { MM_OPEN = 0, MM_VETO, MM_LOCKED, MM_PRIVATE };
	
	int gamemode;
	int gamemillis, gamelimit;
	
	string serverdesc;
	string smapname;
	int lastmillis, totalmillis, curtime;
	enet_uint32 lastsend;
	int mastermode, mastermask;
	int currentmaster;
	bool masterupdate;
	string masterpass;
	FILE *mapdata;
	
	vector<uint> allowedips;
	vector<ban> bannedips;
	vector<clientinfo *> clients;
	vector<worldstate *> worldstates;
	bool reliablemessages;
	
	//~ struct demofile
	//~ {
	//~ string info;
	//~ uchar *data;
	//~ int len;
	//~ };
	
	//~ #define MAXDEMOS 5
	//~ vector<demofile> demos;
	
	//~ bool demonextmatch;
	//~ FILE *demotmp;
	//~ gzFile demorecord, demoplayback;
	//~ int nextplayback;
	
	movieserver() : gamemode(0), lastsend(0), mastermode(MM_OPEN), mastermask(MM_DEFAULT), currentmaster(-1), masterupdate(false), mapdata(NULL), reliablemessages(false) 
	{
		serverdesc[0] = '\0';
		masterpass[0] = '\0';
	}
	
	void *newinfo() { return new clientinfo; }
	void deleteinfo(void *ci) { delete (clientinfo *)ci; } 
	
	static const char *modestr(int n, const char *unknown = "unknown")
	{
		static const char *modenames[] =
		{
			"demo", "Default"
		};
		return (n>=-1 && size_t(n+1)<sizeof(modenames)/sizeof(modenames[0])) ? modenames[n+1] : unknown;
	}
	
	static const char *mastermodestr(int n, const char *unknown = "unknown")
	{
		static const char *mastermodenames[] =
		{
			"open", "veto", "locked", "private"
		};
		return (n>=0 && size_t(n)<sizeof(mastermodenames)/sizeof(mastermodenames[0])) ? mastermodenames[n] : unknown;
	}
	
	void sendservmsg(const char *s) { sendf(-1, 1, "ris", SV_SERVMSG, s); }
	
	int spawntime(int type) { return 0; }
	
	bool pickup(int i, int sender) { return false; }       // server side item pickup, acknowledge first client that gets it
	
	void vote(char *map, int reqmode, int sender)
	{
		clientinfo *ci = (clientinfo *)getinfo(sender);
		if(!ci || (ci->state.state==CS_SPECTATOR && !ci->privilege)) return;
		s_strcpy(ci->mapvote, map);
		ci->modevote = reqmode;
		if(!ci->mapvote[0]) return;
		if(ci->local || (ci->privilege && mastermode>=MM_VETO))
		{
			//~ if(demorecord) enddemorecord();
			if(!ci->local) // && !mapreload) 
			{
				s_sprintfd(msg)("%s forced %s on map %s", privname(ci->privilege), modestr(reqmode), map);
				sendservmsg(msg);
			}
			sendf(-1, 1, "risii", SV_MAPCHANGE, ci->mapvote, ci->modevote, 1);
      //ToDo: delete actors (serverside)
      int pos = 0;
      while (pos < clients.length())
      {
        if(clients[pos]->scripted)
        {
          clientinfo *ci = clients[pos];
          //~ sendf(-1, 1, "ri2", SV_CDIS, ci->clientnum);
          clients.removeobj(ci);
          deleteinfo(ci);
        }
        else pos++;
      }

      //~ loopv(clients) if(clients[i]->scripted)
      //~ {
        //~ clientinfo *ci = clients[i];
        //~ conoutf("__debug__: %s:%d del actor (sv): %s", __FILE__,__LINE__, ci->name); 
        //~ sendf(-1, 1, "ri2", SV_CDIS, ci->clientnum);
        //~ clients.removeobj(ci);
        //~ deleteinfo(ci);
      //~ }
			changemap(ci->mapvote, ci->modevote);
		}
		else 
		{
			s_sprintfd(msg)("%s suggests %s on map %s (select map to vote)", colorname(ci), modestr(reqmode), map);
			sendservmsg(msg);
			checkvotes();
		}
	}
	
	//~ void writedemo(int chan, void *data, int len)
	//~ {
	//~ if(!demorecord) return;
	//~ int stamp[3] = { gamemillis, chan, len };
	//~ endianswap(stamp, sizeof(int), 3);
	//~ gzwrite(demorecord, stamp, sizeof(stamp));
	//~ gzwrite(demorecord, data, len);
	//~ }
	
	//~ void recordpacket(int chan, void *data, int len)
	//~ {
	//~ writedemo(chan, data, len);
	//~ }
	
	//~ void enddemorecord()
	//~ {
	//~ if(!demorecord) return;
	
	//~ gzclose(demorecord);
	//~ demorecord = NULL;
	
	//~ #ifdef WIN32
	//~ demotmp = fopen("demorecord", "rb");
	//~ #endif    
	//~ if(!demotmp) return;
	
	//~ fseek(demotmp, 0, SEEK_END);
	//~ int len = ftell(demotmp);
	//~ rewind(demotmp);
	//~ if(demos.length()>=MAXDEMOS)
	//~ {
	//~ delete[] demos[0].data;
	//~ demos.remove(0);
	//~ }
	//~ demofile &d = demos.add();
	//~ time_t t = time(NULL);
	//~ char *timestr = ctime(&t), *trim = timestr + strlen(timestr);
	//~ while(trim>timestr && isspace(*--trim)) *trim = '\0';
	//~ s_sprintf(d.info)("%s: %s, %s, %.2f%s", timestr, modestr(gamemode), smapname, len > 1024*1024 ? len/(1024*1024.f) : len/1024.0f, len > 1024*1024 ? "MB" : "kB");
	//~ s_sprintfd(msg)("demo \"%s\" recorded", d.info);
	//~ sendservmsg(msg);
	//~ d.data = new uchar[len];
	//~ d.len = len;
	//~ fread(d.data, 1, len, demotmp);
	//~ fclose(demotmp);
	//~ demotmp = NULL;
	//~ }
	
	//~ void setupdemorecord()
	//~ {
	//~ if(haslocalclients() || !m_mp(gamemode) || gamemode==1) return;
	
	//~ #ifdef WIN32
	//~ gzFile f = gzopen("demorecord", "wb9");
	//~ if(!f) return;
	//~ #else
	//~ demotmp = tmpfile();
	//~ if(!demotmp) return;
	//~ setvbuf(demotmp, NULL, _IONBF, 0);
	
	//~ gzFile f = gzdopen(_dup(_fileno(demotmp)), "wb9");
	//~ if(!f)
	//~ {
	//~ fclose(demotmp);
	//~ demotmp = NULL;
	//~ return;
	//~ }
	//~ #endif
	
	//~ sendservmsg("recording demo");
	
	//~ demorecord = f;
	
	//~ demoheader hdr;
	//~ memcpy(hdr.magic, DEMO_MAGIC, sizeof(hdr.magic));
	//~ hdr.version = DEMO_VERSION;
	//~ hdr.protocol = PROTOCOL_VERSION;
	//~ endianswap(&hdr.version, sizeof(int), 1);
	//~ endianswap(&hdr.protocol, sizeof(int), 1);
	//~ gzwrite(demorecord, &hdr, sizeof(demoheader));
	
	//~ ENetPacket *packet = enet_packet_create(NULL, MAXTRANS, 0);
	//~ ucharbuf p(packet->data, packet->dataLength);
	//~ welcomepacket(p, -1, packet);
	//~ writedemo(1, p.buf, p.len);
	//~ enet_packet_destroy(packet);
	
	//~ uchar buf[MAXTRANS];
	//~ loopv(clients)
	//~ {
	//~ clientinfo *ci = clients[i];
	//~ uchar header[16];
	//~ ucharbuf q(&buf[sizeof(header)], sizeof(buf)-sizeof(header));
	//~ putint(q, SV_INITC2S);
	//~ sendstring(ci->name, q);
	//~ sendstring(ci->team, q);
	
	//~ ucharbuf h(header, sizeof(header));
	//~ putint(h, SV_CLIENT);
	//~ putint(h, ci->clientnum);
	//~ putuint(h, q.len);
	
	//~ memcpy(&buf[sizeof(header)-h.len], header, h.len);
	
	//~ writedemo(1, &buf[sizeof(header)-h.len], h.len+q.len);
	//~ }
	//~ }
	
	//~ void listdemos(int cn)
	//~ {
	//~ ENetPacket *packet = enet_packet_create(NULL, MAXTRANS, ENET_PACKET_FLAG_RELIABLE);
	//~ if(!packet) return;
	//~ ucharbuf p(packet->data, packet->dataLength);
	//~ putint(p, SV_SENDDEMOLIST);
	//~ putint(p, demos.length());
	//~ loopv(demos) sendstring(demos[i].info, p);
	//~ enet_packet_resize(packet, p.length());
	//~ sendpacket(cn, 1, packet);
	//~ if(!packet->referenceCount) enet_packet_destroy(packet);
	//~ }
	
	//~ void cleardemos(int n)
	//~ {
	//~ if(!n)
	//~ {
	//~ loopv(demos) delete[] demos[i].data;
	//~ demos.setsize(0);
	//~ sendservmsg("cleared all demos");
	//~ }
	//~ else if(demos.inrange(n-1))
	//~ {
	//~ delete[] demos[n-1].data;
	//~ demos.remove(n-1);
	//~ s_sprintfd(msg)("cleared demo %d", n);
	//~ sendservmsg(msg);
	//~ }
	//~ }
	
	//~ void senddemo(int cn, int num)
	//~ {
	//~ if(!num) num = demos.length();
	//~ if(!demos.inrange(num-1)) return;
	//~ demofile &d = demos[num-1];
	//~ sendf(cn, 2, "rim", SV_SENDDEMO, d.len, d.data); 
	//~ }
	
	//~ void setupdemoplayback()
	//~ {
	//~ demoheader hdr;
	//~ string msg;
	//~ msg[0] = '\0';
	//~ s_sprintfd(file)("%s.dmo", smapname);
	//~ demoplayback = opengzfile(file, "rb9");
	//~ if(!demoplayback) s_sprintf(msg)("could not read demo \"%s\"", file);
	//~ else if(gzread(demoplayback, &hdr, sizeof(demoheader))!=sizeof(demoheader) || memcmp(hdr.magic, DEMO_MAGIC, sizeof(hdr.magic)))
	//~ s_sprintf(msg)("\"%s\" is not a demo file", file);
	//~ else 
	//~ { 
	//~ endianswap(&hdr.version, sizeof(int), 1);
	//~ endianswap(&hdr.protocol, sizeof(int), 1);
	//~ if(hdr.version!=DEMO_VERSION) s_sprintf(msg)("demo \"%s\" requires an %s version of Sauerbraten", file, hdr.version<DEMO_VERSION ? "older" : "newer");
	//~ else if(hdr.protocol!=PROTOCOL_VERSION) s_sprintf(msg)("demo \"%s\" requires an %s version of Sauerbraten", file, hdr.protocol<PROTOCOL_VERSION ? "older" : "newer");
	//~ }
	//~ if(msg[0])
	//~ {
	//~ if(demoplayback) { gzclose(demoplayback); demoplayback = NULL; }
	//~ sendservmsg(msg);
	//~ return;
	//~ }
	
	//~ s_sprintf(msg)("playing demo \"%s\"", file);
	//~ sendservmsg(msg);
	
	//~ sendf(-1, 1, "rii", SV_DEMOPLAYBACK, 1);
	
	//~ if(gzread(demoplayback, &nextplayback, sizeof(nextplayback))!=sizeof(nextplayback))
	//~ {
	//~ enddemoplayback();
	//~ return;
	//~ }
	//~ endianswap(&nextplayback, sizeof(nextplayback), 1);
	//~ }
	
	//~ void enddemoplayback()
	//~ {
	//~ if(!demoplayback) return;
	//~ gzclose(demoplayback);
	//~ demoplayback = NULL;
	
	//~ sendf(-1, 1, "rii", SV_DEMOPLAYBACK, 0);
	
	//~ sendservmsg("demo playback finished");
	
	//~ loopv(clients)
	//~ {
	//~ ENetPacket *packet = enet_packet_create(NULL, MAXTRANS, ENET_PACKET_FLAG_RELIABLE);
	//~ ucharbuf p(packet->data, packet->dataLength);
	//~ welcomepacket(p, clients[i]->clientnum, packet);
	//~ enet_packet_resize(packet, p.length());
	//~ sendpacket(clients[i]->clientnum, 1, packet);
	//~ if(!packet->referenceCount) enet_packet_destroy(packet);
	//~ }
	//~ }
	
	//~ void readdemo()
	//~ {
	//~ if(!demoplayback) return;
	//~ while(gamemillis>=nextplayback)
	//~ {
	//~ int chan, len;
	//~ if(gzread(demoplayback, &chan, sizeof(chan))!=sizeof(chan) ||
	//~ gzread(demoplayback, &len, sizeof(len))!=sizeof(len))
	//~ {
	//~ enddemoplayback();
	//~ return;
	//~ }
	//~ endianswap(&chan, sizeof(chan), 1);
	//~ endianswap(&len, sizeof(len), 1);
	//~ ENetPacket *packet = enet_packet_create(NULL, len, 0);
	//~ if(!packet || gzread(demoplayback, packet->data, len)!=len)
	//~ {
	//~ if(packet) enet_packet_destroy(packet);
	//~ enddemoplayback();
	//~ return;
	//~ }
	//~ sendpacket(-1, chan, packet);
	//~ if(!packet->referenceCount) enet_packet_destroy(packet);
	//~ if(gzread(demoplayback, &nextplayback, sizeof(nextplayback))!=sizeof(nextplayback))
	//~ {
	//~ enddemoplayback();
	//~ return;
	//~ }
	//~ endianswap(&nextplayback, sizeof(nextplayback), 1);
	//~ }
	//~ }
	
	void changemap(const char *s, int mode)
	{
		//~ if(m_demo) enddemoplayback();
		//~ else enddemorecord();
		
		gamemode = mode;
		gamemillis = 0;
		s_strcpy(smapname, s);
		loopv(clients) if (!clients[i]->scripted)
		{
			clientinfo *ci = clients[i];
      ci->mapchange();
      if(m_default && ci->state.state!=CS_SPECTATOR) sendspawn(ci);
    }
	}
	
	struct votecount
	{
		char *map;
		int mode, count;
		votecount() {}
		votecount(char *s, int n) : map(s), mode(n), count(0) {}
	};
	
	void checkvotes(bool force = false)
	{
		vector<votecount> votes;
		int maxvotes = 0;
		loopv(clients)
		{
			clientinfo *oi = clients[i];
			if(oi->state.state==CS_SPECTATOR && !oi->privilege) continue;
			if (!oi->scripted) maxvotes++;
			if(!oi->mapvote[0]) continue;
			votecount *vc = NULL;
			loopvj(votes) if(!strcmp(oi->mapvote, votes[j].map) && oi->modevote==votes[j].mode)
			{ 
				vc = &votes[j];
				break;
			}
			if(!vc) vc = &votes.add(votecount(oi->mapvote, oi->modevote));
			vc->count++;
		}
		votecount *best = NULL;
		loopv(votes) if(!best || votes[i].count > best->count || (votes[i].count == best->count && rnd(2))) best = &votes[i];
		if(force || (best && best->count > maxvotes/2))
		{
			//~ if(demorecord) enddemorecord();
			if(best && (best->count > (force ? 1 : maxvotes/2)))
			{ 
				sendservmsg(force ? "vote passed by default" : "vote passed by majority");
        //ToDo: delete actors here / wait on actors disconnected
        int pos = 0;
        while (pos < clients.length())
        {
          if(clients[pos]->scripted)
          {
            clientinfo *ci = clients[pos];
            //~ sendf(-1, 1, "ri2", SV_CDIS, ci->clientnum);
            clients.removeobj(ci);
            deleteinfo(ci);            
          }
          else pos++;
        }
        sendf(-1, 1, "risii", SV_MAPCHANGE, best->map, best->mode, 1);
				changemap(best->map, best->mode); 
			}
		}
	}
	
	int nonspectators(int exclude = -1)
	{
		int n = 0;
		loopv(clients) if(i!=exclude && clients[i]->state.state!=CS_SPECTATOR) n++;
		return n;
	}
	
	int checktype(int type, clientinfo *ci)
	{
		if(ci && ci->local) return type;
#if 0
		// other message types can get sent by accident if a master forces spectator on someone, so disabling this case for now and checking for spectator state in message handlers
		// spectators can only connect and talk
		static int spectypes[] = { SV_INITC2S, SV_POS, SV_TEXT, SV_PING, SV_CLIENTPING, SV_GETMAP, SV_SETMASTER };
		if(ci && ci->state.state==CS_SPECTATOR && !ci->privilege)
		{
			loopi(sizeof(spectypes)/sizeof(int)) if(type == spectypes[i]) return type;
			return -1;
		}
#endif
		// only allow edit messages in coop-edit mode
		if(type>=SV_EDITENT && type<=SV_GETMAP && gamemode!=0) return -1;
		// server only messages
		static int servtypes[] = { SV_INITS2C, SV_SERVMSG, SV_SPAWNSTATE, SV_CDIS, SV_CURRENTMASTER, SV_PONG, SV_RESUME, SV_SENDDEMOLIST, SV_SENDDEMO, SV_DEMOPLAYBACK, SV_SENDMAP, SV_CLIENT };
		if(ci) loopi(sizeof(servtypes)/sizeof(int)) if(type == servtypes[i]) return -1;
		return type;
	}
	
	static void freecallback(ENetPacket *packet)
	{
		extern igameserver *sv;
		((movieserver *)sv)->cleanworldstate(packet);
	}
	
	void cleanworldstate(ENetPacket *packet)
	{
		loopv(worldstates)
		{
			worldstate *ws = worldstates[i];
			if(packet->data >= ws->positions.getbuf() && packet->data <= &ws->positions.last()) ws->uses--;
			else if(packet->data >= ws->messages.getbuf() && packet->data <= &ws->messages.last()) ws->uses--;
			else continue;
			if(!ws->uses)
			{
				delete ws;
				worldstates.remove(i);
			}
			break;
		}
	}
	
	bool buildworldstate()
	{
		static struct { int posoff, msgoff, msglen; } pkt[MAXCLIENTS];
		worldstate &ws = *new worldstate;
		loopv(clients)
		{
			clientinfo &ci = *clients[i];
			if(ci.position.empty()) pkt[i].posoff = -1;
			else
			{
				pkt[i].posoff = ws.positions.length();
				loopvj(ci.position) ws.positions.add(ci.position[j]);
			}
			if(ci.messages.empty()) pkt[i].msgoff = -1;
			else
			{
				pkt[i].msgoff = ws.messages.length();
				ucharbuf p = ws.messages.reserve(16);
				putint(p, SV_CLIENT);
				putint(p, ci.clientnum);
				putuint(p, ci.messages.length());
				ws.messages.addbuf(p);
				loopvj(ci.messages) ws.messages.add(ci.messages[j]);
				pkt[i].msglen = ws.messages.length() - pkt[i].msgoff;
			}
		}
		int psize = ws.positions.length(), msize = ws.messages.length();
		if(psize) recordpacket(0, ws.positions.getbuf(), psize);
		if(msize) recordpacket(1, ws.messages.getbuf(), msize);
		loopi(psize) { uchar c = ws.positions[i]; ws.positions.add(c); }
		loopi(msize) { uchar c = ws.messages[i]; ws.messages.add(c); }
		ws.uses = 0;
		loopv(clients)
		{
			clientinfo &ci = *clients[i];
			ENetPacket *packet;
			if(psize && (pkt[i].posoff<0 || psize-ci.position.length()>0))
			{
				packet = enet_packet_create(&ws.positions[pkt[i].posoff<0 ? 0 : pkt[i].posoff+ci.position.length()], 
																		pkt[i].posoff<0 ? psize : psize-ci.position.length(), 
																		ENET_PACKET_FLAG_NO_ALLOCATE);
				sendpacket(ci.clientnum, 0, packet);
				if(!packet->referenceCount) enet_packet_destroy(packet);
				else { ++ws.uses; packet->freeCallback = freecallback; }
			}
			ci.position.setsizenodelete(0);
			
			if(msize && (pkt[i].msgoff<0 || msize-pkt[i].msglen>0))
			{
				packet = enet_packet_create(&ws.messages[pkt[i].msgoff<0 ? 0 : pkt[i].msgoff+pkt[i].msglen], 
																		pkt[i].msgoff<0 ? msize : msize-pkt[i].msglen, 
																		(reliablemessages ? ENET_PACKET_FLAG_RELIABLE : 0) | ENET_PACKET_FLAG_NO_ALLOCATE);
				sendpacket(ci.clientnum, 1, packet);
				if(!packet->referenceCount) enet_packet_destroy(packet);
				else { ++ws.uses; packet->freeCallback = freecallback; }
			}
			ci.messages.setsizenodelete(0);
		}
		reliablemessages = false;
		if(!ws.uses) 
		{
			delete &ws;
			return false;
		}
		else 
		{
			worldstates.add(&ws); 
			return true;
		}
	}
	
	bool sendpackets()
	{
		if(clients.empty()) return false;
		enet_uint32 curtime = enet_time_get()-lastsend;
		if(curtime<33) return false;
		bool flush = buildworldstate();
		lastsend += curtime - (curtime%33);
		return flush;
	}
	
	void parsepacket(int sender, int chan, bool reliable, ucharbuf &p)     // has to parse exactly each byte of the packet
	{
		if(sender<0) return;
		if(chan==2)
		{
			receivefile(sender, p.buf, p.maxlen);
			return;
		}
		if(reliable) reliablemessages = true;
		char text[MAXTRANS];
		int cn = -1, type;
		clientinfo *ci = sender>=0 ? (clientinfo *)getinfo(sender) : NULL;
		#define QUEUE_MSG { if(!ci->local) while(curmsg<p.length()) ci->messages.add(p.buf[curmsg++]); }
		#define QUEUE_BUF(size, body) { \
		if(!ci->local) \
		{ \
		curmsg = p.length(); \
		ucharbuf buf = ci->messages.reserve(size); \
		{ body; } \
		ci->messages.addbuf(buf); \
		} \
		}
		#define QUEUE_INT(n) QUEUE_BUF(5, putint(buf, n))
		#define QUEUE_UINT(n) QUEUE_BUF(4, putuint(buf, n))
		#define QUEUE_STR(text) QUEUE_BUF(2*strlen(text)+1, sendstring(text, buf))
		int curmsg;
		while((curmsg = p.length()) < p.maxlen) switch(type = checktype(getint(p), ci))
		{
			case SV_POS:
			{
				cn = getint(p);
        clientinfo *ci = (clientinfo*)getinfo(cn);
				if(cn<0 || cn>=getnumclients() || (ci->scripted == true && sender != ci->owner) )
				{
					disconnect_client(sender, DISC_CN);
					return;
				}
				vec oldpos(ci->state.o);
				loopi(3) ci->state.o[i] = getuint(p)/DMF;
				getuint(p);
				loopi(5) getint(p);
				int physstate = getuint(p);
				if(physstate&0x20) loopi(2) getint(p);
				if(physstate&0x10) getint(p);
				getuint(p);
        getint(p); //animate
        getint(p); //animation
        getuint(p); //fly
				if(!ci->local && (ci->state.state==CS_ALIVE || ci->state.state==CS_EDITING))
				{
					ci->position.setsizenodelete(0);
					while(curmsg<p.length()) ci->position.add(p.buf[curmsg++]);
				}
				break;
			}
				
			case SV_EDITMODE:
			{
				int val = getint(p);
				if(!ci->local && gamemode!=0) break;
				if(val ? ci->state.state!=CS_ALIVE && ci->state.state!=CS_DEAD : ci->state.state!=CS_EDITING) break;
        if(val)
				{
					ci->state.editstate = ci->state.state;
					ci->state.state = CS_EDITING;
				}
				else ci->state.state = ci->state.editstate;
				QUEUE_MSG;
				break;
			}
				
			case SV_SPAWN:
			{
				if(ci->state.state!=CS_ALIVE && ci->state.state!=CS_DEAD) break; 
				ci->state.state = CS_ALIVE;
				QUEUE_BUF(100,
									{
										putint(buf, SV_SPAWN);
									});
				break;
			}
				
			case SV_TEXT:
				QUEUE_MSG;
				getstring(text, p);
				filtertext(text, text);
				QUEUE_STR(text);
				break;
				
			case SV_INITC2S:
			{
				QUEUE_MSG;
				getstring(text, p);
				filtertext(text, text, false, MAXNAMELEN);
				if(!text[0]) s_strcpy(text, "unnamed");
				QUEUE_STR(text);
				s_strcpy(ci->name, text);
				getstring(text, p);
				filtertext(text, text, false, MAXMODELLEN);                
				QUEUE_STR(text);
        //Todo check send INITC2S
				s_strcpy(ci->mdl, text);
				QUEUE_MSG;
				break;
			}
				
			case SV_MAPVOTE:
			case SV_MAPCHANGE:
			{
				getstring(text, p);
				filtertext(text, text);
				int reqmode = getint(p);
				if(type!=SV_MAPVOTE) break;
				//~ if(!ci->local && !m_mp(reqmode)) reqmode = 0; //ToDo gamemode for demo
				reqmode = 0;
				vote(text, reqmode, sender);
				break;
			}
				
				
			case SV_PING:
				sendf(sender, 1, "i2", SV_PONG, getint(p));
				break;
				
			case SV_CLIENTPING:
				getint(p);
				QUEUE_MSG;
				break;
				
			case SV_MASTERMODE:
			{
				int mm = getint(p);
				if(ci->privilege && mm>=MM_OPEN && mm<=MM_PRIVATE)
				{
					if(ci->privilege>=PRIV_ADMIN || (mastermask&(1<<mm)))
					{
						mastermode = mm;
						allowedips.setsize(0);
						if(mm>=MM_PRIVATE) 
						{
							loopv(clients) allowedips.add(getclientip(clients[i]->clientnum));
						}
						s_sprintfd(s)("mastermode is now %s (%d)", mastermodestr(mastermode), mastermode);
						sendservmsg(s);
					}
					else
					{
						s_sprintfd(s)("mastermode %d is disabled on this server", mm);
						sendf(sender, 1, "ris", SV_SERVMSG, s); 
					}
				}   
				break;
			}
				
			case SV_CLEARBANS:
			{
				if(ci->privilege)
				{
					bannedips.setsize(0);
					sendservmsg("cleared all bans");
				}
				break;
			}
				
			case SV_KICK:
			{
				int victim = getint(p);
				if(ci->privilege && victim>=0 && victim<getnumclients() && ci->clientnum!=victim && getinfo(victim))
				{
					ban &b = bannedips.add();
					b.time = totalmillis;
					b.ip = getclientip(victim);
					allowedips.removeobj(b.ip);
					disconnect_client(victim, DISC_KICK);
				}
				break;
			}
				
			case SV_SPECTATOR:
			{
				int spectator = getint(p), val = getint(p);
				if(!ci->privilege && (ci->state.state==CS_SPECTATOR || spectator!=sender)) break;
				clientinfo *spinfo = (clientinfo *)getinfo(spectator);
				if(!spinfo) break;
				
				sendf(-1, 1, "ri3", SV_SPECTATOR, spectator, val);
				
				if(spinfo->state.state!=CS_SPECTATOR && val)
				{
					spinfo->state.state = CS_SPECTATOR;
				}
				else if(spinfo->state.state==CS_SPECTATOR && !val)
				{
					spinfo->state.state = CS_DEAD;
					spinfo->state.respawn();
				}
				break;
			}
				
			//~ case SV_RECORDDEMO:
				//~ {
				//~ int val = getint(p);
				//~ if(ci->privilege<PRIV_ADMIN) break;
				//~ demonextmatch = val!=0;
				//~ s_sprintfd(msg)("demo recording is %s for next match", demonextmatch ? "enabled" : "disabled"); 
				//~ sendservmsg(msg);
				//~ break;
				//~ }
				
			//~ case SV_STOPDEMO:
				//~ {
				//~ if(!ci->local && ci->privilege<PRIV_ADMIN) break;
				//~ if(m_demo) enddemoplayback();
				//~ else enddemorecord();
				//~ break;
				//~ }
				
			//~ case SV_CLEARDEMOS:
				//~ {
				//~ int demo = getint(p);
				//~ if(ci->privilege<PRIV_ADMIN) break;
				//~ cleardemos(demo);
				//~ break;
				//~ }
				
			//~ case SV_LISTDEMOS:
				//~ if(!ci->privilege && ci->state.state==CS_SPECTATOR) break;
				//~ listdemos(sender);
				//~ break;
				
			//~ case SV_GETDEMO:
				//~ {
				//~ int n = getint(p);
				//~ if(!ci->privilege && ci->state.state==CS_SPECTATOR) break;
				//~ senddemo(sender, n);
				//~ break;
				//~ }
				
			case SV_GETMAP:
				if(mapdata)
			{
				sendf(sender, 1, "ris", SV_SERVMSG, "server sending map...");
				sendfile(sender, 2, mapdata, "ri", SV_SENDMAP);
			}
				else sendf(sender, 1, "ris", SV_SERVMSG, "no map to send"); 
				break;
				
			case SV_NEWMAP:
			{
				int size = getint(p);
				if(!ci->privilege && ci->state.state==CS_SPECTATOR) break;
				if(size>=0)
				{
					smapname[0] = '\0';
				}
				QUEUE_MSG;
				break;
			}
				
			case SV_SETMASTER:
			{
				int val = getint(p);
				getstring(text, p);
				setmaster(ci, val!=0, text);
				// don't broadcast the master password
				break;
			}
				
			case SV_APPROVEMASTER:
			{
				int mn = getint(p);
				if(mastermask&MM_AUTOAPPROVE || ci->state.state==CS_SPECTATOR) break;
				clientinfo *candidate = (clientinfo *)getinfo(mn);
				if(!candidate || !candidate->wantsmaster || mn==sender || getclientip(mn)==getclientip(sender)) break;
				setmaster(candidate, true, "", true);
				break;
			}
        
        //__tha__ actor stuff				
      case SV_ADDACTOR:
      {
        int sp = getint(p);
        getstring(text, p);
        filtertext(text, text, false, MAXNAMELEN);
        if(ci->state.state==CS_SPECTATOR && !ci->privilege) break;
        addactor(ci, sp, text);
        break;
      }
        
      case SV_DELACTOR:
      {
        //ToDo: test if client is an actor (scripted)
        int cn = getint(p);
        clientinfo* ai = (clientinfo*)getinfo (cn);
        if(!ai->scripted && ci->state.state==CS_SPECTATOR && !ci->privilege) break;
        sendf(-1, 1, "ri2", SV_CDIS, cn);
        clients.removeobj(ai);
        deleteinfo(ai);
        break;
      }
      case SV_ACTORSIG:
      {
        getstring (text,p);
        int sig = getint(p);
        sendf(-1, 1, "risi", SV_ACTORSIG, text, sig);
        break;
      }
        
      default:
			{
				int size = msgsizelookup(type);
				if(size==-1) { disconnect_client(sender, DISC_TAGT); return; }
				if(size>0) loopi(size-1) getint(p);
				if(ci && ci->state.state!=CS_SPECTATOR) QUEUE_MSG;

        break;
			}
		}
	}
	
  //__tha__ actor
	void addactor(clientinfo *ci,  int sp, const char* name)
	{
		client &cl = addclient();
    //~ cl.type = ST_LOCAL; //ST_LOCAL causes in local game actors adding clients too
    cl.type = ST_SCRIPTED;
    clientinfo *ai = (clientinfo *)getinfo(cl.num);
    #ifndef STANDALONE
    if (ai == NULL) { conoutf("__debug__: %s:%d could not get clientinfo", __FILE__,__LINE__); }
    #endif
    ai->clientnum = cl.num;
		ai->state.state = CS_ALIVE;
    ai->scripted = true;
    ai->owner = ci->clientnum;
    s_strcpy (ai->name, name);
    clients.add(ai);
    sendf(-1, 1, "riiiis", SV_SPAWNACTOR, ci->clientnum, ai->clientnum, sp, name);
	}

	void sendstate(gamestate &gs, ucharbuf &p) {}
	
	int welcomepacket(ucharbuf &p, int n, ENetPacket *packet)
	{
    //ToDo do need welcomepackets for actors / demorecord actors ???

		clientinfo *ci = (clientinfo *)getinfo(n);
		//~ int hasmap = (gamemode==1 && clients.length()>1) || (smapname[0] && (minremain>0 || (ci && ci->state.state==CS_SPECTATOR) || nonspectators(n)));
		int hasmap = (gamemode==0 && clients.length()>1) || (smapname[0] && ((ci && ci->state.state==CS_SPECTATOR) || nonspectators(n)));
		putint(p, SV_INITS2C);
		putint(p, n);
		putint(p, PROTOCOL_VERSION);
		putint(p, hasmap);
		if(hasmap)
		{
			putint(p, SV_MAPCHANGE);
			sendstring(smapname, p);
			putint(p, gamemode);
			//~ putint(p, notgotitems ? 1 : 0);
			putint(p, 0); //ToDo check this
		}
		if(ci && ci->state.state!=CS_SPECTATOR)
		{
			putint(p, SV_SPAWNSTATE);
		}
		if(ci && ci->state.state==CS_SPECTATOR)
		{
			putint(p, SV_SPECTATOR);
			putint(p, n);
			putint(p, 1);
			sendf(-1, 1, "ri3x", SV_SPECTATOR, n, 1, n);   
		}
		if(clients.length()>1)
		{      
			putint(p, SV_RESUME);
			loopv(clients)
			{
				clientinfo *oi = clients[i];
				if(oi->clientnum==n) continue;
				if(p.remaining() < 256)
				{
					enet_packet_resize(packet, packet->dataLength + MAXTRANS);
					p.buf = packet->data;
				}
				putint(p, oi->clientnum);
				putint(p, oi->state.state);
			}
			putint(p, -1);
		}
		return 1;
	}
	
	void clearevent(clientinfo *ci) {}
	
	void spawnstate(clientinfo *ci) {}
	
	void sendspawn(clientinfo *ci) { sendf(ci->clientnum, 1, "ri", SV_SPAWNSTATE); }
	
	void processevents() {}
	
	void serverupdate(int _lastmillis, int _totalmillis)
	{
		curtime = _lastmillis - lastmillis;
		gamemillis += curtime;
		lastmillis = _lastmillis;
		totalmillis = _totalmillis;
		
		//~ if(m_demo) readdemo();
		
		while(bannedips.length() && bannedips[0].time-totalmillis>4*60*60000) bannedips.remove(0);
		
		if(masterupdate) 
		{ 
			clientinfo *m = currentmaster>=0 ? (clientinfo *)getinfo(currentmaster) : NULL;
			sendf(-1, 1, "ri3", SV_CURRENTMASTER, currentmaster, m ? m->privilege : 0); 
			masterupdate = false; 
		} 
	}
	
	bool serveroption(char *arg)
	{
		if(arg[0]=='-') switch(arg[1])
		{
			case 'n': s_strcpy(serverdesc, &arg[2]); return true;
			case 'p': s_strcpy(masterpass, &arg[2]); return true;
			case 'o': if(atoi(&arg[2])) mastermask = (1<<MM_OPEN) | (1<<MM_VETO); return true;
		}
		return false;
	}
	
	void serverinit()
	{
		smapname[0] = '\0';
	}
	
	const char *privname(int type)
	{
		switch(type)
		{
			case PRIV_ADMIN: return "admin";
			case PRIV_MASTER: return "master";
			default: return "unknown";
		}
	}
	
	void setmaster(clientinfo *ci, bool val, const char *pass = "", bool approved = false)
	{
		if(approved && (!val || !ci->wantsmaster)) return;
		const char *name = "";
		if(val)
		{
			if(ci->privilege)
			{
				if(!masterpass[0] || !pass[0]==(ci->privilege!=PRIV_ADMIN)) return;
			}
			else if(ci->state.state==CS_SPECTATOR && (!masterpass[0] || strcmp(masterpass, pass))) return;
			loopv(clients) if(ci!=clients[i] && clients[i]->privilege)
			{
				if(masterpass[0] && !strcmp(masterpass, pass)) clients[i]->privilege = PRIV_NONE;
				else return;
			}
			if(masterpass[0] && !strcmp(masterpass, pass)) ci->privilege = PRIV_ADMIN;
			else if(!approved && !(mastermask&MM_AUTOAPPROVE) && !ci->privilege)
			{
				ci->wantsmaster = true;
				s_sprintfd(msg)("%s wants master. Type \"/approve %d\" to approve.", colorname(ci), ci->clientnum);
				sendservmsg(msg);
				return;
			}
			else ci->privilege = PRIV_MASTER;
			name = privname(ci->privilege);
		}
		else
		{
			if(!ci->privilege) return;
			name = privname(ci->privilege);
			ci->privilege = 0;
		}
		mastermode = MM_OPEN;
		allowedips.setsize(0);
		s_sprintfd(msg)("%s %s %s", colorname(ci), val ? (approved ? "approved for" : "claimed") : "relinquished", name);
		sendservmsg(msg);
		currentmaster = val ? ci->clientnum : -1;
		masterupdate = true;
		loopv(clients) clients[i]->wantsmaster = false;
	}
	
	void localconnect(int n)
	{
		clientinfo *ci = (clientinfo *)getinfo(n);
		ci->clientnum = n;
		ci->local = true;
		clients.add(ci);
	}
	
	void localdisconnect(int n)
	{
		clientinfo *ci = (clientinfo *)getinfo(n);
		clients.removeobj(ci);
	}
	
	int clientconnect(int n, uint ip)
	{
		clientinfo *ci = (clientinfo *)getinfo(n);
		ci->clientnum = n;
		clients.add(ci);
		loopv(bannedips) if(bannedips[i].ip==ip) return DISC_IPBAN;
		if(mastermode>=MM_PRIVATE) 
		{
			if(allowedips.find(ip)<0) return DISC_PRIVATE;
		}
		if(mastermode>=MM_LOCKED) ci->state.state = CS_SPECTATOR;
		if(currentmaster>=0) masterupdate = true;
		return DISC_NONE;
	}
	
	void clientdisconnect(int n) 
	{ 
		clientinfo *ci = (clientinfo *)getinfo(n);
		if(ci->privilege) setmaster(ci, false);
		sendf(-1, 1, "ri2", SV_CDIS, n); 
		clients.removeobj(ci);
		if(clients.empty()) bannedips.setsize(0); // bans clear when server empties
	}
	
	const char *servername() { return "moviecubeserver"; }
	int serverinfoport() { return MOVIECUBE_SERVINFO_PORT; }
	int serverport() { return MOVIECUBE_SERVER_PORT; }
	const char *getdefaultmaster() { return "kids.platinumarts.net/masterserver/"; } 
	
	void serverinforeply(ucharbuf &req, ucharbuf &p)
	{
		if(!getint(req))
		{
			//~ extserverinforeply(req, p);
			return;
		}
		
		putint(p, clients.length());
		putint(p, 4);                   // number of attrs following
		putint(p, PROTOCOL_VERSION);    // a // generic attributes, passed back below
		putint(p, gamemode);            // b
		putint(p, maxclients);
		putint(p, mastermode);
		sendstring(smapname, p);
		sendstring(serverdesc, p);
		sendserverinforeply(p);
	}
	
	bool servercompatible(char *name, char *sdec, char *map, int ping, const vector<int> &attr, int np)
	{
		return attr.length() && attr[0]==PROTOCOL_VERSION;
	}
	
	void receivefile(int sender, uchar *data, int len)
	{
		if(gamemode != 0 || len > 1024*1024) return;
		clientinfo *ci = (clientinfo *)getinfo(sender);
		if(ci->state.state==CS_SPECTATOR && !ci->privilege) return;
		if(mapdata) { fclose(mapdata); mapdata = NULL; }
		if(!len) return;
		mapdata = tmpfile();
		if(!mapdata) { sendf(sender, 1, "ris", SV_SERVMSG, "failed to open temporary file for map"); return; }
		fwrite(data, 1, len, mapdata);
		s_sprintfd(msg)("[%s uploaded map to server, \"/getmap\" to receive it]", colorname(ci));
		sendservmsg(msg);
	}
	
	bool duplicatename(clientinfo *ci, char *name)
	{
		if(!name) name = ci->name;
		loopv(clients) if(clients[i]!=ci && !strcmp(name, clients[i]->name)) return true;
		return false;
	}
	
	char *colorname(clientinfo *ci, char *name = NULL)
	{
		if(!name) name = ci->name;
		if(name[0] && !duplicatename(ci, name)) return name;
		static string cname;
		s_sprintf(cname)("%s \fs\f5(%d)\fr", name, ci->clientnum);
		return cname;
	}
	bool allowbroadcast(int n) { return false; }
        int reserveclients() { return 3; }
};
