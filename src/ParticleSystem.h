//-----------------------------------------------------------------------------
//		         Name: ParticleSystem.cpp
//	  Original Author: Kevin Harris (http://www.codesampler.com/dx8src.htm)
//		  Description: Header file for the CParticleSystem Class
//					   Heavily modified from Kevin Harris's verion 
//					   to work as visualisation
//-----------------------------------------------------------------------------

#ifndef CPARTICLESYSTEM_H_INCLUDED
#define CPARTICLESYSTEM_H_INCLUDED

#include "types.h"

//-----------------------------------------------------------------------------
// SYMBOLIC CONSTANTS
//-----------------------------------------------------------------------------

// Classify Point
const int CP_FRONT   = 0;
const int CP_BACK    = 1;
const int CP_ONPLANE = 2;

// Collision Results
const int CR_BOUNCE  = 0;
const int CR_STICK   = 1;
const int CR_RECYCLE = 2;

//-----------------------------------------------------------------------------
// GLOBALS
//-----------------------------------------------------------------------------

typedef struct HsvColor
{
	public:
		HsvColor( );
		HsvColor( float h, float s, float v );

		float h, s, v;
} HsvColor;

struct Plane
{
    CVector m_vNormal;           // The plane's normal
    CVector m_vPoint;            // A coplanar point within the plane
    float       m_fBounceFactor;     // Coefficient of restitution (or how bouncy the plane is)
    int         m_nCollisionResult;  // What will particles do when they strike the plane

    Plane      *m_pNext;             // Next plane in list
};

struct Particle
{
    CVector m_vCurPos;    // Current position of particle
    CVector m_vCurVel;    // Current velocity of particle
    float       m_fInitTime;  // Time of creation of particle
	HsvColor	m_clrColor;	  // Color of particle

	CVector m_vGravity; 
    CVector m_vWind;    
	float		m_fVelocityVar;
	float		m_fSize;
	float		m_fLifeCycle;
	bool		m_bAirResistence;

    Particle   *m_pNext;      // Next particle in list
};

// Custom vertex and FVF declaration for point sprite vertex points
struct PointVertex
{
    CVector posit;
    CRGBA color;
};

//-----------------------------------------------------------------------------
// GLOBAL FUNCTIONS
//-----------------------------------------------------------------------------

// Helper function to stuff a FLOAT into a DWORD argument
inline int FtoDW( float f ) { return *((int*)&f); }

void convertHSV2RGB(float h,float s,float v,float *r,float *g,float *b);
CRGBA convertHSV2RGB(HsvColor hsvColor);

void convertRGB2HSV(float r, float g, float b, float *h, float *s, float *v);
HsvColor convertRGB2HSV(const CRGBA& d3dColor);

//-----------------------------------------------------------------------------
// CLASSES
//-----------------------------------------------------------------------------

class CParticleSystem
{

public:

    CParticleSystem(void);
   ~CParticleSystem(void);
    void dtor();
    void SetMaxParticles( int dwMaxParticles ) { m_dwMaxParticles = dwMaxParticles; }
	int GetMaxParticles( void ) { return m_dwMaxParticles; }

    void SetNumToRelease( int dwNumToRelease ) { m_dwNumToRelease = dwNumToRelease; }
	int GetNumToRelease( void ) { return m_dwNumToRelease; }

    void SetReleaseInterval( float fReleaseInterval ) { m_fReleaseInterval = fReleaseInterval; }
    float GetReleaseInterval( void ) { return m_fReleaseInterval; }

    void SetLifeCycle( float fLifeCycle ) { m_fLifeCycle = fLifeCycle; }
	float GetLifeCycle( void ) { return m_fLifeCycle; }

    void SetSize( float fSize ) { m_fSize = fSize; }
	float GetSize( void ) { return m_fSize; }
	float GetMaxPointSize( void ) { return m_fMaxPointSize; }

    void SetColor( HsvColor clrColor ) { m_clrColor = clrColor; }
	HsvColor GetColor( void ) { return m_clrColor; }

	void SetPosition( const CVector& vPosition ) { m_vPosition = vPosition; }
	const CVector& GetPosition( void ) { return m_vPosition; }

    void SetVelocity( const CVector& vVelocity ) { m_vVelocity = vVelocity; }
	const CVector& GetVelocity( void ) { return m_vVelocity; }

    void SetGravity( const CVector& vGravity ) { m_vGravity = vGravity; }
	const CVector& GetGravity( void ) { return m_vGravity; }

    void SetWind( const CVector& vWind ) { m_vWind = vWind; }
	const CVector& GetWind( void ) { return m_vWind; }

    void SetAirResistence( bool bAirResistence ) { m_bAirResistence = bAirResistence; }
	bool GetAirResistence( void ) { return m_bAirResistence; }

    void SetVelocityVar( float fVelocityVar ) { m_fVelocityVar = fVelocityVar; }
	float GetVelocityVar( void ) { return m_fVelocityVar; }

    void SetCollisionPlane( const CVector& vPlaneNormal, const CVector& vPoint, 
                            float fBounceFactor = 1.0f, int nCollisionResult = CR_BOUNCE );

	void SetHVar( float fHVar ) { m_fHVar = fHVar; }
	float GetHVar( void ) { return m_fHVar; }

	void SetMaxH( float fMaxH ) { m_fMaxH = fMaxH; }
	float GetMaxH( void ) { return m_fMaxH; }

	void SetMinH( float fMinH ) { m_fMinH = fMinH; }
	float GetMinH( void ) { return m_fMinH; }

	void SetSVar( float fSVar ) { m_fSVar = fSVar; }
	float GetSVar( void ) { return m_fSVar; }

	void SetMaxS( float fMaxS ) { m_fMaxS = fMaxS; }
	float GetMaxS( void ) { return m_fMaxS; }

	void SetMinS( float fMinS ) { m_fMinS = fMinS; }
	float GetMinS( void ) { return m_fMinS; }

	void SetVVar( float fVVar ) { m_fVVar = fVVar; }
	float GetVVar( void ) { return m_fVVar; }

	void SetMaxV( float fMaxV ) { m_fMaxV = fMaxV; }
	float GetMaxV( void ) { return m_fMaxV; }

	void SetMinV( float fMinV ) { m_fMinV = fMinV; }
	float GetMinV( void ) { return m_fMinV; }

	bool Init();
    bool Update( float fElapsedTime );
    bool Render();

    bool SetTexture( char *chTexFile);

    void RestartParticleSystem(void);

  void ctor();
private:

    int m_dwVBOffset;
    int m_dwFlush;
    int m_dwDiscard;
    Particle   *m_pActiveList;
    Particle   *m_pFreeList;
    Plane      *m_pPlanes;
	int m_dwActiveCount;
	float       m_fCurrentTime;
	float       m_fLastUpdate;

    float       m_fMaxPointSize;
    bool        m_bDeviceSupportsPSIZE;

    // Particle Attributes
    int m_dwMaxParticles;
    int m_dwNumToRelease;
    float       m_fReleaseInterval;
    float       m_fLifeCycle;
    float       m_fSize;
    HsvColor	m_clrColor;
    CVector m_vPosition;
    CVector m_vVelocity;
    CVector m_vGravity;
    CVector m_vWind;
    bool        m_bAirResistence;
    float       m_fVelocityVar;
    char       *m_chTexFile;

	float		m_fMinH;
	float		m_fMaxH;
	float		m_fHShiftRate;
	float		m_fHVar;

	float		m_fMinS;
	float		m_fMaxS;
	float		m_fSShiftRate;
	float		m_fSVar;

	float		m_fMinV;
	float		m_fMaxV;
	float		m_fVShiftRate;
	float		m_fVVar;
};

#endif /* CPARTICLESYSTEM_H_INCLUDED */
