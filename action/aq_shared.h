#ifndef __Q_SHARED_H
#define __Q_SHARED_H
#endif

#include <math.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
//FIREBLADE
#include <stddef.h>
//FIREBLADE

// legacy ABI support for Windows
#if defined(__GNUC__) && defined(WIN32) && ! defined(WIN64)
#define		q_gameabi           __attribute__((callee_pop_aggregate_return(0)))
#else
#define		q_gameabi
#endif

//==============================================
#if defined(__linux__) || defined(__FreeBSD__) || defined(__GNUC__)

# define HAVE_INLINE
# define HAVE_STRCASECMP
# define HAVE_SNPRINTF
# define HAVE_VSNPRINTF

#endif
//==============================================


#if ! defined(HAVE__CDECL) && ! defined(__cdecl)
# define __cdecl
#endif

#if ! defined(HAVE___FASTCALL) && ! defined(__fastcall)
# define __fastcall
#endif

#if ! defined(HAVE_INLINE) && ! defined(inline)
# ifdef HAVE___INLINE
#  define inline __inline
# else
#  define inline
# endif
#endif

#if defined(HAVE__SNPRINTF) && ! defined(snprintf)
# define snprintf _snprintf
#endif

#if defined(HAVE__VSNPRINTF) && ! defined(vsnprintf)
# define vsnprintf(dest, size, src, list) _vsnprintf((dest), (size), (src), (list)), (dest)[(size)-1] = 0
#endif

#ifdef HAVE__STRICMP
# ifndef Q_stricmp
#  define Q_stricmp _stricmp
# endif
# ifndef Q_strnicmp
#  define Q_strnicmp _strnicmp
# endif
# ifndef strcasecmp
#  define strcasecmp _stricmp
# endif
# ifndef strncasecmp
#  define strncasecmp _strnicmp
# endif
#elif defined(HAVE_STRCASECMP)
# ifndef Q_stricmp
#  define Q_stricmp strcasecmp
# endif
# ifndef Q_strnicmp
#  define Q_strnicmp strncasecmp
# endif
#endif


#ifndef M_PI
#define M_PI            3.14159265358979323846f	// matches value in gcc v2 math.h
#endif

#ifndef M_TWOPI
# define M_TWOPI		6.28318530717958647692f
#endif

#define M_PI_DIV_180	0.0174532925199432957692f
#define M_180_DIV_PI	57.295779513082320876798f

#define DEG2RAD( a ) (a * M_PI_DIV_180)
#define RAD2DEG( a ) (a * M_180_DIV_PI)


#ifndef max
# define max(a,b) ((a) > (b) ? (a) : (b))
#endif

#ifndef min
# define min(a,b) ((a) < (b) ? (a) : (b))
#endif

#define bound(a,b,c) ((a) >= (c) ? (a) : (b) < (a) ? (a) : (b) > (c) ? (c) : (b))

#define clamp(a,b,c) ((b) >= (c) ? (a)=(b) : (a) < (b) ? (a)=(b) : (a) > (c) ? (a)=(c) : (a))

struct cplane_s;

extern vec3_t vec3_origin;

#define nanmask (255<<23)

#define IS_NAN(x) (((*(int *)&x)&nanmask)==nanmask)

// microsoft's fabs seems to be ungodly slow...
//float Q_fabs (float f);
//#define       fabs(f) Q_fabs(f)
// next define line was changed by 3.20  -FB
#if !defined C_ONLY && !defined __linux__ && !defined __sgi
extern long Q_ftol (float f);
#else
#define Q_ftol( f ) ( long ) (f)
#endif

#define DotProduct(x,y)			((x)[0]*(y)[0]+(x)[1]*(y)[1]+(x)[2]*(y)[2])
#define CrossProduct(v1,v2,c)	((c)[0]=(v1)[1]*(v2)[2]-(v1)[2]*(v2)[1],(c)[1]=(v1)[2]*(v2)[0]-(v1)[0]*(v2)[2],(c)[2]=(v1)[0]*(v2)[1]-(v1)[1]*(v2)[0])

#define PlaneDiff(point,plane) (((plane)->type < 3 ? (point)[(plane)->type] : DotProduct((point), (plane)->normal)) - (plane)->dist)

#define VectorSubtract(a,b,c)   ((c)[0]=(a)[0]-(b)[0],(c)[1]=(a)[1]-(b)[1],(c)[2]=(a)[2]-(b)[2])
#define VectorAdd(a,b,c)		((c)[0]=(a)[0]+(b)[0],(c)[1]=(a)[1]+(b)[1],(c)[2]=(a)[2]+(b)[2])
#define VectorCopy(a,b)			((b)[0]=(a)[0],(b)[1]=(a)[1],(b)[2]=(a)[2])
#define VectorClear(a)			((a)[0]=(a)[1]=(a)[2]=0)
#define VectorNegate(a,b)		((b)[0]=-(a)[0],(b)[1]=-(a)[1],(b)[2]=-(a)[2])
#define VectorSet(v, x, y, z)	((v)[0]=(x),(v)[1]=(y),(v)[2]=(z))
#define VectorAvg(a,b,c)		((c)[0]=((a)[0]+(b)[0])*0.5f,(c)[1]=((a)[1]+(b)[1])*0.5f, (c)[2]=((a)[2]+(b)[2])*0.5f)
#define VectorMA(a,b,c,d)		((d)[0]=(a)[0]+(b)*(c)[0],(d)[1]=(a)[1]+(b)*(c)[1],(d)[2]=(a)[2]+(b)*(c)[2])
#define VectorCompare(v1,v2)	((v1)[0]==(v2)[0] && (v1)[1]==(v2)[1] && (v1)[2]==(v2)[2])
#define VectorLength(v)			(sqrtf(DotProduct((v),(v))))
#define VectorInverse(v)		((v)[0]=-(v)[0],(v)[1]=-(v)[1],(v)[2]=-(v)[2])
#define VectorScale(in,s,out)	((out)[0]=(in)[0]*(s),(out)[1]=(in)[1]*(s),(out)[2]=(in)[2]*(s))
#define LerpVector(a,b,c,d) \
    ((d)[0]=(a)[0]+(c)*((b)[0]-(a)[0]), \
     (d)[1]=(a)[1]+(c)*((b)[1]-(a)[1]), \
     (d)[2]=(a)[2]+(c)*((b)[2]-(a)[2]))

#define VectorCopyMins(a,b,c)	((c)[0]=min((a)[0],(b)[0]),(c)[0]=min((a)[1],(b)[1]),(c)[2]=min((a)[2],(b)[2]))
#define VectorCopyMaxs(a,b,c)	((c)[0]=max((a)[0],(b)[0]),(c)[0]=max((a)[1],(b)[1]),(c)[2]=max((a)[2],(b)[2]))

#define DistanceSquared(v1,v2)	(((v1)[0]-(v2)[0])*((v1)[0]-(v2)[0])+((v1)[1]-(v2)[1])*((v1)[1]-(v2)[1])+((v1)[2]-(v2)[2])*((v1)[2]-(v2)[2]))
#define Distance(v1,v2)			(sqrtf(DistanceSquared(v1,v2)))

#define ClearBounds(mins,maxs)	((mins)[0]=(mins)[1]=(mins)[2]=99999,(maxs)[0]=(maxs)[1]=(maxs)[2]=-99999)


void AddPointToBounds (const vec3_t v, vec3_t mins, vec3_t maxs);
vec_t VectorNormalize (vec3_t v);	// returns vector length
vec_t VectorNormalize2 (const vec3_t v, vec3_t out);

int Q_log2 (int val);

void R_ConcatRotations (float in1[3][3], float in2[3][3], float out[3][3]);
void R_ConcatTransforms (float in1[3][4], float in2[3][4], float out[3][4]);

void AngleVectors (const vec3_t angles, vec3_t forward, vec3_t right, vec3_t up);
int BoxOnPlaneSide (const vec3_t emins, const vec3_t emaxs, const struct cplane_s *plane);
float anglemod (float a);
float LerpAngle (float a1, float a2, float frac);


#define BOX_ON_PLANE_SIDE(emins, emaxs, p)        \
  (((p)->type < 3)?                               \
    (                                             \
      ((p)->dist <= (emins)[(p)->type])?          \
        1                                         \
        :                                         \
          (                                       \
            ((p)->dist >= (emaxs)[(p)->type])?    \
              2                                   \
              :                                   \
              3                                   \
            )                                     \
          )                                       \
        :                                         \
          BoxOnPlaneSide( (emins), (emaxs), (p)))

void ProjectPointOnPlane (vec3_t dst, const vec3_t p, const vec3_t normal);
void PerpendicularVector (vec3_t dst, const vec3_t src);
void RotatePointAroundVector (vec3_t dst, const vec3_t dir,
			      const vec3_t point, float degrees);

void VectorRotate( vec3_t in, vec3_t angles, vec3_t out );  // a_doorkick.c
void VectorRotate2( vec3_t v, float degrees );

//=============================================

char *COM_SkipPath (char *pathname);
void COM_StripExtension (char *in, char *out);
void COM_FileBase (char *in, char *out);
void COM_FilePath (char *in, char *out);
void COM_DefaultExtension (char *path, char *extension);

char *COM_Parse (char **data_p);
// data is an in/out parm, returns a parsed out token

void Com_PageInMemory (byte * buffer, int size);

//=============================================
#define Q_isupper( c )	( (c) >= 'A' && (c) <= 'Z' )
#define Q_islower( c )	( (c) >= 'a' && (c) <= 'z' )
#define Q_isdigit( c )	( (c) >= '0' && (c) <= '9' )
#define Q_isalpha( c )	( Q_isupper( c ) || Q_islower( c ) )
#define Q_isalnum( c )	( Q_isalpha( c ) || Q_isdigit( c ) )

int Q_tolower( int c );
int Q_toupper( int c );

char *Q_strlwr( char *s );
char *Q_strupr( char *s );

#ifndef Q_strnicmp
 int Q_strnicmp (const char *s1, const char *s2, size_t size);
#endif
#ifndef Q_stricmp
#  define Q_stricmp(s1, s2) Q_strnicmp((s1), (s2), 99999)
#endif
 char *Q_stristr( const char *str1, const char *str2 );

// buffer safe operations
void Q_strncpyz (char *dest, const char *src, size_t size );
void Q_strncatz (char *dest, const char *src, size_t size );
#ifdef HAVE_SNPRINTF
# define Com_sprintf snprintf
#else
 void Com_sprintf(char *dest, size_t size, const char *fmt, ...);
#endif

//=============================================

void Swap_Init (void);