DEFINES = 
CXXOPTFLAGS= -O2 -fomit-frame-pointer
INCLUDES= -Ishared -Iengine -Ifpsgame -Imoviecube -Isspgame -Ienet/include -I/usr/X11R6/include `sdl-config --cflags`
CXXFLAGS= -Wall -fsigned-char $(CXXOPTFLAGS) $(INCLUDES) $(DEFINES)


PLATFORM_PREFIX=sandbox

CLIENT_LIBS= -Lenet -lenet -L/usr/X11R6/lib `sdl-config --libs` -lSDL_image -lSDL_mixer -lz -lGL
ifeq ($(shell uname -s),Linux)
CLIENT_LIBS+= -lrt
endif
CLIENT_OBJS= \
	shared/tools.o \
	shared/geom.o \
	engine/3dgui.o \
	engine/bih.o \
	engine/blend.o \
	engine/blob.o \
	engine/client.o	\
	engine/command.o \
	engine/console.o \
	engine/cubeloader.o \
	engine/decal.o \
	engine/dynlight.o \
	engine/glare.o \
	engine/grass.o \
	engine/lightmap.o \
	engine/main.o \
	engine/material.o \
	engine/menus.o \
	engine/normal.o	\
	engine/octa.o \
	engine/octaedit.o \
	engine/octarender.o \
	engine/pvs.o \
	engine/physics.o \
	engine/rendergl.o \
	engine/rendermodel.o \
	engine/renderparticles.o \
	engine/rendersky.o \
	engine/rendertext.o \
	engine/renderva.o \
	engine/server.o	\
	engine/serverbrowser.o \
	engine/shader.o \
	engine/shadowmap.o \
	engine/sound.o \
	engine/texture.o \
	engine/water.o \
	engine/world.o \
	engine/worldio.o \
	fpsgame/fps.o \
	sspgame/ssp.o \
	rpggame/rpg.o
	
SERVER_LIBS= -Lenet -lenet -lz
SERVER_OBJS= \
	shared/tools-standalone.o \
	engine/server-standalone.o \
	fpsgame/fps-standalone.o \

LAUNCHER_LIBS= `wx-config --libs`
LAUNCHER_FLAGS= -O2 -Wall `wx-config --cxxflags`
LAUNCHER_OBJS= \
	launcher/pasl.o \
	launcher/paslapp.o

ifeq ($(shell uname -s),SunOS) 
CLIENT_LIBS+= -lsocket -lnsl -lX11 
SERVER_LIBS+= -lsocket -lnsl 
endif 

default: all

pasl:
	g++ $(LAUNCHER_FLAGS) -c -o launcher/pasl.o launcher/pasl.cpp
	g++ $(LAUNCHER_FLAGS) -c -o launcher/paslapp.o launcher/paslapp.cpp
	g++ $(LAUNCHER_FLAGS) -o launch $(LAUNCHER_OBJS) $(LAUNCHER_LIBS)
	cp launch ../launcher
	strip ../launcher
	chmod +x ../launcher

all: client server

enet/Makefile:
	cd enet; ./configure
	
libenet: enet/Makefile
	$(MAKE)	-C enet/ all

clean: enet/Makefile
	-$(RM) $(SERVER_OBJS) $(CLIENT_OBJS) $(PLATFORM_PREFIX)_client_$(shell uname -m) sandbox_server_$(shell uname -m)
	$(MAKE)	-C enet/ clean

%-standalone.o:
	$(CXX) $(CXXFLAGS) -DSTANDALONE -c -o $@ $(subst -standalone.o,.cpp,$@)

client:	libenet $(CLIENT_OBJS)
	$(CXX) $(CXXFLAGS) -o $(PLATFORM_PREFIX)_client_$(shell uname -m) $(CLIENT_OBJS) $(CLIENT_LIBS)
	
server:	libenet $(SERVER_OBJS)
	$(CXX) $(CXXFLAGS) -o $(PLATFORM_PREFIX)_server_$(shell uname -m) $(SERVER_OBJS) $(SERVER_LIBS)  

install: all
	cp $(PLATFORM_PREFIX)_* ../bin/
	strip ../bin/$(PLATFORM_PREFIX)_*
	chmod +x ../bin/$(PLATFORM_PREFIX)_*

distinstall: install
	chmod +x ./install.sh
	exec ./install.sh

distclean:
	rm -rf /usr/local/sandbox/
	rm -f /usr/bin/sandbox
