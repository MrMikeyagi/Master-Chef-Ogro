#define DEMO_VERSION 1
#define DEMO_MAGIC "MOVIECUBE_DEMO"
#define MAXDEMOS 5


struct demoheader
{
  char magic[16]; 
  int version, protocol, demomillis;
};

struct demofile
{
  string info;
  uchar *data;
  int len;
};


//demo timeline index - holds the pos of the first packet in a given second
struct demoindex
{
  int demomillis;
  z_off_t gzoffset;
};

//demotimeline
struct demotimeline
{
  vector<demoindex> secs;
  int duration; //in millis
  
  void add(int millis, z_off_t offset)	
  {	
    demoindex idx;
    idx.demomillis = millis;
    idx.gzoffset = offset;
    secs.add(idx); 
  }
};


struct moviedemo
{
  movieserver &sv;
  
  //playback
  ENetPacket *packet;
  vector<demofile> demos;
  gzFile demoplayback;
  int nextplayback;
  int state, seek, chan, len; 
  bool demopause, demoseek;
  z_off_t demostart;
  demotimeline timeline;
  int oldmillis;
  
  //record
  bool demonextmatch;
  FILE *demotmp;
  gzFile demorecord;
  
  moviedemo(movieserver &_sv) : sv(_sv), packet(NULL), demoplayback(NULL), state(0), demopause (false), demoseek(false), oldmillis(0), demonextmatch(false), demotmp(NULL), demorecord(NULL)	{}
  
  void list(int cn)
  {
    ENetPacket *packet = enet_packet_create(NULL, MAXTRANS, ENET_PACKET_FLAG_RELIABLE);
    if(!packet) return;
    ucharbuf p(packet->data, packet->dataLength);
    putint(p, SV_SENDDEMOLIST);
    putint(p, demos.length());
    loopv(demos) sendstring(demos[i].info, p);
    enet_packet_resize(packet, p.length());
    sendpacket(cn, 1, packet);
    if(!packet->referenceCount) enet_packet_destroy(packet);
  }
  
  void send(int cn, int num)
  {
    if(!num) num = demos.length();
    if(!demos.inrange(num-1)) return;
    demofile &d = demos[num-1];
    sendf(cn, 2, "rim", SV_SENDDEMO, d.len, d.data); 
  }
  
  void clear(int n)
  {
    if(!n)
    {
      loopv(demos) delete[] demos[i].data;
      demos.setsize(0);
      sv.sendservmsg("cleared all demos");
    }
    else if(demos.inrange(n-1))
    {
      delete[] demos[n-1].data;
      demos.remove(n-1);
      s_sprintfd(msg)("cleared demo %d", n);
      sv.sendservmsg(msg);
    }
  }
  
  ///////////////////playback/////////////////////
    void setupplay()
  {
    demoheader hdr;
    string msg;
    msg[0] = '\0';
    s_sprintfd(file)("%s.dmo", sv.smapname);
    demoplayback = opengzfile(file, "rb9");
    if(!demoplayback) s_sprintf(msg)("could not read demo \"%s\"", file);
    else if(gzread(demoplayback, &hdr, sizeof(demoheader))!=sizeof(demoheader) || memcmp(hdr.magic, DEMO_MAGIC, sizeof(hdr.magic)))
      s_sprintf(msg)("\"%s\" is not a demo file", file);
    else 
    { 
      endianswap(&hdr.version, sizeof(int), 1);
      endianswap(&hdr.protocol, sizeof(int), 1);
      if(hdr.version!=DEMO_VERSION) s_sprintf(msg)("demo \"%s\" requires an %s version of Moviecube", file, hdr.version<DEMO_VERSION ? "older" : "newer");
      else if(hdr.protocol!=PROTOCOL_VERSION) s_sprintf(msg)("demo \"%s\" requires an %s version of Moviecube", file, hdr.protocol<PROTOCOL_VERSION ? "older" : "newer");
    }
    if(msg[0])
    {
      if(demoplayback) { gzclose(demoplayback); demoplayback = NULL; }
      sv.sendservmsg(msg);
      return;
    }
    
    s_sprintf(msg)("playing demo \"%s\"", file);
    sv.sendservmsg(msg);
    sendf(-1, 1, "rii", SV_DEMOPLAYBACK, 1); 
    
    demostart = gztell(demoplayback);
    
    // get demo length - create simple timeline//
    int tlsec = 0;
    while(gzread(demoplayback, &nextplayback,sizeof(nextplayback)) == sizeof(nextplayback))
    {
      endianswap(&nextplayback, sizeof(nextplayback), 1);
      z_off_t offset = gztell(demoplayback);
      
      if(gzread(demoplayback, &chan, sizeof(chan))!=sizeof(chan) ||
         gzread(demoplayback, &len, sizeof(len))!=sizeof(len))
      {
        break;
      }
      endianswap(&chan, sizeof(chan), 1);
      endianswap(&len, sizeof(len), 1);
      
      //TODO was wenn größers sprünge als 1
      if (tlsec < nextplayback/1000) { timeline.add(nextplayback, offset); }
      tlsec = nextplayback/1000;
      
      ENetPacket *packet = enet_packet_create(NULL, len, 0);
      if(!packet || gzread(demoplayback, packet->data, len)!=len)
      {
        if(packet) enet_packet_destroy(packet);
        break;
      }				
      if(packet) enet_packet_destroy(packet);
    }
    timeline.duration = nextplayback;
    //rewind to demostart
    gzseek (demoplayback, demostart, SEEK_SET);
    //get first nextplayback
    if(gzread(demoplayback, &nextplayback, sizeof(nextplayback))!=sizeof(nextplayback))
    {
      endplayback();
      return;
    }
    endianswap(&nextplayback, sizeof(nextplayback), 1);
  }
  
  void read()
  {    
    if(!demoplayback) return;
    
    if(demopause && !demoseek) { sendpacket(-1, chan, packet); return; }
    
    if(demoseek) {
      gzseek(demoplayback, timeline.secs[seek].gzoffset, SEEK_SET);
      sv.gamemillis = nextplayback = timeline.secs[seek].demomillis;
      demoseek = false;
      demopause = true;
      oldmillis = sv.gamemillis;
    }
    
    while(sv.gamemillis>=nextplayback)
    {
      //clean up old packets 
      if (packet && !packet->referenceCount) enet_packet_destroy(packet); 
      
      if(gzread(demoplayback, &chan, sizeof(chan))!=sizeof(chan) ||
         gzread(demoplayback, &len, sizeof(len))!=sizeof(len))
      {
        endplayback();
        return;
      }
      endianswap(&chan, sizeof(chan), 1);
      endianswap(&len, sizeof(len), 1);
      
      packet = enet_packet_create(NULL, len, 0);
      if(!packet || gzread(demoplayback, packet->data, len)!=len)
      {
        if(packet) enet_packet_destroy(packet);
        endplayback();
        return;
      }
      
      sendpacket(-1, chan, packet);
      
      if(gzread(demoplayback, &nextplayback, sizeof(nextplayback))!=sizeof(nextplayback))
      {
        endplayback();
        return;
      }
      endianswap(&nextplayback, sizeof(nextplayback), 1);			
    }
  }		
  
  void endplayback() //old finish_playback()
  {
    if(!demoplayback) return;
    gzclose(demoplayback);
    demoplayback = NULL;
    
    sendf(-1, 1, "rii", SV_DEMOPLAYBACK, 0);
    sv.sendservmsg("demo playback finished");
    
    loopv(sv.clients)
    {
      ENetPacket *packet = enet_packet_create(NULL, MAXTRANS, ENET_PACKET_FLAG_RELIABLE);
      ucharbuf p(packet->data, packet->dataLength);
      sv.welcomepacket(p, sv.clients[i]->clientnum);
      enet_packet_resize(packet, p.length());
      sendpacket(sv.clients[i]->clientnum, 1, packet);
      if(!packet->referenceCount) enet_packet_destroy(packet);
    }
  }
  
  void togglepause()
  {
    demopause = !demopause;
    if(demopause) { oldmillis = sv.gamemillis; }
    else { sv.gamemillis = oldmillis; }
  }
  
  void seeksec(int sec) {
    if(sec >= 0 && sec <= timeline.duration/1000) {
      seek = sec;
      demoseek = true;
    }
  }
  
  void seekperc(int percent) {
    seek = (timeline.duration/1000)*percent/100;
    demoseek = true;
  }
  
  ///////////////////record/////////////////////
    void setuprecord()
  {
    if(haslocalclients() || !m_mp(sv.gamemode) || sv.gamemode==1) return;
    demotmp = tmpfile();
    if(!demotmp) return;
    setvbuf(demotmp, NULL, _IONBF, 0);
    
    sv.sendservmsg("recording demo");
    
    demorecord = gzdopen(_dup(_fileno(demotmp)), "wb9");
    if(!demorecord)
    {
      fclose(demotmp);
      demotmp = NULL;
      return;
    }
    
    demoheader hdr;
    memcpy(hdr.magic, DEMO_MAGIC, sizeof(hdr.magic));
    hdr.version = DEMO_VERSION;
    hdr.protocol = PROTOCOL_VERSION;
    endianswap(&hdr.version, sizeof(int), 1);
    endianswap(&hdr.protocol, sizeof(int), 1);
    gzwrite(demorecord, &hdr, sizeof(demoheader));
    
    uchar buf[MAXTRANS];
    ucharbuf p(buf, sizeof(buf));
    sv.welcomepacket(p, -1);
    write(1, buf, p.len);
    
    loopv(sv.clients)
    {
      uchar header[16];
      ucharbuf q(&buf[sizeof(header)], sizeof(buf)-sizeof(header));
      putint(q, SV_INITC2S);
      sendstring(sv.clients[i]->name, q);
      sendstring(sv.clients[i]->team, q);
      sendstring(sv.clients[i]->clientmodel, q);
      
      ucharbuf h(header, sizeof(header));
      putint(h, SV_CLIENT);
      putint(h, sv.clients[i]->clientnum);
      putuint(h, q.len);
      
      memcpy(&buf[sizeof(header)-h.len], header, h.len);
      
      write(1, &buf[sizeof(header)-h.len], h.len+q.len);
    }
  }
  
  void write(int chan, void *data, int len) 
  {
    if(!demorecord) return;
    int stamp[3] = { sv.gamemillis, chan, len };
    endianswap(stamp, sizeof(int), 3);
    gzwrite(demorecord, stamp, sizeof(stamp));
    gzwrite(demorecord, data, len);
    //lastgamemillis = sv.gamemillis; //save gamemillis for header
  }
  
  void recordpacket(int chan, void *data, int len) 
  {
    write(chan, data, len);
  }
  
  void endrecord()
  {
    if(!demorecord) return;
    
    gzclose(demorecord);
    demorecord = NULL;
    
    if(!demotmp) return;
    
    fseek(demotmp, 0, SEEK_END);
    int len = ftell(demotmp);
    rewind(demotmp);
    if(demos.length()>=MAXDEMOS)
    {
      delete[] demos[0].data;
      demos.remove(0);
    }
    demofile &d = demos.add();
    time_t t = time(NULL);
    char *timestr = ctime(&t), *trim = timestr + strlen(timestr);
    while(trim>timestr && isspace(*--trim)) *trim = '\0';
    s_sprintf(d.info)("%s: %s, %s, %.2f%s", timestr, sv.modestr(sv.gamemode), sv.smapname, len > 1024*1024 ? len/(1024*1024.f) : len/1024.0f, len > 1024*1024 ? "MB" : "kB");
    s_sprintfd(msg)("demo \"%s\" recorded", d.info);
    sv.sendservmsg(msg);
    d.data = new uchar[len];
    d.len = len;
    fread(d.data, 1, len, demotmp);
    fclose(demotmp);
    demotmp = NULL;
  }
};
