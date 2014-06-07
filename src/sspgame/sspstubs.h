struct sspclientcom : iclientcom
{
    ~sspclientcom() {}

    void gamedisconnect() {}
    void parsepacketclient(int chan, ucharbuf &p) {}
    int sendpacketclient(ucharbuf &p, bool &reliable, dynent *d) { return -1; }
    void gameconnect(bool _remote) {}
    bool allowedittoggle() { return true; }
    void writeclientinfo(FILE *f) {}
    void toserver(char *text) {}
    void changemap(const char *name) { load_world(name); }
    void connectattempt(const char*, const char*, const ENetAddress&) {}
    void connectfail() {}
};

struct sspclientserver : igameserver
{
    ~sspclientserver() {}

    void *newinfo() { return NULL; }
    void deleteinfo(void *ci) {}
    void serverinit() {}
    void clientdisconnect(int n) {}
    int clientconnect(int n, uint ip) { return DISC_NONE; }
    void localdisconnect(int n) {}
    void localconnect(int n) {}
    const char *servername() { return "foo"; }
    void parsepacket(int sender, int chan, bool reliable, ucharbuf &p) {}
    bool sendpackets() { return false; }
    int welcomepacket(ucharbuf &p, int n, ENetPacket *packet) { return -1; }
    void serverinforeply(ucharbuf &q, ucharbuf &p) {}
    void serverupdate(int lastmillis, int totalmillis) {}
    bool servercompatible(char *name, char *sdec, char *map, int ping, const vector<int> &attr, int np) { return false; }
    void serverinfostr(char *buf, const char *name, const char *desc, const char *map, int ping, const vector<int> &attr, int np) {}
    int serverinfoport() { return 0; }
    int serverport() { return 0; }
    const char *getdefaultmaster() { return "localhost"; }
    void sendservmsg(const char *s) {}
    bool allowbroadcast(int n) { return false; }
    int reserveclients() { return 3; }
};
