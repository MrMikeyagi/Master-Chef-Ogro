struct vec4;

struct vec
{
    union
    {
        struct { float x, y, z; };
        float v[3];
    };

    vec() {}
    explicit vec(int a) : x(a), y(a), z(a) {} 
    explicit vec(float a) : x(a), y(a), z(a) {} 
    vec(float a, float b, float c) : x(a), y(b), z(c) {}
    vec(int v[3]) : x(v[0]), y(v[1]), z(v[2]) {}
    vec(float *v) : x(v[0]), y(v[1]), z(v[2]) {}
    explicit vec(const vec4 &v);

    vec(float yaw, float pitch) : x(sinf(yaw)*cosf(pitch)), y(-cosf(yaw)*cosf(pitch)), z(sinf(pitch)) {}

    float &operator[](int i)       { return v[i]; }
    float  operator[](int i) const { return v[i]; }
    
    vec &set(int i, float f) { v[i] = f; return *this; }

    bool operator==(const vec &o) const { return x == o.x && y == o.y && z == o.z; }
    bool operator!=(const vec &o) const { return x != o.x || y != o.y || z != o.z; }

    bool iszero() const { return x==0 && y==0 && z==0; }
    float squaredlen() const { return x*x + y*y + z*z; }
    float dot(const vec &o) const { return x*o.x + y*o.y + z*o.z; }
    vec &mul(float f)        { x *= f; y *= f; z *= f; return *this; }
    vec &div(float f)        { x /= f; y /= f; z /= f; return *this; }
    vec &add(const vec &o)   { x += o.x; y += o.y; z += o.z; return *this; }
    vec &add(float f)        { x += f; y += f; z += f; return *this; }
    vec &sub(const vec &o)   { x -= o.x; y -= o.y; z -= o.z; return *this; }
    vec &sub(float f)        { x -= f; y -= f; z -= f; return *this; }
    vec &neg()               { return mul(-1); }
    float magnitude() const  { return sqrtf(squaredlen()); }
    vec &normalize()         { div(magnitude()); return *this; }
    bool isnormalized() const { float m = squaredlen(); return (m>0.99f && m<1.01f); }
    float squaredist(const vec &e) const { return vec(*this).sub(e).squaredlen(); }
    float dist(const vec &e) const { vec t; return dist(e, t); }
    float dist(const vec &e, vec &t) const { t = *this; t.sub(e); return t.magnitude(); }
    bool reject(const vec &o, float max) { return x>o.x+max || x<o.x-max || y>o.y+max || y<o.y-max; }
    vec &cross(const vec &a, const vec &b) { x = a.y*b.z-a.z*b.y; y = a.z*b.x-a.x*b.z; z = a.x*b.y-a.y*b.x; return *this; }
    vec &reflect(const vec &n) { float k = 2*dot(n); x -= k*n.x; y -= k*n.y; z -= k*n.z; return *this; }
    vec &project(const vec &n) { float k = dot(n); x -= k*n.x; y -= k*n.y; z -= k*n.z; return *this; }
    vec &projectxydir(const vec &n) { if(n.z) z = -(x*n.x/n.z + y*n.y/n.z); return *this; }
    vec &projectxy(const vec &n)
    {
        float m = squaredlen(), k = dot(n);
        projectxydir(n);
        rescale(sqrtf(max(m - k*k, 0.0f)));
        return *this;
    }
    vec &projectxy(const vec &n, float threshold)
    {
        float m = squaredlen(), k = min(dot(n), threshold);
        projectxydir(n);
        rescale(sqrtf(max(m - k*k, 0.0f)));
        return *this;
    }
    void lerp(const vec &a, const vec &b, float t) { x = a.x*(1-t)+b.x*t; y = a.y*(1-t)+b.y*t; z = a.z*(1-t)+b.z*t; }

    vec &rescale(float k)
    {
        float mag = magnitude();
        if(mag > 1e-6f) mul(k / mag);
        return *this;
    }

    vec &rotate_around_z(float angle) { *this = vec(cosf(angle)*x-sinf(angle)*y, cosf(angle)*y+sinf(angle)*x, z); return *this; }
    vec &rotate_around_x(float angle) { *this = vec(x, cosf(angle)*y-sinf(angle)*z, cosf(angle)*z+sinf(angle)*y); return *this; }
    vec &rotate_around_y(float angle) { *this = vec(cosf(angle)*x-sinf(angle)*z, y, cosf(angle)*z+sinf(angle)*x); return *this; }

    vec &rotate(float angle, const vec &d)
    {
        float c = cosf(angle), s = sinf(angle);
        return rotate(c, s, d);
    }

    vec &rotate(float c, float s, const vec &d)
    {
        *this = vec(x*(d.x*d.x*(1-c)+c) + y*(d.x*d.y*(1-c)-d.z*s) + z*(d.x*d.z*(1-c)+d.y*s),
                    x*(d.y*d.x*(1-c)+d.z*s) + y*(d.y*d.y*(1-c)+c) + z*(d.y*d.z*(1-c)-d.x*s),
                    x*(d.x*d.z*(1-c)-d.y*s) + y*(d.y*d.z*(1-c)+d.x*s) + z*(d.z*d.z*(1-c)+c));
        return *this;
    }

    void orthogonal(const vec &d)
    {
        int i = fabs(d.x) > fabs(d.y) ? (fabs(d.x) > fabs(d.z) ? 0 : 2) : (fabs(d.y) > fabs(d.z) ? 1 : 2); 
        v[i] = d[(i+1)%3];
        v[(i+1)%3] = -d[i];
        v[(i+2)%3] = 0;
    }

    void orthonormalize(vec &s, vec &t) const
    {
        s.sub(vec(*this).mul(dot(s)));
        t.sub(vec(*this).mul(dot(t)))
         .sub(vec(s).mul(s.dot(t)));
    }

    template<class T> float dist_to_bb(const T &min, const T &max) const
    {
        float sqrdist = 0;
        loopi(3)
        {
            if     (v[i] < min[i]) { float delta = v[i]-min[i]; sqrdist += delta*delta; }
            else if(v[i] > max[i]) { float delta = max[i]-v[i]; sqrdist += delta*delta; }
        }
        return sqrtf(sqrdist);
    }

    template<class T, class S> float dist_to_bb(const T &o, S size) const
    {
        return dist_to_bb(o, T(o).add(size));
    }
};

static inline bool htcmp(const vec &x, const vec &y)
{
    return x == y;
}

static inline uint hthash(const vec &k)
{
    const int *v = (const int *)k.v;
    return v[0]^v[1]^v[2];
}

struct vec4
{
    union
    {
        struct { float x, y, z, w; };
        float v[4];
    };

    vec4() {}
    explicit vec4(const vec &p, float w = 0) : x(p.x), y(p.y), z(p.z), w(w) {}
    vec4(float x, float y, float z, float w) : x(x), y(y), z(z), w(w) {}

    float &operator[](int i)       { return v[i]; }
    float  operator[](int i) const { return v[i]; }

    float dot3(const vec4 &o) const { return x*o.x + y*o.y + z*o.z; }
    float dot3(const vec &o) const { return x*o.x + y*o.y + z*o.z; }
    float dot(const vec4 &o) const { return dot3(o) + w*o.w; }
    float dot(const vec &o) const  { return x*o.x + y*o.y + z*o.z + w; }
    float squaredlen() const { return dot(*this); }
    float magnitude() const  { return sqrtf(squaredlen()); }
    float magnitude3() const { return sqrtf(dot3(*this)); }
    vec4 &normalize() { mul(1/magnitude()); return *this; }

    void lerp(const vec4 &a, const vec4 &b, float t) 
    { 
        x = a.x*(1-t)+b.x*t; 
        y = a.y*(1-t)+b.y*t; 
        z = a.z*(1-t)+b.z*t;
        w = a.w*(1-t)+b.w*t;
    }

    vec4 &mul3(float f)       { x *= f; y *= f; z *= f; return *this; }
    vec4 &mul(float f)       { mul3(f); w *= f; return *this; }
    vec4 &add(const vec4 &o) { x += o.x; y += o.y; z += o.z; w += o.w; return *this; }
    vec4 &neg3()             { x = -x; y = -y; z = -z; return *this; }
    vec4 &neg()              { neg3(); w = -w; return *this; }
};

inline vec::vec(const vec4 &v) : x(v.x), y(v.y), z(v.z) {}

struct matrix3x3;
struct matrix3x4;

struct quat : vec4
{
    quat() {}
    quat(float x, float y, float z, float w) : vec4(x, y, z, w) {}
    quat(const vec &axis, float angle)
    {
        w = cosf(angle/2);
        float s = sinf(angle/2);
        x = s*axis.x;
        y = s*axis.y;
        z = s*axis.z;
    }
    explicit quat(const matrix3x3 &m) { convertmatrix(m); }
    explicit quat(const matrix3x4 &m) { convertmatrix(m); }

    void restorew() { w = 1.0f-x*x-y*y-z*z; w = w<0 ? 0 : -sqrtf(w); }
    
    void add(const vec4 &o) { vec4::add(o); }
    void mul(float k) { vec4::mul(k); }

    void mul(const quat &p, const quat &o)
    {
        x = p.w*o.x + p.x*o.w + p.y*o.z - p.z*o.y;
        y = p.w*o.y - p.x*o.z + p.y*o.w + p.z*o.x;
        z = p.w*o.z + p.x*o.y - p.y*o.x + p.z*o.w;
        w = p.w*o.w - p.x*o.x - p.y*o.y - p.z*o.z;
    }
    void mul(const quat &o) { mul(quat(*this), o); }

    void mul(const quat &p, const vec &o)
    {
        x =  p.w*o.x + p.y*o.z - p.z*o.y;
        y =  p.w*o.y - p.x*o.z + p.z*o.x;
        z =  p.w*o.z + p.x*o.y - p.y*o.x;
        w = -p.x*o.x - p.y*o.y - p.z*o.z;
    }
    void mul(const vec &o) { mul(quat(*this), o); }

    quat &invert() { neg3(); return *this; }

    void calcangleaxis(float &angle, vec &axis)
    {
        float rr = dot3(*this);
        if(rr>0)
        {
            angle = 2*acosf(w);
            axis = vec(x, y, z).mul(1/rr); 
        }
        else { angle = 0; axis = vec(0, 0, 1); }
    }

    void slerp(const quat &from, const quat &to, float t)
    {
        float cosomega = from.dot(to), fromk, tok = 1;
        if(cosomega<0) { cosomega = -cosomega; tok = -1; }

        if(cosomega > 1 - 1e-6) { fromk = 1-t; tok *= t; }
        else
        {
            float omega = acosf(cosomega), recipsinomega = 1/sinf(omega);
            fromk = sinf((1-t)*omega)*recipsinomega;
            tok *= sinf(t*omega)*recipsinomega;
        }

        loopi(4) v[i] = from[i]*fromk + to[i]*tok;
    }

    vec rotate(const vec &v) const
    {
        vec t1, t2;
        t1.cross(vec(*this), v);
        t2.cross(vec(*this), t1);
        t1.mul(w).add(t2).mul(2).add(v);
        return t1;
#if 0
        quat inv(*this);
        inv.invert();
        inv.normalize();
        quat tmp;
        tmp.mul(*this, v);
        tmp.mul(inv);
        return vec(tmp.x, tmp.y, tmp.z);
#endif
    }

    template<class M>
    void convertmatrix(const M &m)
    {
        float trace = m.a.x + m.b.y + m.c.z;
        if(trace>0)
        {
            float r = sqrtf(1 + trace), inv = 0.5f/r;
            w = 0.5f*r;
            x = (m.c.y - m.b.z)*inv;
            y = (m.a.z - m.c.x)*inv;
            z = (m.b.x - m.a.y)*inv;
        }
        else if(m.a.x > m.b.y && m.a.x > m.c.z)
        {
            float r = sqrtf(1 + m.a.x - m.b.y - m.c.z), inv = 0.5f/r;
            x = 0.5f*r;
            y = (m.b.x + m.a.y)*inv;
            z = (m.a.z + m.c.x)*inv;
            w = (m.c.y - m.b.z)*inv;
        }
        else if(m.b.y > m.c.z)
        {
            float r = sqrtf(1 + m.b.y - m.a.x - m.c.z), inv = 0.5f/r;
            x = (m.b.x + m.a.y)*inv;
            y = 0.5f*r;
            z = (m.c.y + m.b.z)*inv;
            w = (m.a.z - m.c.x)*inv;
        }
        else
        {
            float r = sqrtf(1 + m.c.z - m.a.x - m.b.y), inv = 0.5f/r;
            x = (m.a.z + m.c.x)*inv;
            y = (m.c.y + m.b.z)*inv;
            z = 0.5f*r;
            w = (m.b.x - m.a.y)*inv;
        }
    }
};

struct dualquat
{
    quat real, dual;

    dualquat() {}
    dualquat(const quat &q, const vec &p) 
        : real(q),
          dual(0.5f*( p.x*q.w + p.y*q.z - p.z*q.y),
               0.5f*(-p.x*q.z + p.y*q.w + p.z*q.x),
               0.5f*( p.x*q.y - p.y*q.x + p.z*q.w),
              -0.5f*( p.x*q.x + p.y*q.y + p.z*q.z))
    {
    }
    explicit dualquat(const quat &q) : real(q), dual(0, 0, 0, 0) {}
    explicit dualquat(const matrix3x4 &m);

    dualquat &mul(float k) { real.mul(k); dual.mul(k); return *this; }
    dualquat &add(const dualquat &d) { real.add(d.real); dual.add(d.dual); return *this; }

    void lerp(const dualquat &from, const dualquat &to, float t)
    {
        float a = 1-t, b = from.real.dot(to.real)<0 ? -t : t;
        loopi(4)
        {
            real[i] = from.real[i]*a + to.real[i]*b;
            dual[i] = from.dual[i]*a + to.dual[i]*b;
        }
    }

    dualquat &invert()
    {
        float rr = real.squaredlen();
        if(rr > 0)
        {
            float invrr = 1/rr,
                  invrd = -2*real.dot(dual)*invrr*invrr;

            dual.mul3(-invrr);
            dual.w *= invrr;
            quat tmp(real);
            tmp.mul3(-invrd);
            tmp.w *= invrd;
            dual.add(tmp);

            real.mul3(-invrr);
            real.w *= invrr;
        }
        else { real = dual = quat(0, 0, 0, 0); }
        return *this;
    }
    
    void mul(const dualquat &p, const dualquat &o)
    {
        real.mul(p.real, o.real);
        dual.mul(p.real, o.dual);
        quat tmp;
        tmp.mul(p.dual, o.real);
        dual.add(tmp);
    }       
    void mul(const dualquat &o) { mul(dualquat(*this), o); }    
  
    void mulorient(const quat &q)
    {
        real.mul(q, quat(real));
        dual.mul(quat(q).invert(), quat(dual));
    }

    void normalize()
    {
        float invlen = 1/real.magnitude();
        real.mul(invlen);
        dual.mul(invlen);
    }

    void translate(const vec &p)
    {
        dual.x +=  0.5f*( p.x*real.w + p.y*real.z - p.z*real.y);
        dual.y +=  0.5f*(-p.x*real.z + p.y*real.w + p.z*real.x);
        dual.z +=  0.5f*( p.x*real.y - p.y*real.x + p.z*real.w);
        dual.w += -0.5f*( p.x*real.x + p.y*real.y + p.z*real.z);
    }

    void scale(float k)
    {
        dual.mul(k);
    }

    void fixantipodal(const dualquat &d)
    {
        if(real.dot(d.real) < 0)
        {
            real.neg();
            dual.neg();
        }
    }

    void accumulate(const dualquat &d, float k)
    {
        real.add(vec4(d.real).mul(k));
        dual.add(vec4(d.dual).mul(k));
    }

    vec transform(const vec &v) const
    {
        vec t1, t2;
        t1.cross(vec(real), v);
        t1.add(vec(v).mul(real.w));
        t2.cross(vec(real), t1);

        vec t3;
        t3.cross(vec(real), vec(dual));
        t3.add(vec(dual).mul(real.w));
        t3.sub(vec(real).mul(dual.w));

        t2.add(t3).mul(2).add(v);

        return t2;
    }
};

struct matrix3x3
{
    vec a, b, c;

    matrix3x3() {}
    matrix3x3(const vec &a, const vec &b, const vec &c) : a(a), b(b), c(c) {}
    explicit matrix3x3(const quat &q)
    {
        float x = q.x, y = q.y, z = q.z, w = q.w,
              tx = 2*x, ty = 2*y, tz = 2*z,
              txx = tx*x, tyy = ty*y, tzz = tz*z,
              txy = tx*y, txz = tx*z, tyz = ty*z,
              twx = w*tx, twy = w*ty, twz = w*tz;
        a = vec(1 - (tyy + tzz), txy - twz, txz + twy);
        b = vec(txy + twz, 1 - (txx + tzz), tyz - twx);
        c = vec(txz - twy, tyz + twx, 1 - (txx + tyy));
    }

    void mul(const matrix3x3 &m, const matrix3x3 &n)
    {
        a = vec(m.a.dot(vec(n.a.x, n.b.x, n.c.x)),
                m.a.dot(vec(n.a.y, n.b.y, n.c.y)),
                m.a.dot(vec(n.a.z, n.b.z, n.c.z)));
        b = vec(m.b.dot(vec(n.a.x, n.b.x, n.c.x)),
                m.b.dot(vec(n.a.y, n.b.y, n.c.y)),
                m.b.dot(vec(n.a.z, n.b.z, n.c.z)));
        c = vec(m.c.dot(vec(n.a.x, n.b.x, n.c.x)),
                m.c.dot(vec(n.a.y, n.b.y, n.c.y)),
                m.c.dot(vec(n.a.z, n.b.z, n.c.z)));
    }
    void mul(const matrix3x3 &n) { mul(matrix3x3(*this), n); }

    void transpose(const matrix3x3 &o)
    {
        a = vec(o.a.x, o.b.x, o.c.x);
        b = vec(o.a.y, o.b.y, o.c.y);
        c = vec(o.a.z, o.b.z, o.c.z);
    }

    void rotate(float angle, const vec &axis)
    {
        rotate(cosf(angle), sinf(angle), axis);
    }

    void rotate(float ck, float sk, const vec &axis)
    {
        a = vec(axis.x*axis.x*(1-ck)+ck, axis.x*axis.y*(1-ck)-axis.z*sk, axis.x*axis.z*(1-ck)+axis.y*sk);
        b = vec(axis.y*axis.x*(1-ck)+axis.z*sk, axis.y*axis.y*(1-ck)+ck, axis.y*axis.z*(1-ck)-axis.x*sk);
        c = vec(axis.x*axis.z*(1-ck)-axis.y*sk, axis.y*axis.z*(1-ck)+axis.x*sk, axis.z*axis.z*(1-ck)+ck);
    }

    void calcangleaxis(float &angle, vec &axis)
    {
        angle = acosf(clamp(0.5f*(a.x + b.y + c.z - 1), -1.0f, 1.0f));

		if(angle <= 0) axis = vec(0, 0, 1);
        else if(angle < M_PI) axis = vec(c.y - b.z, a.z - c.x, b.x - a.y).normalize();
        else if(a.x >= b.y && a.x >= c.z)
        {
            float r = sqrtf(1 + a.x - b.y - c.z), inv = 1/r;
            axis.x = 0.5f*r;
            axis.y = a.y*inv;
            axis.z = a.z*inv;
        }
        else if(b.y >= c.z)
        {
            float r = sqrtf(1 + b.y - a.x - c.z), inv = 1/r;
            axis.y = 0.5f*r;
            axis.x = a.y*inv;
            axis.z = b.z*inv;
        }
        else
        {
            float r = sqrtf(1 + b.y - a.x - c.z), inv = 1/r;
            axis.z = 0.5f*r;
            axis.x = a.z*inv;
            axis.y = b.z*inv;
        }
    }

    vec transform(const vec &o) const { return vec(a.dot(o), b.dot(o), c.dot(o)); }
    vec transposedtransform(const vec &o) const
    {
        return vec(a.x*o.x + b.x*o.y + c.x*o.z,
                   a.y*o.x + b.y*o.y + c.y*o.z,
                   a.z*o.x + b.z*o.y + c.z*o.z);
    }
};

struct matrix3x4
{
    vec4 a, b, c;
    
    matrix3x4() {}
    matrix3x4(const vec4 &x, const vec4 &y, const vec4 &z) : a(x), b(y), c(z) {}
    matrix3x4(const matrix3x3 &rot, const vec &trans)
     : a(rot.a, trans.x), b(rot.b, trans.y), c(rot.c, trans.z)
    {}
    matrix3x4(const dualquat &d)
    {
        float x = d.real.x, y = d.real.y, z = d.real.z, w = d.real.w, 
              ww = w*w, xx = x*x, yy = y*y, zz = z*z,
              xy = x*y, xz = x*z, yz = y*z,
              wx = w*x, wy = w*y, wz = w*z;
        a = vec4(ww + xx - yy - zz, 2*(xy - wz), 2*(xz + wy),
            -2*(d.dual.w*x - d.dual.x*w + d.dual.y*z - d.dual.z*y));
        b = vec4(2*(xy + wz), ww + yy - xx - zz, 2*(yz - wx),
            -2*(d.dual.w*y - d.dual.x*z - d.dual.y*w + d.dual.z*x));
        c = vec4(2*(xz - wy), 2*(yz + wx), ww + zz - xx - yy,
            -2*(d.dual.w*z + d.dual.x*y - d.dual.y*x - d.dual.z*w));

        float invrr = 1/d.real.dot(d.real);
        a.mul(invrr);
        b.mul(invrr);
        c.mul(invrr);
    }

    void scale(float k)
    {
        a.mul(k);
        b.mul(k);
        c.mul(k);
    }

    void translate(const vec &p)
    {
        a.w += p.x;
        b.w += p.y;
        c.w += p.z;
    }

    void accumulate(const matrix3x4 &m, float k)
    {
        a.add(vec4(m.a).mul(k));
        b.add(vec4(m.b).mul(k));
        c.add(vec4(m.c).mul(k));
    }

    void normalize()
    {
        a.mul3(1/a.magnitude3());
        b.mul3(1/b.magnitude3());
        c.mul3(1/c.magnitude3());
    }

    void lerp(const matrix3x4 &from, const matrix3x4 &to, float t)
    {
        loopi(4)
        {
            a[i] += to.a[i]*t + from.a[i]*(1-t);
            b[i] += to.b[i]*t + from.b[i]*(1-t);
            c[i] += to.c[i]*t + from.c[i]*(1-t);
        }
    }

    void identity()
    {
        a = vec4(1, 0, 0, 0);
        b = vec4(0, 1, 0, 0);
        c = vec4(0, 0, 1, 0);
    }

    void mul(const matrix3x4 &m, const matrix3x4 &n)
    {
        vec nx(n.a.x, n.b.x, n.c.x),
            ny(n.a.y, n.b.y, n.c.y),
            nz(n.a.z, n.b.z, n.c.z),
            nw(n.a.w, n.b.w, n.c.w);
        a = vec4(m.a.dot3(nx), m.a.dot3(ny), m.a.dot3(nz), m.a.dot(nw));
        b = vec4(m.b.dot3(nx), m.b.dot3(ny), m.b.dot3(nz), m.b.dot(nw));
        c = vec4(m.c.dot3(nx), m.c.dot3(ny), m.c.dot3(nz), m.c.dot(nw));
    }
    void mul(const matrix3x4 &n) { mul(matrix3x4(*this), n); }

    void mulorient(const matrix3x3 &m)
    {
        vec nx(a.x, b.x, c.x),
            ny(a.y, b.y, c.y),
            nz(a.z, b.z, c.z);
        a.x = m.a.dot(nx); a.y = m.a.dot(ny); a.z = m.a.dot(nz);
        b.x = m.b.dot(nx); b.y = m.b.dot(ny); b.z = m.b.dot(nz);
        c.x = m.c.dot(nx); c.y = m.c.dot(ny); c.z = m.c.dot(nz);
    }

    void transpose(const matrix3x4 &o)
    {
        a = vec4(o.a.x, o.b.x, o.c.x, -(o.a.x*o.a.w + o.b.x*o.b.w + o.c.x*o.c.w));
        b = vec4(o.a.y, o.b.y, o.c.y, -(o.a.y*o.a.w + o.b.y*o.b.w + o.c.y*o.c.w));
        c = vec4(o.a.z, o.b.z, o.c.z, -(o.a.z*o.a.w + o.b.z*o.b.w + o.c.z*o.c.w));
    }

    void transposemul(const matrix3x3 &rot, const vec &trans, const matrix3x4 &o)
    {
        a = vec4(o.a).mul(rot.a.x).add(vec4(o.b).mul(rot.b.x)).add(vec4(o.c).mul(rot.c.x));
        a.w += trans.x;
        b = vec4(o.a).mul(rot.a.y).add(vec4(o.b).mul(rot.b.y)).add(vec4(o.c).mul(rot.c.y));
        b.w += trans.y;
        c = vec4(o.a).mul(rot.a.z).add(vec4(o.b).mul(rot.b.z)).add(vec4(o.c).mul(rot.c.z));
        c.w += trans.z;
    } 

    void rotate(float angle, const vec &d)
    {
        rotate(cosf(angle), sinf(angle), d);
    }

    void rotate(float ck, float sk, const vec &d)
    {
        a = vec4(d.x*d.x*(1-ck)+ck, d.x*d.y*(1-ck)-d.z*sk, d.x*d.z*(1-ck)+d.y*sk, 0);
        b = vec4(d.y*d.x*(1-ck)+d.z*sk, d.y*d.y*(1-ck)+ck, d.y*d.z*(1-ck)-d.x*sk, 0);
        c = vec4(d.x*d.z*(1-ck)-d.y*sk, d.y*d.z*(1-ck)+d.x*sk, d.z*d.z*(1-ck)+ck, 0);
    }

    #define ROTVEC(V, m, n) \
    { \
        float m = V.m, n = V.n; \
        V.m = m*ck + n*sk; \
        V.n = n*ck - m*sk; \
    }

    void rotate_around_x(float angle)
    {
        float ck = cosf(angle), sk = sinf(angle);
        ROTVEC(a, y, z);
        ROTVEC(b, y, z);
        ROTVEC(c, y, z);
    }

    void rotate_around_y(float angle)
    {
        float ck = cosf(angle), sk = sinf(angle);
        ROTVEC(a, z, x);
        ROTVEC(b, z, x);
        ROTVEC(c, z, x);
    }

    void rotate_around_z(float angle)
    {
        float ck = cosf(angle), sk = sinf(angle);
        ROTVEC(a, x, y);
        ROTVEC(b, x, y);
        ROTVEC(c, x, y);
    }

    #undef ROTVEC

    vec transform(const vec &o) const { return vec(a.dot(o), b.dot(o), c.dot(o)); }
    vec transposedtransform(const vec &o) const
    {
        vec p = o;
        p.x -= a.w;
        p.y -= b.w;
        p.z -= c.w;
        return vec(a.x*p.x + b.x*p.y + c.x*p.z,
                   a.y*p.x + b.y*p.y + c.y*p.z,
                   a.z*p.x + b.z*p.y + c.z*p.z);
    }
    vec transformnormal(const vec &o) const { return vec(a.dot3(o), b.dot3(o), c.dot3(o)); }
    vec transposedtransformnormal(const vec &o) const
    {
        return vec(a.x*o.x + b.x*o.y + c.x*o.z,
                   a.y*o.x + b.y*o.y + c.y*o.z,
                   a.z*o.x + b.z*o.y + c.z*o.z);
    }

    float getscale() const { return a.magnitude3(); }
    vec gettranslation() const { return vec(a.w, b.w, c.w); }
};

inline dualquat::dualquat(const matrix3x4 &m) : real(m)
{
    dual.x =  0.5f*( m.a.w*real.w + m.b.w*real.z - m.c.w*real.y);
    dual.y =  0.5f*(-m.a.w*real.z + m.b.w*real.w + m.c.w*real.x);
    dual.z =  0.5f*( m.a.w*real.y - m.b.w*real.x + m.c.w*real.w);
    dual.w = -0.5f*( m.a.w*real.x + m.b.w*real.y + m.c.w*real.z);
}

struct plane : vec
{
    float offset;

    float dist(const vec &p) const { return dot(p)+offset; }
    bool operator==(const plane &p) const { return x==p.x && y==p.y && z==p.z && offset==p.offset; }
    bool operator!=(const plane &p) const { return x!=p.x || y!=p.y || z!=p.z || offset!=p.offset; }

    plane() {}
    plane(const vec &c, float off) : vec(c), offset(off) {} 
    plane(int d, float off)
    {
        x = y = z = 0.0f;
        v[d] = 1.0f;
        offset = -off;
    }
    plane(float a, float b, float c, float d) : vec(a, b, c), offset(d) {}

    void toplane(const vec &n, const vec &p)
    {
        x = n.x; y = n.y; z = n.z;
        offset = -dot(p);
    }

    bool toplane(const vec &a, const vec &b, const vec &c)
    {
        cross(vec(b).sub(a), vec(c).sub(a));
        float mag = magnitude();
        if(!mag) return false;
        div(mag);
        offset = -dot(a);
        return true;
    }

    bool rayintersect(const vec &o, const vec &ray, float &dist)
    {
        float cosalpha = dot(ray);
        if(cosalpha==0) return false;
        float deltac = offset+dot(o);
        dist -= deltac/cosalpha;
        return true;
    }

    plane &scale(float k)
    {
        mul(k);
        return *this;
    }

    plane &translate(const vec &p)
    {
        offset += dot(p);
        return *this;
    }

    float zintersect(const vec &p) const { return -(x*p.x+y*p.y+offset)/z; }
    float zdist(const vec &p) const { return p.z-zintersect(p); }
};

struct triangle
{
    vec a, b, c;

    triangle(const vec &a, const vec &b, const vec &c) : a(a), b(b), c(c) {}
    triangle() {}

    triangle &add(const vec &o) { a.add(o); b.add(o); c.add(o); return *this; }
    triangle &sub(const vec &o) { a.sub(o); b.sub(o); c.sub(o); return *this; }

    bool operator==(const triangle &t) const { return a == t.a && b == t.b && c == t.c; }
};

/**

sandbox uses 3 different linear coordinate systems
which are oriented around each of the axis dimensions.

So any point within the game can be defined by four coordinates: (d, x, y, z)

d is the reference axis dimension
x is the coordinate of the ROW dimension
y is the coordinate of the COL dimension
z is the coordinate of the reference dimension (DEPTH)

typically, if d is not used, then it is implicitly the Z dimension.
ie: d=z => x=x, y=y, z=z

**/

// DIM: X=0 Y=1 Z=2.
const int R[3]  = {1, 2, 0}; // row
const int C[3]  = {2, 0, 1}; // col
const int D[3]  = {0, 1, 2}; // depth

struct ivec
{
    union
    {
        struct { int x, y, z; };
        int v[3];
    };

    ivec() {}
    ivec(const vec &v) : x(int(v.x)), y(int(v.y)), z(int(v.z)) {}
    ivec(int i)
    {
        x = ((i&1)>>0);
        y = ((i&2)>>1);
        z = ((i&4)>>2);
    }
    ivec(int a, int b, int c) : x(a), y(b), z(c) {}
    ivec(int d, int row, int col, int depth)
    {
        v[R[d]] = row;
        v[C[d]] = col;
        v[D[d]] = depth;
    }
    ivec(int i, int cx, int cy, int cz, int size)
    {
        x = cx+((i&1)>>0)*size;
        y = cy+((i&2)>>1)*size;
        z = cz+((i&4)>>2)*size;
    }
    vec tovec() const { return vec(x, y, z); }
    int toint() const { return (x>0?1:0) + (y>0?2:0) + (z>0?4:0); }

    int &operator[](int i)       { return v[i]; }
    int  operator[](int i) const { return v[i]; }

    //int idx(int i) { return v[i]; }
    bool operator==(const ivec &v) const { return x==v.x && y==v.y && z==v.z; }
    bool operator!=(const ivec &v) const { return x!=v.x || y!=v.y || z!=v.z; }
    bool iszero() const { return x==0 && y==0 && z==0; }
    ivec &shl(int n) { x<<= n; y<<= n; z<<= n; return *this; }
    ivec &shr(int n) { x>>= n; y>>= n; z>>= n; return *this; }
    ivec &mul(int n) { x *= n; y *= n; z *= n; return *this; }
    ivec &div(int n) { x /= n; y /= n; z /= n; return *this; }
    ivec &add(int n) { x += n; y += n; z += n; return *this; }
    ivec &sub(int n) { x -= n; y -= n; z -= n; return *this; }
    ivec &add(const ivec &v) { x += v.x; y += v.y; z += v.z; return *this; }
    ivec &sub(const ivec &v) { x -= v.x; y -= v.y; z -= v.z; return *this; }
    ivec &mask(int n) { x &= n; y &= n; z &= n; return *this; }
    ivec &neg() { return mul(-1); }
    ivec &cross(const ivec &a, const ivec &b) { x = a.y*b.z-a.z*b.y; y = a.z*b.x-a.x*b.z; z = a.x*b.y-a.y*b.x; return *this; }
    int dot(const ivec &o) const { return x*o.x + y*o.y + z*o.z; }
    float dist(const plane &p) const { return x*p.x + y*p.y + z*p.z + p.offset; }
};

static inline bool htcmp(const ivec &x, const ivec &y)
{
    return x == y;
}  

static inline uint hthash(const ivec &k)
{
    return k.x^k.y^k.z;
}  

struct svec
{
    union
    {
        struct { ushort x, y, z; };
        ushort v[3];
    };

    svec() {}
    svec(ushort x, ushort y, ushort z) : x(x), y(y), z(z) {}

    ushort &operator[](int i)       { return v[i]; }
    ushort  operator[](int i) const { return v[i]; }

    bool operator==(const svec &v) const { return x==v.x && y==v.y && z==v.z; }
    bool operator!=(const svec &v) const { return x!=v.x || y!=v.y || z!=v.z; }

    svec &add(const svec &o) { x += o.x; y += o.y; z += o.z; return *this; }
    svec &add(const ivec &o) { x += o.x; y += o.y; z += o.z; return *this; }
    svec &add(int n) { x += n; y += n; z += n; return *this; }
    svec &sub(const svec &o) { x -= o.x; y -= o.y; z -= o.z; return *this; }
    svec &sub(const ivec &o) { x -= o.x; y -= o.y; z -= o.z; return *this; }
    svec &sub(int n) { x -= n; y -= n; z -= n; return *this; }
    svec &mul(int f) { x *= f; y *= f; z *= f; return *this; }
    svec &div(int f) { x /= f; y /= f; z /= f; return *this; }

    bool iszero() const { return x==0 && y==0 && z==0; }
};

struct bvec
{
    union
    {
        struct { uchar x, y, z; };
        uchar v[3];
    };

    bvec() {}
    bvec(uchar x, uchar y, uchar z) : x(x), y(y), z(z) {}
    bvec(const vec &v) : x((uchar)((v.x+1)*255/2)), y((uchar)((v.y+1)*255/2)), z((uchar)((v.z+1)*255/2)) {}

    uchar &operator[](int i)       { return v[i]; }
    uchar  operator[](int i) const { return v[i]; }

    bool operator==(const bvec &v) const { return x==v.x && y==v.y && z==v.z; }
    bool operator!=(const bvec &v) const { return x!=v.x || y!=v.y || z!=v.z; }

    bool iszero() const { return x==0 && y==0 && z==0; }

    vec tovec() const { return vec(x*(2.0f/255.0f)-1.0f, y*(2.0f/255.0f)-1.0f, z*(2.0f/255.0f)-1.0f); }
};

struct glmatrixf
{
    float v[16];

    glmatrixf() {}
    glmatrixf(const matrix3x4 &m)
    {
        v[0] = m.a.x; v[1] = m.b.x; v[2] = m.c.x;
        v[4] = m.a.y; v[5] = m.b.y; v[6] = m.c.y;
        v[8] = m.a.z; v[9] = m.b.z; v[10] = m.c.z;
        v[12] = m.a.w; v[13] = m.b.w; v[14] = m.c.w;
        v[3] = v[7] = v[11] = 0.0f; v[15] = 1.0f;
    }

    float operator[](int i) const { return v[i]; }
    float &operator[](int i) { return v[i]; }

    #define ROTVEC(A, B) \
    { \
        float a = A, b = B; \
        A = a*c + b*s; \
        B = b*c - a*s; \
    }

    void rotate_around_x(float angle)
    {
        float c = cosf(angle), s = sinf(angle);
        ROTVEC(v[4], v[8]);
        ROTVEC(v[5], v[9]);
        ROTVEC(v[6], v[10]);
    }

    void rotate_around_y(float angle)
    {
        float c = cosf(angle), s = sinf(angle);
        ROTVEC(v[8], v[0]);
        ROTVEC(v[9], v[1]);
        ROTVEC(v[10], v[2]);
    }

    void rotate_around_z(float angle)
    {
        float c = cosf(angle), s = sinf(angle);
        ROTVEC(v[0], v[4]);
        ROTVEC(v[1], v[5]);
        ROTVEC(v[2], v[6]);
    }

    #undef ROTVEC

    void rotate(float c, float s, const vec &d)
    {
        vec c1(d.x*d.x*(1-c)+c, d.y*d.x*(1-c)+d.z*s, d.x*d.z*(1-c)-d.y*s),
            c2(d.x*d.y*(1-c)-d.z*s, d.y*d.y*(1-c)+c, d.y*d.z*(1-c)+d.x*s),
            c3(d.x*d.z*(1-c)+d.y*s, d.y*d.z*(1-c)-d.x*s, d.z*d.z*(1-c)+c);

        vec r1(v[0], v[4], v[8]);
        v[0] = r1.dot(c1);
        v[4] = r1.dot(c2);
        v[8] = r1.dot(c3);

        vec r2(v[1], v[5], v[9]);
        v[1] = r2.dot(c1);
        v[5] = r2.dot(c2);
        v[9] = r2.dot(c3);

        vec r3(v[2], v[6], v[10]);
        v[2] = r3.dot(c1);
        v[6] = r3.dot(c2);
        v[10] = r3.dot(c3);
    }

    void rotate(float angle, const vec &d)
    {
        float c = cosf(angle), s = sinf(angle);
        rotate(c, s, d);
    }

    #define MULMAT(row, col) \
       v[col + row] = x[row]*y[col] + x[row + 4]*y[col + 1] + x[row + 8]*y[col + 2] + x[row + 12]*y[col + 3];

    template<class XT, class YT>
    void mul(const XT x[16], const YT y[16])
    {
        MULMAT(0, 0); MULMAT(1, 0); MULMAT(2, 0); MULMAT(3, 0);
        MULMAT(0, 4); MULMAT(1, 4); MULMAT(2, 4); MULMAT(3, 4);
        MULMAT(0, 8); MULMAT(1, 8); MULMAT(2, 8); MULMAT(3, 8);
        MULMAT(0, 12); MULMAT(1, 12); MULMAT(2, 12); MULMAT(3, 12);
    }

    #undef MULMAT

    void mul(const glmatrixf &x, const glmatrixf &y)
    {
        mul(x.v, y.v);
    }

    void identity()
    {
        static const float m[16] =
        {
            1, 0, 0, 0,
            0, 1, 0, 0,
            0, 0, 1, 0,
            0, 0, 0, 1
        };
        memcpy(v, m, sizeof(v));
    }

    void translate(float x, float y, float z)
    {
        v[12] += x;
        v[13] += y;
        v[14] += z;
    }

    void translate(const vec &o)
    {
        translate(o.x, o.y, o.z);
    }

    void scale(float x, float y, float z)
    {
        v[0] *= x; v[1] *= x; v[2] *= x; v[3] *= x;
        v[4] *= y; v[5] *= y; v[6] *= y; v[7] *= y;
        v[8] *= z; v[9] *= z; v[10] *= z; v[11] *= z;
    }

    void reflectz(float z)
    {
        v[8] = -v[8]; v[9] = -v[9]; v[10] = -v[10]; v[11] = -v[11];
        v[14] += 2*z;
    }

    void projective(float zscale = 0.5f, float zoffset = 0.5f)
    {
        loopi(2) loopj(4) v[i + j*4] = 0.5f*(v[i + j*4] + v[3 + j*4]); 
        loopj(4) v[2 + j*4] = zscale*v[2 + j*4] + zoffset*v[3 + j*4];
    }

    void transpose()
    {
        swap(v[1], v[4]); swap(v[2], v[8]); swap(v[3], v[12]);
        swap(v[6], v[9]); swap(v[7], v[13]);
        swap(v[11], v[14]);
    }

    void invertnormal(vec &dir) const
    {
        vec n(dir);
        dir.x = n.x*v[0] + n.y*v[1] + n.z*v[2];
        dir.y = n.x*v[4] + n.y*v[5] + n.z*v[6];
        dir.z = n.x*v[8] + n.y*v[9] + n.z*v[10];
    }

    void invertvertex(vec &pos) const
    {
        pos.x -= v[12];
        pos.y -= v[13];
        pos.z -= v[14];
        invertnormal(pos);
    }

    void invertplane(plane &p)
    {
        p.offset += p.x*v[12] + p.y*v[13] + p.z*v[14];
        invertnormal(p);
    }

    template<class T> float transformx(const T &p) const
    {
        return p.x*v[0] + p.y*v[4] + p.z*v[8] + v[12];
    }

    template<class T> float transformy(const T &p) const
    {
        return p.x*v[1] + p.y*v[5] + p.z*v[9] + v[13];
    }

    template<class T> float transformz(const T &p) const
    {
        return p.x*v[2] + p.y*v[6] + p.z*v[10] + v[14];
    }

    template<class T> float transformw(const T &p) const
    {
        return p.x*v[3] + p.y*v[7] + p.z*v[11] + v[15];
    }

    template<class T> void transform(const T &in, vec &out) const
    {
        out.x = transformx(in);
        out.y = transformy(in);
        out.z = transformz(in);
    }

    template<class T> void transform(const T &in, vec4 &out) const
    {
        out.x = transformx(in);
        out.y = transformy(in);
        out.z = transformz(in);
        out.w = transformw(in);
    }

    float getscale() const
    {
        return sqrtf(v[0]*v[0] + v[4]*v[4] + v[8]*v[8]);
    }

    vec gettranslation() const
    {
        return vec(v[12], v[13], v[14]);
    }

    float determinant() const;
    void adjoint(const glmatrixf &m);
    bool invert(const glmatrixf &m, float mindet = 1.0e-10f);
};

extern bool raysphereintersect(const vec &center, float radius, const vec &o, const vec &ray, float &dist);
extern bool rayrectintersect(const vec &b, const vec &s, const vec &o, const vec &ray, float &dist, int &orient);
extern bool linecylinderintersect(const vec &from, const vec &to, const vec &start, const vec &end, float radius, float &dist); 

