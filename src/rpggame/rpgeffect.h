/***********************************************************************************
* this is an effect system I just dreamt up one night, lost a bit of sleep...      *
* Simply you have an effect that applies to a player, and each effect have various *
* stages to which it progresses, after reaching the final stage, and doing it's    *
* job, to the best of it's definitions, it ends. and deletes the reference to the  *
* dynent. -Hirato Kirata
***********************************************************************************/

enum
{
	FX_NONE = 0,
	FX_SHOCKWAVE = 1<<0, //shockwaves reach the current max rad, ie a rapid flux to the end that repeats during the process
	FX_SPIRAL = 1<<1, //circles the hit item
	FX_GRADCOLOUR = 1<<2, //gradually changes to the next colour, otherwise just pop to it when reaching the next stage
	FX_GRADSIZE = 1<<3, //gradual particle size changes
	FX_SURROUND = 1<<4, //shoots tendrils that puts the object in a diamond, if not flare, it sets the amount of instances surrounding the player, although only applicable to certain effects
};

struct rpgfxstage //each stage, it describes how the effect gets to the next stage
{
	float partsize;
	int properties, /*gravity,*/ height, endheight, radius, endradius, colour, particle, transtime, tendrils, spirals, shockwaves;
	
	rpgfxstage()
	{
		properties = FX_GRADCOLOUR|FX_GRADSIZE;
		radius = endradius = 100; //percentage radius
		height = endheight = 50; //percentage height
		//gravity = 0; //note particle gravity, const / gravity, higher values are slower, and 0 constitutes no movement, for when we import it into the main trunk
		spirals = shockwaves = partsize = 2;
		colour = 0xFFFFFF;
		particle = PART_FIREBALL3;
		transtime = 2000;
		tendrils = 6;
	}
	~rpgfxstage() {}
};

struct rpgeffect //handles the stages of the effects
{
	vector<rpgfxstage *> stages;
	
	//these respond to the particle's trail/projectile
	float trailsize,
		projsize;
		
	int trailpart, trailcol,
		projpart, projcol,
		vel, rad;
	
	rpgeffect()
	{
		stages.add(new rpgfxstage());
		
		trailpart = PART_STEAM;
		trailcol = 0x7F7F7F;
		trailsize = 1.0f;
		projpart = PART_FIREBALL1;
		projcol = 0xFFBF00;
		projsize = 4;
		vel = 200;
		rad = 32;
	}
	~rpgeffect() {stages.setsize(0);}
};

struct rpgfxowner //tracks the effect and the victim that befall it
{
	rpgeffect &fx;
	rpgent &owner;
	int starttime;
	
	rpgfxowner(rpgeffect &_fx, rpgent &_owner) : fx(_fx), owner(_owner)
	{
		starttime = lastmillis;
	}
	~rpgfxowner() {}
};

struct rpgfx //handles drawing and rendering
{
	rpgclient &cl;

	vector<rpgeffect *> rpgeffects;
	vector<rpgfxowner *> rpgownfx;
	
	rpgfx(rpgclient &_cl) : cl(_cl)
	{
		rpgeffects.setsize(0);
		rpgownfx.setsize(0);
		
		CCOMMAND(rfx_new, "", (rpgfx *self), self->rpgeffects.add(new rpgeffect););
		CCOMMAND(rfx_addstage, "", (rpgfx *self),
			if(self->rpgeffects.length())
				self->rpgeffects[self->rpgeffects.length()-1]->stages.add(new rpgfxstage());
			else
				conoutf("error: no effects (use rfx_new)");
		);
		//simplifies variable configuration commands
		#define N(n, f, t) CCOMMAND(rfx_eff_##n, f, (rpgfx *self, t *i), \
			if(self->rpgeffects.length()) \
			{ \
				rpgfxstage &fx = *self->rpgeffects[self->rpgeffects.length()-1]->stages[self->rpgeffects[self->rpgeffects.length()-1]->stages.length()-1]; \
				fx.n = *i; \
			} \
			else \
				conoutf("error: no effects (use rfx_new)"); \
		)
		#define NI(n) N(n, "i", int)
		
		NI(properties);
		NI(radius);
		NI(endradius);
		NI(colour);
		NI(transtime);
		NI(tendrils);
		NI(spirals);
		NI(shockwaves);
		NI(height);
		NI(endheight);
		NI(particle);
		//NI(gravity);
		
		#define NF(n) N(n, "f", float)
		
		NF(partsize);
		
		#undef N
		
		#define N(n, f, t) CCOMMAND(rfx_proj_##n, f, (rpgfx *self, t *i), \
			if(self->rpgeffects.length()) \
			{ \
				rpgeffect &fx = *self->rpgeffects[self->rpgeffects.length()-1]; \
				fx.n = *i; \
			} \
			else \
				conoutf("error: no effects (use rfx_new)"); \
		)
		
		NI(trailpart);
		NI(trailcol);
		NI(projpart);
		NI(projcol);
		NI(vel);
		NI(rad);
		
		#undef NI
		
		NF(trailsize);
		NF(projsize);
		
		#undef NF
		#undef N
	}
	~rpgfx() {}
	
	//unceomment for debugging only
	/*void printvars(rpgfxstage &stage)
	{
		conoutf("%f %i %i %i %i %i %.6X %i %i %i %i %i", stage.partsize, stage.properties, stage.height, stage.endheight, stage.radius, stage.endradius, stage.colour, stage.particle, stage.transtime, stage.tendrils, stage.spirals, stage.shockwaves);
	} */
	
	void addfx(int i, rpgent &owner)
	{
		if(rpgeffects.inrange(i))
			rpgownfx.add(new rpgfxowner(*rpgeffects[i], owner));
		else if(rpgeffects.length())
			rpgownfx.add(new rpgfxowner(*rpgeffects[0], owner));
		else
			conoutf("error: no effects declared");
	}
	
	void updatefx()
	{
		loopv(rpgeffects)
		{
			loopvj(rpgeffects[i]->stages)
			{
				int &props = rpgeffects[i]->stages[j]->properties;
				
				//here we do some optimising filtering of it's properties parameter to make it more efficient for the rendering part
				if(props&FX_SHOCKWAVE)
				{
					
				}
			}
		}

		loopv(rpgownfx)
		{
			rpgfxowner &own = *rpgownfx[i];
			int time = own.starttime;
			bool end = false;
			rpgfxstage current, next;
			loopvj(own.fx.stages)
			{
				if(time+own.fx.stages[j]->transtime > lastmillis) //this stage is currently in progress
				{
					current = *own.fx.stages[j];
					
					if(own.fx.stages.inrange(j+1))
						next = *own.fx.stages[j+1];
					else
						next = *own.fx.stages[j]; //ie, next and current are identical;
					break;
				}
				else
				{
					if(own.fx.stages.inrange(j+1))
						time += max(0, own.fx.stages[j]->transtime);
					else//no stages left :(
						end = true; //no point issueing a break here, we're at the end of the vector
				}
			}
			
			if(end){rpgownfx.remove(i); i--; continue;}
			
			float progress = (lastmillis - time) / (float) current.transtime;
			
			
			float radius = current.radius + (current.endradius-current.radius)*progress,
				size = current.partsize;
			int colour = current.colour,
				fadetime = 1000; //FIXME, dynamic values for fade time, mostly just for BIG rpgents
				
			if(current.properties&FX_GRADCOLOUR && current.colour != next.colour) //memo to self, clean
			{
				int red1 = current.colour&0xFF0000,
					green1 = current.colour&0x00FF00,
					blue1 = current.colour&0x0000FF,
					red2 = next.colour&0xFF0000,
					green2 = next.colour&0x00FF00,
					blue2 = next.colour&0x0000FF;
					
					red1 += (red2-red1)*progress,
					green1 += (green2-green1)*progress,
					blue1 += (blue2-blue1)*progress;
					
					colour = (red1&0xFF0000)|(green1&0xFF00)|(blue1&0xFF);
			}
			
			if(current.properties&FX_GRADSIZE && current.partsize != next.partsize)
				size += (next.partsize-current.partsize) * progress;
				
			//drawing code functions are called here, but will be kept outside for clarity //these are TEMPORARY dummies, just something to test and please the eyes while the real stuffs being done so...
			//FIXME
			
			vec pos = vec(own.owner.o).sub(vec(0.f, 0.0f, own.owner.eyeheight-own.owner.radius));
			if(current.properties&FX_SURROUND)
				loopi(current.tendrils) {regularshape(current.particle, own.owner.radius * radius/100, colour, 12, 1, fadetime, pos, size, 200);}
			else
				regularshape(current.particle, own.owner.radius * radius/100, colour, 12, 1, fadetime, pos, size, 200);
		}
	}
};