// Fountain.cpp
//
//////////////////////////////////////////////////////////////////////

#include "Util.h"
#include "Fountain.h"
#include <math.h>
#include <stdio.h>
#include <xbmc/xbmc_vis_dll.h>
#include <xbmc/libXBMC_addon.h>
#include <memory.h>
#include <time.h>
#include <algorithm>
#include <GL/gl.h>
#include <GL/glu.h>

#define	FREQ_DATA_SIZE 1024			// size of frequency data wanted
#define MAX_BARS 720				// number of bars in the Spectrum
#define MIN_PEAK_DECAY_SPEED 0		// decay speed in dB/frame
#define MAX_PEAK_DECAY_SPEED 4
#define MIN_RISE_SPEED 0.01f		// fraction of actual rise to allow
#define MAX_RISE_SPEED 1
#define MIN_FALL_SPEED 0.01f		// fraction of actual fall to allow
#define MAX_FALL_SPEED 1
#define MIN_FREQUENCY 1				// allowable frequency range
#define MAX_FREQUENCY 24000
#define MIN_LEVEL 0					// allowable level range
#define MAX_LEVEL 96
#define TEXTURE_HEIGHT 256
#define TEXTURE_MID 128
#define TEXTURE_WIDTH 1
#define MAX_CHANNELS 2
#define MAX_SETTINGS 64

static CParticleSystem m_ParticleSystem;

static HsvColor m_clrColor;
static int m_iHDir = 1;
static int m_iSDir = 1;
static int m_iVDir = 1;

static float m_fElapsedTime;
static int m_dwCurTime;
static int m_dwLastTime;

static float m_fUpdateSpeed			= 1000.0f;
static float m_fRotation			= 0.0f;

static ParticleSystemSettings m_pssSettings[MAX_SETTINGS];
static int m_iCurrSetting = 0;
static int m_iNumSettings = 2;
static bool m_bCycleSettings = true;

static int		m_iSampleRate;

static float	m_pFreq[FREQ_DATA_SIZE];
static float	m_pFreqPrev[FREQ_DATA_SIZE];				

static int		m_iBars		= 12;
static bool		m_bLogScale = false;
static float	m_fMinFreq	= 200;
static float	m_fMaxFreq	= MAX_FREQUENCY;
static float	m_fMinLevel = MIN_LEVEL;
static float	m_fMaxLevel = MAX_LEVEL;

ADDON::CHelper_libXBMC_addon *XBMC           = NULL;

void SetDefaults();
void SetDefaults(ParticleSystemSettings* settings);
void SetDefaults(EffectSettings* settings);
void ShiftColor(ParticleSystemSettings* settings);
void CreateArrays();

CVector Shift(EffectSettings* settings);

inline int RandPosNeg() {
	return (float)rand() > RAND_MAX/2.0f ? 1 : -1;
}

ADDON_STATUS ADDON_Create(void* hdl, void* props)
{
  if (!props)
    return ADDON_STATUS_UNKNOWN;

  if (!XBMC)
    XBMC = new ADDON::CHelper_libXBMC_addon;

  if (!XBMC->RegisterMe(hdl))
  {
    delete XBMC, XBMC=NULL;
    return ADDON_STATUS_PERMANENT_FAILURE;
  }

  m_clrColor = HsvColor( 360.0f, 1.0f, .06f );
  m_iCurrSetting = -1;
  srand(time(NULL));

  m_ParticleSystem.ctor();
  SetDefaults();
  m_ParticleSystem.Init();

  return ADDON_STATUS_OK;
}

extern "C" void Start(int iChannels, int iSamplesPerSec, int iBitsPerSample, const char* szSongName)
{
  m_iSampleRate = iSamplesPerSec;

  //set (or reset) our previous frequency data array
  for (int i=0; i<FREQ_DATA_SIZE; i++)
  {
    m_pFreqPrev[i] = 0.0f;
  }



  if (m_bCycleSettings || m_iNumSettings < 3)
    m_iCurrSetting++;
  else
  {
    int iNextSetting = m_iCurrSetting;
    //TODO: try and fix this to be more random
    while (iNextSetting == m_iCurrSetting)
      iNextSetting = ((float)rand () / RAND_MAX) * m_iNumSettings;
    m_iCurrSetting = iNextSetting;
  }
  if (m_iCurrSetting >= m_iNumSettings || m_iCurrSetting < 0)
    m_iCurrSetting = 0;

  m_clrColor = m_pssSettings[m_iCurrSetting].m_hsvColor;
  m_iHDir = 1;
  m_iSDir = 1;
  m_iVDir = 1;
  InitParticleSystem(m_pssSettings[m_iCurrSetting]);
}


extern "C" void AudioData(const float* pAudioData, int iAudioDataLength, float *pFreqData, int iFreqDataLength)
{
  ParticleSystemSettings *currSettings = &m_pssSettings[m_iCurrSetting];

  if (iFreqDataLength>FREQ_DATA_SIZE)
    iFreqDataLength = FREQ_DATA_SIZE;

  // Group data into frequency bins by averaging (Ignore the constant term)
  int jmin=2;
  int jmax;
  // FIXME:  Roll conditionals out of loop
  for (int i=0, iBin=0; i < m_iBars; i++, iBin+=2)
  {
    m_pFreq[iBin]=0.000001f;	// almost zero to avoid taking log of zero later
    if (m_bLogScale)
      jmax = (int) (m_fMinFreq*pow(m_fMaxFreq/m_fMinFreq,(float)i/m_iBars)/m_iSampleRate*iFreqDataLength + 0.5f);
    else
      jmax = (int) ((m_fMinFreq + (m_fMaxFreq-m_fMinFreq)*i/m_iBars)/m_iSampleRate*iFreqDataLength + 0.5f);
    // Round up to nearest multiple of 2 and check that jmin is not jmax
    jmax<<=1;
    if (jmax > iFreqDataLength) jmax = iFreqDataLength;
    if (jmax==jmin) jmin-=2;
    for (int j=jmin; j<jmax; j+=2)
    {
      m_pFreq[iBin]+=pFreqData[j]+pFreqData[j+1];
    }
    m_pFreq[iBin] /=(jmax-jmin);
    jmin = jmax;
  }


  // Transform data to dB scale, 0 (Quietest possible) to 96 (Loudest)
  int i;
  for (i=0; i < (m_iBars*2); i++)
  {
    m_pFreq[i] = 10*log10(m_pFreq[i]);
    if (m_pFreq[i] > MAX_LEVEL)
      m_pFreq[i] = MAX_LEVEL;
    if (m_pFreq[i] < MIN_LEVEL)
      m_pFreq[i] = MIN_LEVEL;
  }

  // truncate data to the users range
  if (m_pFreq[i] > m_fMaxLevel)
    m_pFreq[i] = m_fMaxLevel;
  if (m_pFreq[i] < m_fMinLevel)
    m_pFreq[i] = m_fMinLevel;

  //if we exceed the rotation sensitivity threshold, reverse our rotation
  int rotationBar = std::min(m_iBars, currSettings->m_iRotationBar);
  if (abs(m_pFreq[rotationBar] - m_pFreqPrev[rotationBar]) > MAX_LEVEL * currSettings->m_fRotationSensitivity)
    currSettings->m_fRotationSpeed*=-1;

  ShiftColor(currSettings);

  //adjust num to release
  if (currSettings->m_fNumToReleaseMod != 0.0f)
  {
    int numToReleaseBar = std::min(m_iBars, 10);
    float level = (m_pFreq[numToReleaseBar]/(float)MAX_LEVEL);
    int numToRelease = level * currSettings->m_dwNumToRelease;
    int mod = numToRelease * currSettings->m_fNumToReleaseMod;
    numToRelease+=mod;
    m_ParticleSystem.SetNumToRelease(numToRelease);
  }

  //adjust gravity
  CVector vGravity = Shift(&currSettings->m_esGravity);
  m_ParticleSystem.SetGravity(vGravity);

  //adjust wind
  CVector vWind = Shift(&currSettings->m_esWind);
  m_ParticleSystem.SetWind(vWind);

  //adjust velocity
  CVector vVelocity = Shift(&currSettings->m_esVelocity);
  m_ParticleSystem.SetVelocity(vVelocity);

  //adjust position
  CVector vPosition = Shift(&currSettings->m_esPosition);
  m_ParticleSystem.SetPosition(vPosition);

  memcpy(m_pFreqPrev, m_pFreq, FREQ_DATA_SIZE);
}

//-- GetInfo ------------------------------------------------------------------
// Tell XBMC our requirements
//-----------------------------------------------------------------------------
extern "C" void GetInfo(VIS_INFO* pInfo)
{
  pInfo->bWantsFreq = true;
  pInfo->iSyncDelay = 15;
}

#include <iostream>

extern "C" void Render()
{
  //
  // Set up our view
  SetupCamera();
  SetupPerspective();
  m_fRotation+=m_pssSettings[m_iCurrSetting].m_fRotationSpeed;
  std::cout << "rotation at " << m_pssSettings[m_iCurrSetting].m_fRotationSpeed << std::endl;
  SetupRotation(0.0f, 0.0f, m_fRotation);
  glClearColor(0.0, 0.0, 0.0, 1.0);
  glDisable(GL_DEPTH_TEST);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPT_BUFFER_BIT);

  //
  // The particle system will need to know how much time has passed since 
  // the last time it was updated, so we'll need to keep track of how much   
  // time has elapsed since the last frame update...
  //

  m_dwCurTime = time(NULL);
  m_fElapsedTime = (m_dwCurTime - m_dwLastTime)/m_fUpdateSpeed;
  m_dwLastTime = m_dwCurTime;
  m_ParticleSystem.Update( m_fElapsedTime );


  //
  // Prepare to render particle system
  //

  //
  // Setting D3DRS_ZWRITEENABLE to FALSE makes the Z-Buffer read-only, which 
  // helps remove graphical artifacts generated from  rendering a list of 
  // particles that haven't been sorted by distance to the eye.
  //
  // Setting D3DRS_ALPHABLENDENABLE to TRUE allows particles, which overlap, 
  // to alpha blend with each other correctly.
  //

  glDisable(GL_CULL_FACE);
  glEnable(GL_LIGHTING);
  glEnable(GL_BLEND);
  glBlendFunc(GL_ONE, GL_ONE);

  //
  // Render particle system
  //

  m_ParticleSystem.Render();
  glDisable(GL_LIGHTING);
  glDisable(GL_BLEND);
}


void CreateArrays()
{
  memset(m_pFreq, 0, 1024*sizeof(float));
}

void SetDefaults()
{
  m_ParticleSystem.SetMaxParticles(1000);
  SetDefaults(&m_pssSettings[0]);
  SetDefaults(&m_pssSettings[1]);
}

void SetDefaults(ParticleSystemSettings* settings)
{
  settings->m_bAirResistence		= true;
  settings->m_chTexFile			= (char*)szDefaultTexFile;
  settings->m_hsvColor			= HsvColor(0.0f, 1.0f, 0.6f);
  settings->m_dwNumToRelease		= 2;
  settings->m_fNumToReleaseMod	= 0.0f;
  settings->m_fLifeCycle			= 3.0f;
  settings->m_fReleaseInterval	= 0.0f;
  settings->m_fSize				= 0.4f;
  settings->m_fVelocityVar		= 1.5f;

  SetDefaults(&settings->m_esGravity);
  settings->m_esGravity.vector = CVector(0, 0, -15);

  SetDefaults(&settings->m_esPosition);

  SetDefaults(&settings->m_esVelocity);
  settings->m_esVelocity.vector = CVector(-4, 4, 0);

  SetDefaults(&settings->m_esWind);
  settings->m_esWind.vector = CVector(2, -2, 0);

  settings->m_csHue.min		= 0.0f;
  settings->m_csHue.max		= 360.0f;
  settings->m_csHue.shiftRate	= 0.1f;
  settings->m_csHue.modifier	= 360.0f;
  settings->m_csHue.variation	= 45.0f;
  settings->m_csHue.bar		= 1;

  settings->m_csSaturation.min		= 1.0f;
  settings->m_csSaturation.min		= 1.0f;
  settings->m_csSaturation.shiftRate	= 0.0f;
  settings->m_csSaturation.modifier	= 0.0f;
  settings->m_csSaturation.variation	= 0.0f;
  settings->m_csSaturation.bar		= 1;

  settings->m_csValue.min			= 0.2f;
  settings->m_csValue.min			= 0.6f;
  settings->m_csValue.shiftRate	= 0.0005f;
  settings->m_csValue.modifier	= 0.0f;
  settings->m_csValue.variation	= 0.3f;
  settings->m_csValue.bar			= 1;

  settings->m_fRotationSpeed			= 0.1f;
  settings->m_fRotationSensitivity	= 0.02;
  settings->m_iRotationBar			= 1;
}

void SetDefaults(EffectSettings* settings)
{
	settings->bars				= CVector( 1, 1, 1 );
	settings->bInvert			= false;
	settings->modifier			= 0.0f;
	settings->mode				= MODE_BOTH;
	settings->modificationMode	= MODIFICATION_MODE_LINEAR;
	settings->vector			= CVector( 0, 0, 0 );
}

void SetupCamera()
{
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  gluLookAt(0.0, 0.0, -30.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0);

}

void SetupPerspective()
{
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  gluPerspective(45.0, 1.0, 1.0, 100.0);
}

void SetupRotation(float x, float y, float z)
{
  ////Here we will rotate our view around the x, y and z axis.
  glMatrixMode(GL_MODELVIEW);
  glRotatef(x/M_PI*180, 1.0, 0.0, 0.0);
  glRotatef(y/M_PI*180, 0.0, 1.0, 0.0);
  glRotatef(z/M_PI*180, 0.0, 0.0, 1.0);
}

void ShiftColor(ParticleSystemSettings* settings)
{
  float hadjust	= m_pssSettings[m_iCurrSetting].m_csHue.shiftRate;
  float hmin		= m_pssSettings[m_iCurrSetting].m_csHue.min;
  float hmax		= m_pssSettings[m_iCurrSetting].m_csHue.max;

  float sadjust	= m_pssSettings[m_iCurrSetting].m_csSaturation.shiftRate;
  float smin		= m_pssSettings[m_iCurrSetting].m_csSaturation.min;
  float smax		= m_pssSettings[m_iCurrSetting].m_csSaturation.max;

  float vadjust	= m_pssSettings[m_iCurrSetting].m_csValue.shiftRate;
  float vmin		= m_pssSettings[m_iCurrSetting].m_csValue.min;
  float vmax		= m_pssSettings[m_iCurrSetting].m_csValue.max;

  m_clrColor.h += hadjust * m_iHDir;
  if ( m_clrColor.h >= hmax  || m_clrColor.h <= hmin )
  {
    m_clrColor.h -= hadjust * m_iHDir;
    m_iHDir *= -1;
  }

  m_clrColor.h = std::min(m_clrColor.h, hmax);
  m_clrColor.h = std::max(m_clrColor.h, hmin);

  m_clrColor.s += sadjust * m_iSDir;
  if ( m_clrColor.s >= smax  || m_clrColor.s <= smin )
  {
    m_clrColor.s -= sadjust * m_iSDir;
    m_iSDir *= -1;
  }

  m_clrColor.s = std::min(m_clrColor.s, smax);
  m_clrColor.s = std::max(m_clrColor.s, smin);

  m_clrColor.v += vadjust * m_iVDir;
  if ( m_clrColor.v >= vmax  || m_clrColor.v <= vmin )
  {
    m_clrColor.v -= vadjust * m_iVDir;
    m_iVDir *= -1;
  }

  m_clrColor.v = std::min(m_clrColor.v, vmax);
  m_clrColor.v = std::max(m_clrColor.v, vmin);

  float audioh = (m_pFreq[settings->m_csHue.bar] / MAX_LEVEL)			* settings->m_csHue.modifier;
  float audios = (m_pFreq[settings->m_csSaturation.bar] / MAX_LEVEL)	* settings->m_csSaturation.modifier;
  float audiov = (m_pFreq[settings->m_csValue.bar] / MAX_LEVEL)		* settings->m_csValue.modifier;

  float h = m_clrColor.h + audioh;
  while(h > 360)
    h -= 360;

  float s = m_clrColor.s + audios;
  while(s > 1)
    s -= 1;

  float v = m_clrColor.v + audiov;
  while(v > 1)
    v -= 1;

  h = std::min(h, hmax);
  h = std::max(h, hmin);

  s = std::min(s, smax);
  s = std::max(s, smin);

  v = std::min(v, vmax);
  v = std::max(v, vmin);

  m_ParticleSystem.SetColor( HsvColor(h, s, v) );
}

void InitParticleSystem(ParticleSystemSettings settings)
{
	//m_chTexFile		= settings.m_chTexFile;
  m_ParticleSystem.SetNumToRelease	( settings.m_dwNumToRelease );
  m_ParticleSystem.SetReleaseInterval	( settings.m_fReleaseInterval );
  m_ParticleSystem.SetLifeCycle		( settings.m_fLifeCycle );
  m_ParticleSystem.SetSize			( settings.m_fSize );
  m_ParticleSystem.SetColor			( settings.m_hsvColor );
  m_ParticleSystem.SetPosition		( settings.m_esPosition.vector );
  m_ParticleSystem.SetVelocity		( settings.m_esVelocity.vector );
  m_ParticleSystem.SetGravity			( settings.m_esGravity.vector );
  m_ParticleSystem.SetWind			( settings.m_esWind.vector );
  m_ParticleSystem.SetAirResistence	( settings.m_bAirResistence );
  m_ParticleSystem.SetVelocityVar		( settings.m_fVelocityVar );

  m_ParticleSystem.SetMaxH			( settings.m_csHue.max );
  m_ParticleSystem.SetMinH			( settings.m_csHue.min );
  m_ParticleSystem.SetHVar			( settings.m_csHue.variation );

  m_ParticleSystem.SetMaxS			( settings.m_csSaturation.max );
  m_ParticleSystem.SetMinS			( settings.m_csSaturation.min );
  m_ParticleSystem.SetSVar			( settings.m_csSaturation.variation );

  m_ParticleSystem.SetMaxV			( settings.m_csValue.max );
  m_ParticleSystem.SetMinV			( settings.m_csValue.min );
  m_ParticleSystem.SetVVar			( settings.m_csValue.variation );

  char tmp[1024];
  XBMC->GetSetting("__addonpath__", tmp);
  strcat(tmp, "/resources/particle.bmp");
  m_ParticleSystem.SetTexture(tmp);
}

CVector Shift(EffectSettings* settings)
{
  int xBand = std::min(m_iBars, (int)settings->bars.x);
  int yBand = std::min(m_iBars, (int)settings->bars.y);
  int zBand = std::min(m_iBars, (int)settings->bars.z);

  xBand-=1;
  xBand*=2;

  yBand-=1;
  yBand*=2;

  zBand-=1;
  zBand*=2;

  if (settings->modifier == 0.0f)
    return settings->vector;

  float x, y, z;

  if (settings->mode == MODE_DIFFERENCE)
  {
    x = abs((m_pFreq[xBand] - m_pFreqPrev[xBand])/MAX_LEVEL);
    y = abs((m_pFreq[yBand] - m_pFreqPrev[yBand])/MAX_LEVEL);
    z = abs((m_pFreq[zBand] - m_pFreqPrev[zBand])/MAX_LEVEL);
  }
  else if (settings->mode == MODE_LEVEL)
  {
    x = m_pFreq[xBand]/MAX_LEVEL;
    y = m_pFreq[yBand]/MAX_LEVEL;
    z = m_pFreq[zBand]/MAX_LEVEL;
  }
  else
  {
    x = std::max(1.f, (m_pFreq[xBand] + abs(m_pFreq[xBand] - m_pFreqPrev[xBand]))/MAX_LEVEL);
    y = std::max(1.f, (m_pFreq[yBand] + abs(m_pFreq[yBand] - m_pFreqPrev[yBand]))/MAX_LEVEL);
    z = std::max(1.f, (m_pFreq[zBand] + abs(m_pFreq[zBand] - m_pFreqPrev[zBand]))/MAX_LEVEL);
  }

  if (settings->bInvert)
  {
    x = 1 - x;
    y = 1 - y;
    z = 1 - z;
  }

  x = getRandomMinMax(-x, x);
  y = getRandomMinMax(-y, y);
  z = getRandomMinMax(-z, z);

  if (settings->modificationMode == MODIFICATION_MODE_LINEAR)
  {
    x = (x * settings->modifier * settings->vector.x);
    y = (y * settings->modifier * settings->vector.y);
    z = (z * settings->modifier * settings->vector.z);
  }
  else
  {
    //hack--for some reason, floats seem to cause powf to break ???
    int mod = settings->modifier;
    x = (powf((x + 1) * settings->vector.x, mod));
    y = (powf((y + 1) * settings->vector.y, mod));
    z = (powf((z + 1) * settings->vector.z, mod));

    x = randomizeSign(x);
    y = randomizeSign(y);
    z = randomizeSign(z);
  }

  x += settings->vector.x;
  y += settings->vector.y;
  z += settings->vector.z;

  return CVector(x, y, z);
}

//-- GetSubModules ------------------------------------------------------------
// Return any sub modules supported by this vis
//-----------------------------------------------------------------------------
extern "C" unsigned int GetSubModules(char ***names)
{
  return 0; // this vis supports 0 sub modules
}

//-- OnAction -----------------------------------------------------------------
// Handle XBMC actions such as next preset, lock preset, album art changed etc
//-----------------------------------------------------------------------------
extern "C" bool OnAction(long flags, const void *param)
{
  bool ret = false;
  return ret;
}

//-- GetPresets ---------------------------------------------------------------
// Return a list of presets to XBMC for display
//-----------------------------------------------------------------------------
extern "C" unsigned int GetPresets(char ***presets)
{
  return 0;
}

//-- GetPreset ----------------------------------------------------------------
// Return the index of the current playing preset
//-----------------------------------------------------------------------------
extern "C" unsigned GetPreset()
{
  return 0;
}

//-- IsLocked -----------------------------------------------------------------
// Returns true if this add-on use settings
//-----------------------------------------------------------------------------
extern "C" bool IsLocked()
{
  return false;
}

//-- Stop ---------------------------------------------------------------------
// This dll must cease all runtime activities
// !!! Add-on master function !!!
//-----------------------------------------------------------------------------
extern "C" void ADDON_Stop()
{
  m_ParticleSystem.dtor();
}

//-- Destroy ------------------------------------------------------------------
// Do everything before unload of this add-on
// !!! Add-on master function !!!
//-----------------------------------------------------------------------------
extern "C" void ADDON_Destroy()
{
}

//-- HasSettings --------------------------------------------------------------
// Returns true if this add-on use settings
// !!! Add-on master function !!!
//-----------------------------------------------------------------------------
extern "C" bool ADDON_HasSettings()
{
  return false;
}

//-- GetStatus ---------------------------------------------------------------
// Returns the current Status of this visualisation
// !!! Add-on master function !!!
//-----------------------------------------------------------------------------
extern "C" ADDON_STATUS ADDON_GetStatus()
{
  return ADDON_STATUS_OK;
}

//-- GetSettings --------------------------------------------------------------
// Return the settings for XBMC to display
// !!! Add-on master function !!!
//-----------------------------------------------------------------------------
extern "C" unsigned int ADDON_GetSettings(ADDON_StructSetting ***sSet)
{
  return 0;
}

//-- FreeSettings --------------------------------------------------------------
// Free the settings struct passed from XBMC
// !!! Add-on master function !!!
//-----------------------------------------------------------------------------

extern "C" void ADDON_FreeSettings()
{
}

//-- SetSetting ---------------------------------------------------------------
// Set a specific Setting value (called from XBMC)
// !!! Add-on master function !!!
//-----------------------------------------------------------------------------
extern "C" ADDON_STATUS ADDON_SetSetting(const char *strSetting, const void* value)
{
  return ADDON_STATUS_OK;
}

//-- Announce -----------------------------------------------------------------
// Receive announcements from XBMC
// !!! Add-on master function !!!
//-----------------------------------------------------------------------------
extern "C" void ADDON_Announce(const char *flag, const char *sender, const char *message, const void *data)
{
}
