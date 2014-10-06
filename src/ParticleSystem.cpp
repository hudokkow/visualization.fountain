//-----------------------------------------------------------------------------
//		         Name: ParticleSystem.cpp
//	  Original Author: Kevin Harris (http://www.codesampler.com/dx8src.htm)
//		  Description: Implementation file for the CParticleSystem Class
//					   Heavily modified from Kevin Harris's verion 
//					   (particularly in Render) to work as visualisation
//-----------------------------------------------------------------------------

#include "ParticleSystem.h"
#include "Util.h"

const int HBAND = 128;
const int SBAND = 256;
const int VBAND = 384;

HsvColor::HsvColor() {}

HsvColor::HsvColor( float hin, float sin, float vin )
{
	h = hin;
	s = sin;
	v = vin;
}

//-----------------------------------------------------------------------------
// Name: getRandomVector()
// Desc: Generates a random vector where X,Y, and Z components are between
//       -1.0 and 1.0
//-----------------------------------------------------------------------------
CVector getRandomVector()
{
  CVector vVector;
    // Pick a random Z between -1.0f and 1.0f.
    vVector.z = getRandomMinMax( -1.0f, 1.0f);
    
    // Get radius of this circle
    float radius = (float)sqrt(1 - vVector.z * vVector.z);
    
    // Pick a random point on a circle.
    float t = getRandomMinMax( -M_PI, M_PI);

    // Compute matching X and Y for our Z.
    vVector.x = (float)cosf(t) * radius;
    vVector.y = (float)sinf(t) * radius;

    return vVector;
}

//-----------------------------------------------------------------------------
// Name : classifyPoint()
// Desc : Classifies a point against the plane passed
//-----------------------------------------------------------------------------
int classifyPoint( CVector *vPoint, Plane *pPlane )
{
	CVector vDirection = pPlane->m_vPoint - *vPoint;
	float fResult = DotProduct(vDirection, pPlane->m_vNormal );

	if( fResult < -0.001f )
        return CP_FRONT;

	if( fResult >  0.001f )
        return CP_BACK;

	return CP_ONPLANE;
}

//-----------------------------------------------------------------------------
// Name: CParticleSystem()
// Desc:
//-----------------------------------------------------------------------------
CParticleSystem::CParticleSystem()
{
}
void CParticleSystem::ctor()
{
    m_dwVBOffset       = 0;    // Gives the offset of the vertex buffer chunk that's currently being filled
    m_dwFlush          = 512;  // Number of point sprites to load before sending them to hardware(512 = 2048 divided into 4 chunks)
    m_dwDiscard        = 2048; // Max number of point sprites the vertex buffer can load until we are forced to discard and start over
    m_pActiveList      = NULL; // Head node of point sprites that are active
    m_pFreeList        = NULL; // Head node of point sprites that are inactive and waiting to be recycled.
    m_pPlanes          = NULL;
	m_dwActiveCount    = 0;
	m_fCurrentTime     = 0.0f;
	m_fLastUpdate      = 0.0f;
    m_chTexFile        = NULL;
    m_texture     = 0;
    m_dwMaxParticles   = 1;
    m_dwNumToRelease   = 1;
    m_fReleaseInterval = 1.0f;
    m_fLifeCycle       = 1.0f;
    m_fSize            = 1.0f;
    m_clrColor         = HsvColor(0.0f,1.0f,0.6f);
    m_vPosition        = CVector(0.0f,0.0f,0.0f);
    m_vVelocity        = CVector(0.0f,0.0f,0.0f);
    m_vGravity         = CVector(0.0f,0.0f,0.0f);
    m_vWind            = CVector(0.0f,0.0f,0.0f);
    m_bAirResistence   = true;
    m_fVelocityVar     = 1.0f;

	m_fMaxH				= 360.0f;
	m_fMinH				= 0.0f;
	m_fHVar				= 45.0f;

	m_fMaxS				= 1.0f;
	m_fMinS				= 1.0f;
	m_fSVar				= 0.0f;
	
	m_fMaxV				= 0.6f;
	m_fMinV				= 0.2f;
	m_fVVar				= 0.3f;
}

//-----------------------------------------------------------------------------
// Name: ~CParticleSystem()
// Desc:
//-----------------------------------------------------------------------------
CParticleSystem::~CParticleSystem()
{
}

void CParticleSystem::dtor()
{
    while( m_pPlanes ) // Repeat till null...
    {
        Plane *pPlane = m_pPlanes;   // Hold onto the first one
        m_pPlanes = pPlane->m_pNext; // Move down to the next one
        free(pPlane);               // Delete the one we're holding
    }

    while( m_pActiveList )
    {
        Particle* pParticle = m_pActiveList;
        m_pActiveList = pParticle->m_pNext;
        free(pParticle);
    }
    m_pActiveList = NULL;

    while( m_pFreeList )
    {
        Particle *pParticle = m_pFreeList;
        m_pFreeList = pParticle->m_pNext;
        free(pParticle);
    }
    m_pFreeList = NULL;

	if( m_chTexFile != NULL )
	{
		free(m_chTexFile);
		m_chTexFile = NULL;
	}

    if( m_texture != NULL )
      glDeleteTextures(1, &m_texture);
}

//-----------------------------------------------------------------------------
// Name: SetTexture()
// Desc: 
//-----------------------------------------------------------------------------
bool CParticleSystem::SetTexture( char *chTexFile)
{
    // Deallocate the memory that was previously reserved for this string.
  if( m_chTexFile != NULL )
  {
    free(m_chTexFile);
    m_chTexFile = NULL;
  }
    
  // Dynamically allocate the correct amount of memory.
  m_chTexFile = (char*)malloc( strlen( chTexFile ) + 1);

  // If the allocation succeeds, copy the initialization string.
  if( m_chTexFile != NULL )
    strcpy( m_chTexFile, chTexFile );

  if( m_ptexParticle != NULL )
    m_ptexParticle->Release();
  m_ptexParticle = NULL;

  m_texture = SOIL_load_OGL_texture(m_chTexFile, SOIL_LOAD_RGBA, 0, 0);

  return m_texture != 0;
}

//-----------------------------------------------------------------------------
// Name: SetCollisionPlane()
// Desc: 
//-----------------------------------------------------------------------------
void CParticleSystem::SetCollisionPlane( CVector vPlaneNormal, CVector vPoint, 
                                         float fBounceFactor, int nCollisionResult )
{
    Plane *pPlane = (Plane*)malloc(sizeof (Plane));
    memset(pPlane,0,sizeof(Plane));

    pPlane->m_vNormal          = vPlaneNormal;
    pPlane->m_vPoint           = vPoint;
    pPlane->m_fBounceFactor    = fBounceFactor;
    pPlane->m_nCollisionResult = nCollisionResult;

    pPlane->m_pNext = m_pPlanes; // Attach the curent list to it...
    m_pPlanes = pPlane;          // ... and make it the new head.
}

//-----------------------------------------------------------------------------
// Name: Init()
// Desc: 
//-----------------------------------------------------------------------------
bool CParticleSystem::Init()
{
    // Get max point size
    m_fMaxPointSize = 63; // TODO?

    m_bDeviceSupportsPSIZE = false;

    return true;
}

//-----------------------------------------------------------------------------
// Name: Update()
// Desc:
//-----------------------------------------------------------------------------
bool CParticleSystem::Update( float fElpasedTime )
{
  Particle  *pParticle;
  Particle **ppParticle;
  Plane     *pPlane;
  Plane    **ppPlane;
  CVector vOldPosition;

  m_fCurrentTime += fElpasedTime;     // Update our particle system timer...

  ppParticle = &m_pActiveList; // Start at the head of the active list

  while( *ppParticle )
  {
    pParticle = *ppParticle; // Set a pointer to the head

    // Calculate new position
    float fTimePassed  = m_fCurrentTime - pParticle->m_fInitTime;

    if( fTimePassed >= pParticle->m_fLifeCycle )
    {
      // Time is up, put the particle back on the free list...
      *ppParticle = pParticle->m_pNext;
      pParticle->m_pNext = m_pFreeList;
      m_pFreeList = pParticle;

      --m_dwActiveCount;
    }
    else
    {
      // Update particle position and velocity

      // Update velocity with respect to Gravity (Constant Accelaration)
      pParticle->m_vCurVel += pParticle->m_vGravity * fElpasedTime;

      // Update velocity with respect to Wind (Accelaration based on 
      // difference of vectors)
      if( pParticle->m_bAirResistence == true )
        pParticle->m_vCurVel += (pParticle->m_vWind - pParticle->m_vCurVel) * fElpasedTime;

      // Finally, update position with respect to velocity
      vOldPosition = pParticle->m_vCurPos;
      pParticle->m_vCurPos += pParticle->m_vCurVel * fElpasedTime;

      //-----------------------------------------------------------------
      // BEGIN Checking the particle against each plane that was set up

      ppPlane = &m_pPlanes; // Set a pointer to the head

      while( *ppPlane )
      {
        pPlane = *ppPlane;
        int result = classifyPoint( &pParticle->m_vCurPos, pPlane );

        if( result == CP_BACK /*|| result == CP_ONPLANE */ )
        {
          if( pPlane->m_nCollisionResult == CR_BOUNCE )
          {
            pParticle->m_vCurPos = vOldPosition;

            //-----------------------------------------------------------------
            //
            // The new velocity vector of a particle that is bouncing off
            // a plane is computed as follows:
            //
            // Vn = (N.V) * N
            // Vt = V - Vn
            // Vp = Vt - Kr * Vn
            //
            // Where:
            // 
            // .  = Dot product operation
            // N  = The normal of the plane from which we bounced
            // V  = Velocity vector prior to bounce
            // Vn = Normal force
            // Kr = The coefficient of restitution ( Ex. 1 = Full Bounce, 
            //      0 = Particle Sticks )
            // Vp = New velocity vector after bounce
            //
            //-----------------------------------------------------------------

            float Kr = pPlane->m_fBounceFactor;

            CVector Vn = DotProduct( pPlane->m_vNormal, 
                pParticle->m_vCurVel ) * 
              pPlane->m_vNormal;
            CVector Vt = pParticle->m_vCurVel - Vn;
            CVector Vp = Vt - Kr * Vn;

            pParticle->m_vCurVel = Vp;
          }
          else if( pPlane->m_nCollisionResult == CR_RECYCLE )
          {
            pParticle->m_fInitTime -= pParticle->m_fLifeCycle;
          }

          else if( pPlane->m_nCollisionResult == CR_STICK )
          {
            pParticle->m_vCurPos = vOldPosition;
            pParticle->m_vCurVel = CVector(0.0f,0.0f,0.0f);
          }
        }

        ppPlane = &pPlane->m_pNext;
      }

      // END Plane Checking
      //-----------------------------------------------------------------

      ppParticle = &pParticle->m_pNext;
    }
  }

  //-------------------------------------------------------------------------
  // Emit new particles in accordance to the flow rate...
  // 
  // NOTE: The system operates with a finite number of particles.
  //       New particles will be created until the max amount has
  //       been reached, after that, only old particles that have
  //       been recycled can be reintialized and used again.
  //-------------------------------------------------------------------------

  if( m_fCurrentTime - m_fLastUpdate > m_fReleaseInterval )
  {
    // Reset update timing...
    m_fLastUpdate = m_fCurrentTime;

    // Emit new particles at specified flow rate...
    for( int i = 0; i < m_dwNumToRelease; ++i )
    {
      // Do we have any free particles to put back to work?
      if( m_pFreeList )
      {
        // If so, hand over the first free one to be reused.
        pParticle = m_pFreeList;
        // Then make the next free particle in the list next to go!
        m_pFreeList = pParticle->m_pNext;
      }
      else
      {
        // There are no free particles to recycle...
        // We'll have to create a new one from scratch!
        if( m_dwActiveCount < m_dwMaxParticles )
        {
          if( NULL == ( pParticle = (Particle*)malloc( sizeof(Particle)) ) )
          {
            return false;
          }
          memset(pParticle ,0,sizeof(Particle) );
        }
      }

      if( m_dwActiveCount < m_dwMaxParticles )
      {
        pParticle->m_pNext = m_pActiveList; // Make it the new head...
        m_pActiveList = pParticle;

        // Set the attributes for our new particle...
        pParticle->m_vCurVel = m_vVelocity;

        if( m_fVelocityVar != 0.0f )
        {
          CVector vRandomVec = getRandomVector();
          pParticle->m_vCurVel += vRandomVec * m_fVelocityVar;
        }

        pParticle->m_fInitTime  = m_fCurrentTime;
        pParticle->m_vCurPos    = m_vPosition;

        //modifiy h by m_fHMod
        float h = m_clrColor.h;
        if (m_fHVar > 0)
        {
          h = getRandomMinMax(-1.0f, 1.0f) * m_fHVar;
          h+=m_clrColor.h;

          while (h > 360.0f)	h -= 360.0f;
          while (h < 0)		h += 360.0f;

          h = std::max(m_fMinH, min(m_fMaxH, h));
        }

        //modifiy s by m_fSMod
        float s = m_clrColor.s;
        if (m_fSVar > 0)
        {
          s = getRandomMinMax(-1.0f, 1.0f) * m_fSVar;
          s+=m_clrColor.s;

          while (s > 1.0f) s-= 1.0f;
          while (s < 0.0f) s+= 1.0f;

          s = std::max(m_fMinS, min(m_fMaxS, s));
        }

        //modifiy v by m_fVMod
        float v = m_clrColor.v;
        if (m_fVVar > 0)
        {
          v = getRandomMinMax(-1.0f, 1.0f) * m_fVVar;
          v+=m_clrColor.v;

          while (v > 1.0f) v-= 1.0f;
          while (v < 1.0f) v+= 1.0f;

          v = std::max(m_fMinV, min(m_fMaxV, v));
        }

        pParticle->m_clrColor	= HsvColor(h, s, v);

        pParticle->m_vGravity	= m_vGravity;
        pParticle->m_vWind		= m_vWind;
        pParticle->m_fVelocityVar = m_fVelocityVar;
        pParticle->m_fSize		= m_fSize;
        pParticle->m_fLifeCycle	= m_fLifeCycle;
        pParticle->m_bAirResistence = m_bAirResistence;

        ++m_dwActiveCount;
      }
    }
  }

  return true;
}

//-----------------------------------------------------------------------------
// Name: RestartParticleSystem()
// Desc:
//-----------------------------------------------------------------------------
void CParticleSystem::RestartParticleSystem( void )
{
  Particle  *pParticle;
  Particle **ppParticle;

  ppParticle = &m_pActiveList; // Start at the head of the active list

  while( *ppParticle )
  {
    pParticle = *ppParticle; // Set a pointer to the head

    // Put the particle back on the free list...
    *ppParticle = pParticle->m_pNext;
    pParticle->m_pNext = m_pFreeList;
    m_pFreeList = pParticle;

    --m_dwActiveCount;
  }
}

//-----------------------------------------------------------------------------
// Name: Render()
// Desc: Renders the particle system
// Note: I couldn't get textures to display on the point sprites used by
//		 the original Render method, so I have heavily rewritten it to not
//		 user point sprites
//-----------------------------------------------------------------------------
bool CParticleSystem::Render()
{
    CUSTOMVERTEX* pVertices;

    struct CUSTOMVERTEX
    {
      float x, y, z; // The transformed position for the vertex.
      float tu, tv;  // The vertex texture coordinates
    };

    //Store each point of the triangle together with it's colour
    CUSTOMVERTEX cvVertices[] =
    {
      { -1.0f, -1.0f, 0.0f    ,0.0f, 1.0f }, // x, y, z, textures (tu, tv) 
      { -1.0f,  1.0f, 0.0f    ,0.0f, 0.0f }, 
      {  1.0f,  1.0f, 0.0f    ,1.0f, 0.0f },

      { -1.0f, -1.0f, 0.0f    ,0.0f, 1.0f },
      {  1.0f,  1.0f, 0.0f    ,1.0f, 0.0f },
      {  1.0f, -1.0f, 0.0f    ,1.0f, 1.0f }

    };

    const GLfloat dif[] = {1.0, 1.0, 1.0, 1.0};
    glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, dif);
    glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, dif);
    glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, dif);
    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, dif);

    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, m_texture);

    Particle    *pParticle = m_pActiveList;
    while (pParticle)
    {
      CRGBA col = convertHSV2RGB(pParticle->m_clrColor);
      glMaterialfv(GL_EMISSION, (const GLfloat*)col.col);
      glLoadIdentity();
      glTranslate(pParticle->m_vCurPos.x, pParticlar->m_vCurPos.y, pParticle->m_vCurPos.z);
      glScalef(m_fSize, m_fSize, m_fSize);
      glBegin(GL_TRIANGLES);
      for (size_t i=0;i<6;++i)
      {
        glTexCoord2f(cvVertices[i].tu, cvVertices[i].tv);
        glVertex3f(cvVertices[i].x, cvVertices[i].y, cvVertices[i].z);
      }
      glEnd();
      pParticle = pParticle->m_pNext;
    }

    return true;
}

//-----------------------------------------------------------------------------
// Name: convertHSV2RGB()
// Desc: converts an hsv color to rgb. all values should be specified in the range 0.0f - 1.0f
//-----------------------------------------------------------------------------

CRGBA convertHSV2RGB(HsvColor hsvColor)
{
	float r, g, b;
	convertHSV2RGB(hsvColor.h, hsvColor.s, hsvColor.v, &r, &g, &b);
	return CRGBA(r, g, b, 1.0f);
}

void convertHSV2RGB(float h,float s,float v,float *r,float *g,float *b)
{
	//since we are using vals in the range of 0.0 - 1.0
	//we need to mutiply h by 360 as that is what the rest
	//of the function expects
	//h*= 360.0f;

	// conversion from Foley et.al., fig. 13.34, p. 593
	float f, p, q, t;
	int k;
	if (s == 0.0)
	{ // achromatic case
		*r = *g = *b = v;
	}
	else
	{ // chromatic case
		if (h == 360.0) h=0.0f;
		h = h/60.0f;
		k = (int)h;
		f = h - (float)k;
		p = v * (1.0f - s);
		q = v * (1.0f - (s * f));
		t = v * (1.0f - (s * (1.0f - f)));
		switch (k)
		{
			case 0: *r = v; *g = t; *b = p; break;
			case 1: *r = q; *g = v; *b = p; break;
			case 2: *r = p; *g = v; *b = t; break;
			case 3: *r = p; *g = q; *b = v; break;
			case 4: *r = t; *g = p; *b = v; break;
			case 5: *r = v; *g = p; *b = q; break;
		}
	}
}

// Convert RGB to HSV
HsvColor convertRGB2HSV(const CRGBA& d3dColor)
{
  float h, s, v;
  convertRGB2HSV(d3dColor.r, d3dColor.g, d3dColor.b, &h, &s, &v);
  HsvColor result;
  result.h = h;
  result.s = s;
  result.v = v;
  return result;
}

void convertRGB2HSV(float r, float g, float b, float *h, float *s, float *v)
{
  float min, max, delta;
  min = min( r, g, b );
  max = max( r, g, b );
  *v = max;				// v
  delta = max - min;
  if( max != 0 )
    *s = delta / max;		// s
  else {
    // r = g = b = 0		// s = 0, v is undefined
    *s = 0;
    *h = -1;
    return;
  }
  if( r == max )
    *h = ( g - b ) / delta;		// between yellow & magenta
  else if( g == max )
    *h = 2 + ( b - r ) / delta;	// between cyan & yellow
  else
    *h = 4 + ( r - g ) / delta;	// between magenta & cyan
  *h *= 60;				// degrees
  if( *h < 0 )
    *h += 360;
}
