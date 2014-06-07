//Camera Modi:
//1. Player Stays (Player is rendered on last position, controls are taken by camera):
//  1.1. Free Cam (camera has all controls, can move free)
//  1.2. Free Cam with free pitch and yaw (pitch and yaw have no influence of the camera's the direction of the movement)

//camera should be an own dynent not player1
//player1 could be camera itself or not by option
//add free camera control (player1 stays on old pos an is rendered if needed (playeranimation etc.) )

enum { CAM_OFF, CAM_ON };
//ROT_FIX, ROT_DIR -> ROT_ABS, ROT_REL
enum { ROT_WORLD, ROT_LOCAL };
enum { ROT_NONE, ROT_FIX, ROT_DUR, ROT_DIR, ROT_TRGT_ENT, ROT_TARGT_PHYSENT };
enum { MOVE_NONE, MOVE_TRGT_ENT, MOVE_TRGT_PATH, MOVE_TRGT_PHYSENT };
enum { ZOOM_NONE, ZOOM_FIX, ZOOM_DUR };

//enum { ROT_NONE, ROT_LOCAL, ROT_WORLD, ROT_TRGT } //target -> class encupsulate ents/physents
// enum { MOVE_NONE, MOVE_WORLD, MOVE_TRGT }


//ToDo new :)
//~ struct camdata 
//~ {
	//~ float yaw;
	//~ float pitch;
	//~ int fov;

	//~ vec o;
	//~ vec lastdir;
	//~ vec lasttarget;
	
	//~ int rotdur;
	//~ int fovdur;
	//~ int dirdur;
//~ };

struct moviecamera
{  
	movieclient &cl;
	
	bool mode;
	bool init;
	int movement;
	int rotation;
	int zooming;
	
	float yaw, pitch;
	float fov;
	
	vec lastdir;
	vec lasttarget;
	
	int oldmillis;
	bool clockwise;
	
	int rotdur;
	int fovdur;	//duration to change fov
	int dirdur, curdirdur; //duration to change direction
	
	physent *movetrgt;
	physent *tracktrgt;
	entity  *camerapos;
	
	physent camera;
	
	vector<movieentity*> path;
	int pathidx;
	
	trlincirc panyaw;
	trlin panpitch;
	trlin panzoom;
	
	moviecamera(movieclient &_cl) : cl(_cl), mode(CAM_OFF), init(false), movement(MOVE_NONE), 
	rotation(ROT_NONE), yaw(0), pitch(0), fov(100), clockwise(true), 
	rotdur(3000), fovdur(3000), dirdur(5000), 
	movetrgt(NULL), tracktrgt(NULL), camerapos(NULL), pathidx(0)
	{
		CCOMMAND(togglecamera, "", (moviecamera *self),  self->togglecamera() );
		
		CCOMMAND(cameraswitch, "ii", (moviecamera *self, int *n, int *flag), self->switchcamera(*n, *flag) );
		CCOMMAND(camerasetposition, "fff", (moviecamera *self, float *x, float *y, float *z), self->setposition(*x, *y, *z) );
		CCOMMAND(camerasetangle, "ffi", (moviecamera *self, float *y, float *p, int *flag), self->setangle(*y, *p, *flag) );
		CCOMMAND(camerasetfov, "f", (moviecamera *self, float *fov), self->setfov(*fov) );
		CCOMMAND(camerasetspeed, "i", (moviecamera *self, int *speed), self->setspeed(*speed) );
		CCOMMAND(camerasetddur, "i", (moviecamera *self, int *millis), self->setdirdur(*millis) );
		CCOMMAND(camerasetfdur, "i", (moviecamera *self, int *millis), self->setfovdur(*millis) );
		CCOMMAND(camerasetrdur, "i", (moviecamera *self, int *millis), self->setrotdur(*millis) );
		
		CCOMMAND(cameramove, "i", (moviecamera *self, int *n), self->move(*n) );
		CCOMMAND(camerarotate, "ff", (moviecamera *self, float *yaw, float *pitch), self->rotate(*yaw, *pitch) );
		CCOMMAND(camerazoom, "f", (moviecamera *self, float *fov), self->zoom(*fov) );
		CCOMMAND(camerasetplayerfocus, "si", (moviecamera *self, char *s), self->camerafocusplayer(s) );
		
		CCOMMAND(camerapathappend, "i", (moviecamera *self, int *n), self->pathappend(*n) );
		CCOMMAND(camerapathstart, "", (moviecamera *self), self->pathstart() );
		CCOMMAND(camerapathclear, "", (moviecamera *self), self->pathclear() );
		
		camera.type = ENT_CAMERA;
		camera.eyeheight = 2;
		camera.maxspeed = 100;
	}
	
	void initcamera() //give us some basic values ???
	{
		camera.o = cl.player1->o;
		camera.yaw = cl.player1->yaw;
		camera.pitch = cl.player1->pitch;
		init = true;
	}
	
	void pathstart() 
	{ 
		mode = true;
		curdirdur = 0;
		movement = MOVE_TRGT_PATH;
		if(!init)
		{
			lasttarget = cl.player1->o;
			vecfromyawpitch(cl.player1->yaw, cl.player1->pitch, 1, 0, lastdir);
		}
		else
		{
			//use first target for lastdir
			vec dir (path[0]->o);
			lastdir = dir.sub(camera.o);
		}
	}

	void pathappend(int n) 
	{ 
		int e;
		if ( (e = cl.et.findent(CAMERA, ATTR1, n)) >= 0 ) 
		{ 
			movieentity* node = cl.et.ents[e];
			path.add(node);
		}
	}

	//~ void pathpop() { delete path.remove(0); }
	//~ void pathdrop() { path.drop(); }  
	
	void pathclear() { path.setsize(0); } //path.deletecontentsp(); } 
	
	bool cameramode() { return mode; }
	
	void togglecamera()
	{
		mode = !mode;
	}
	
	void setposition(float x, float y, float z) 
	{
		if (!init) initcamera();
		extern physent *camera1;
		camera.o = vec(x,y,z);
		camera1 = &camera;
	}
	
	void setangle(float y, float p, bool flag = false) 
	{
		if(!init) initcamera();
		if (!flag)
		{
			yaw = y;
			pitch = p;
			rotation = ROT_FIX;
		}
		else
		{
			//ToDo set relative angle
			//ROT_DIR
			//set yaw and pitch
		}
	}
	
	void setfov(float _fov)
	{
		if(!init) initcamera();
		fov = _fov;
		zooming = ZOOM_FIX;
	}
	
	void setspeed(int _speed)
	{
		camera.maxspeed = _speed;
	}
	
	void setdirdur(int millis) { dirdur = millis; }
	void setfovdur(int millis) { fovdur = millis; }
	void setrotdur(int millis) { rotdur = millis; }
	
	void switchcamera(int n, int shouldrot) 
	{
		if(!init) initcamera();
		int e;
		if ( (e = cl.et.findent(CAMERA, ATTR1, n)) >= 0 ) 
		{ 
			reset();
			extern physent *camera1;
			camera.o = cl.et.ents[e]->o;
			if (shouldrot) 
			{
				yaw = cl.et.ents[e]->attr2;
				pitch = cl.et.ents[e]->attr3;
			}
			cl.player1->yaw = camera1->yaw;
			cl.player1->pitch = camera1->pitch;
			mode = CAM_ON;
			movement = MOVE_NONE;
			rotation = ROT_FIX;
		}
	}
	
	void move(int n) 
	{
		int e;
		if ( (e = cl.et.findent(CAMERA, ATTR1, n)) >= 0 ) 
		{ 
			if (!mode) { reset(); }
			camerapos = cl.et.ents[e];
			mode = CAM_ON;
			movement = MOVE_TRGT_ENT;
			if (rotation == ROT_NONE) { rotation = ROT_DIR; }
		}
	}
	
	void rotate(float _yaw, float _pitch)
	{
		rotation = ROT_DUR;
		mode = CAM_ON;
		panyaw.start(_yaw, lastmillis, rotdur);
		panpitch.start(_pitch, lastmillis, rotdur);
	}

	void zoom(float _fov)
	{
		zooming = ZOOM_DUR;
		mode = CAM_ON;
		panzoom.start(_fov, lastmillis, fovdur);
	}
	
	void camerafocusplayer(char* s) 
	{
		if (!mode) { reset(); }
		int cn = s[0] ? cl.cc.parseplayer(s) : -1;
		if (cn < 0) { return; }
		tracktrgt = cl.getclient(cn);
		rotation = ROT_TARGT_PHYSENT;
		mode = CAM_ON;
	}
	
	void camerafollow(char *s) {} //add relative position
	
	void reset()
	{
		mode = CAM_OFF;
		movement = MOVE_NONE;
		rotation = ROT_NONE;
		camera.o = cl.player1->o;
	}
	
	void updatecamera(int curtime, int lastmillis)
	{
		if(!mode) return;
		if (!init)
		{
			initcamera();
		}
		
		extern physent* camera1;
		camera1 = &camera;
		
		if (movement == MOVE_TRGT_ENT)
		{
			vec target = camerapos->o;
			float dist = target.dist(vec(camera.o));
			if (dist < 15) 
			{
				camera.move = camera.strafe = 0;
				movement = MOVE_NONE;
				camerapos = NULL;
				return;
			}
			vec dir (target);
			dir.sub(camera.o);
			vectoyawpitch(dir, camera.yaw, camera.pitch);
			camera.move = 1;
			moveplayer(&camera, 1, false);
		}
		
		if (movement == MOVE_TRGT_PATH)
		{
			if (pathidx >= path.length()) //reached end
			{
				camera.move = camera.strafe = 0;
				movement = MOVE_NONE;
				pathidx = 0;
				return;
			}
			
			vec target = path[pathidx]->o; //our target
			vec dir (target);
			
			static float lastdist; //ToDo use lastdist
			float dist = target.dist(vec(camera.o));
			if (dist < 0.5)  { lasttarget = path[pathidx]->o; pathidx++; curdirdur = 0; return; } //new target reached
			
			if (curdirdur <= dirdur) 
			{ 
				curdirdur += curtime;
				
				vec ndir = target; //new direction
				ndir.sub(camera.o);
				ndir.normalize();
				
				vec odir = lastdir; //olddirection
				odir.normalize();
				
				ndir.mul((float)curdirdur/(float)dirdur);
				odir.mul(1 - (float)curdirdur/(float)dirdur); 
				vec actdir = odir;
				actdir.add(ndir);
				vectoyawpitch(actdir, camera.yaw, camera.pitch);
			}
			else
			{
				dir.sub(camera.o);
				vectoyawpitch(dir, camera.yaw, camera.pitch);
				lastdir = dir;        
			}
			
			camera.move = 1;
			moveplayer(&camera, 1, false);
		}
		
		if (rotation == ROT_FIX) 
		{
			camera1 = &camera;
			camera.yaw = yaw;
			camera.pitch = pitch;
		}
		
		if (rotation == ROT_DUR)
		{
			camera1 = &camera;
			if (panyaw.state == TRANSITION_IDLE || panpitch.state == TRANSITION_IDLE) 
			{ 
				rotation = ROT_FIX; return; 
			}
			yaw = panyaw.getnext(yaw, lastmillis);
			pitch = panpitch.getnext(pitch, lastmillis);
			//ToDo bound degree
			//~ conoutf("__debug__: %s:%d %f %f", __FILE__,__LINE__, yaw, pitch); 
			camera.yaw = yaw;
			camera.pitch = pitch;
		}
		
		if (rotation == ROT_DIR) 
		{
			camera1 = &camera;
			vectoyawpitch(camera1->vel, camera1->yaw, camera1->pitch);
		}
		
		if(rotation == ROT_TARGT_PHYSENT)
		{
			camera1 = &camera;
			vec target = tracktrgt->o;
			vec dir (target);
			dir.sub(camera1->o);
			vectoyawpitch(dir, camera1->yaw, camera1->pitch);
		}
		if( zooming == ZOOM_FIX ) 
		{ 
			s_sprintfd(s)("fov %d", (int)fov); execute(s);
			zooming = ZOOM_NONE;
		}
		
		if( zooming == ZOOM_DUR ) 
		{ 
			if (panzoom.state == TRANSITION_IDLE) 
			{ 
				zooming = ZOOM_NONE; return; 
			}
			fov = panzoom.getnext(fov, lastmillis);
			s_sprintfd(s)("fov %d", (int)fov); execute(s); 
		}		
	}
};

//~ void update(int lastmillis)
//~ {
	//~ if(!mode) return;
	//~ static int oldmillis = 0;
	//~ if (movement)
	//~ {
		//~ cameramovement.nextstep()
		//~ moveplayer();
	//~ }
	
	//~ if(rotation)
	//~ {
		//~ camerarotation.nextstep()
		//~ camera.yaw = camerarotation.getyaw();
		//~ camera.pitch = camerarotation.getyaw();
	//~ }

	//~ if(zooming)
	//~ {
	
	//~ }
	//~ oldmillis = lastmillis;
//~ }
