// -*-Mode: C++;-*-
//
// Density object class
//
// $Id: DensityMap.cpp,v 1.4 2010/09/11 17:54:46 rishitani Exp $

#include <common.h>
#include "DensityMap.hpp"
#include "QdfDenMapWriter.hpp"

#ifdef WIN32
#define USE_TBO
#endif

#include <qlib/Box3D.hpp>
#include <qsys/View.hpp>

using namespace xtal;
using symm::CrystalInfo;

// default constructor
DensityMap::DensityMap()
{
  m_nColInt = m_nRowInt = m_nSecInt = 0;
  m_nCols = m_nRows = m_nSecs = 0;
  m_nStartCol = m_nStartRow = m_nStartSec = 0;

  m_dMinMap = m_dMaxMap = m_dMeanMap = m_dRmsdMap = 0.0;
  m_pByteMap = NULL;
  m_pFloatMap = NULL;
  // m_pRecipAry = NULL;
  m_pHKLList = NULL;

  m_dLevelBase = m_dLevelStep = 0.0;

//  m_bUseMolBndry = false;
  m_pMapTex = NULL;

  m_pCCtxt = NULL;
}

DensityMap::~DensityMap()
{
  if (m_pByteMap!=NULL)
    delete m_pByteMap;

  //if (m_pRecipAry!=NULL)
  //delete m_pRecipAry;

  if (m_pFloatMap!=NULL)
    delete m_pFloatMap;

  if (m_pMapTex!=NULL)
    delete m_pMapTex;
}

///////////////////////////////////////////////
// setup density map

// construct by float array
void DensityMap::setMapFloatArray(const float *array,
				  int ncol, int nrow, int nsect,
                                  int axcol, int axrow, int axsect)
{
  int ntotal = ncol*nrow*nsect;
  m_nCols = ncol;
  m_nRows = nrow;
  m_nSecs = nsect;

  // calculate the statistics of the map
  double
    rhomax=MAP_FLOAT_MIN, rhomin=MAP_FLOAT_MAX,
    rhomean=0.0, sqmean=0.0,
    rhodev=0.0;

  for (int i=0; i<ntotal; i++) {
    double rho = double( array[i] );
    rhomean += rho;
    sqmean += rho*rho;
    rhomax = qlib::max(rhomax, rho);
    rhomin = qlib::min(rhomin, rho);
  }

  rhomean /= double(ntotal);
  sqmean /= double(ntotal);

  rhodev = sqrt(sqmean-rhomean*rhomean);

  m_dMinMap = rhomin;
  m_dMaxMap = rhomax;
  m_dMeanMap = rhomean;
  m_dRmsdMap = rhodev;

  LOG_DPRINT("DensityMap.load> calculated stats:\n");
  LOG_DPRINT("  map minimum density  : %f\n", rhomin);
  LOG_DPRINT("  map maximum density  : %f\n", rhomax);
  LOG_DPRINT("  map mean density     : %f\n", rhomean);
  LOG_DPRINT("  map density r.m.s.d. : %f\n", rhodev);

  // map truncation
  m_dLevelStep = (rhomax - rhomin)/256.0;
  m_dLevelBase = rhomin;

  MB_DPRINT("truncating to 8bit map base: %f, step: %f\n",
	    m_dLevelBase, m_dLevelStep);

  //
  //

  rotate(m_nCols,m_nRows,m_nSecs,axcol,axrow,axsect);

  //if (m_pRecipAry!=NULL)
  //delete m_pRecipAry;
  //m_pRecipAry = NULL;

  if (m_pFloatMap!=NULL)
    delete m_pFloatMap;
  m_pFloatMap = MB_NEW FloatArray(m_nCols,m_nRows,m_nSecs);

  if (m_pByteMap!=NULL)
    delete m_pByteMap;
  m_pByteMap = MB_NEW ByteArray(m_nCols,m_nRows,m_nSecs);

  int i, j, k, ii, jj, kk;
  for (k=0; k<nsect; k++)
    for (j=0; j<nrow; j++)
      for (i=0; i<ncol; i++) {
        const double drho = (double)array[i + (j + k*nrow)*ncol];
        const double rho = qlib::clamp((drho-m_dLevelBase)/m_dLevelStep, 0.0, 255.0);
        ii=i,jj=j,kk=k;
	rotate(ii,jj,kk,axcol,axrow,axsect);
        m_pByteMap->at(ii,jj,kk) = qbyte(rho);
        m_pFloatMap->at(ii,jj,kk) = qfloat32(drho);
      }

  if (m_pMapTex!=NULL)
    delete m_pMapTex;
  m_pMapTex = NULL;


  MB_DPRINTLN("OK.");
}

/**
 construct by uchar array
 array must be sorted by the Fast-Medium-Slow order
 */
void DensityMap::setMapByteArray(const unsigned char*array,
                                 int ncol, int nrow, int nsect,
                                 double rhomin, double rhomax, double mean, double sigma)
{
  int ntotal = ncol*nrow*nsect;
  m_nCols = ncol;
  m_nRows = nrow;
  m_nSecs = nsect;

  m_dMinMap = rhomin;
  m_dMaxMap = rhomax;
  m_dMeanMap = mean;
  m_dRmsdMap = sigma;

  MB_DPRINTLN("load density map ...");
  MB_DPRINTLN("   minimum: %f", rhomin);
  MB_DPRINTLN("   maximum: %f", rhomax);
  MB_DPRINTLN("   mean   : %f", mean);
  MB_DPRINTLN("   r.m.s.d: %f", sigma);

  // calc map trunc params
  m_dLevelStep = (double)(rhomax - rhomin)/256.0;
  m_dLevelBase = rhomin;

  //if (m_pRecipAry!=NULL)
  //delete m_pRecipAry;
  //m_pRecipAry = NULL;

  if (m_pFloatMap!=NULL)
    delete m_pFloatMap;
  m_pFloatMap = NULL;

  if (m_pByteMap!=NULL)
    delete m_pByteMap;
  m_pByteMap = MB_NEW ByteArray(m_nCols,m_nRows,m_nSecs, array);

  if (m_pMapTex!=NULL)
    delete m_pMapTex;
  m_pMapTex = NULL;


  MB_DPRINTLN("OK.");
}

// setup column, row, section params
void DensityMap::setMapParams(int stacol, int starow, int stasect,
			      int intcol, int introw, int intsect)
{
  m_nStartCol = stacol;
  m_nStartRow = starow;
  m_nStartSec = stasect;

  m_nColInt = intcol;
  m_nRowInt = introw;
  m_nSecInt = intsect;
}

// setup crystal system's parameters
void DensityMap::setXtalParams(double a, double b, double c,
                               double alpha, double beta, double gamma, int nsg /* = 1 (P1) */)
{
  m_xtalInfo.setCellDimension(a,b,c,alpha,beta,gamma);
  m_xtalInfo.setSG(nsg);

  CrystalInfo *pci = new CrystalInfo(a,b,c,alpha,beta,gamma,nsg);
  this->setExtData(qsys::ObjExtDataPtr(pci));
}

Vector4D DensityMap::getCenter() const
{
/*
  Vector4D fcen;
  fcen.x() = (double(m_nStartCol)+double(m_nCols)/2.0)/double(m_nColInt);
  fcen.y() = (double(m_nStartRow)+double(m_nRows)/2.0)/double(m_nRowInt);
  fcen.z() = (double(m_nStartSec)+double(m_nSecs)/2.0)/double(m_nSecInt);
  
  m_xtalInfo.fracToOrth(fcen);
  return fcen;
*/
  Vector4D tv(double(m_nStartCol+m_nCols)/2.0,
              double(m_nStartRow+m_nRows)/2.0,
              double(m_nStartSec+m_nSecs)/2.0);
  tv = convToOrth(tv);
  return tv;
}

/*
void DensityMap::dump()
{
  LOG_DPRINT("DensityMap dump...\n");
  LOG_DPRINT("  map size  : (%d,%d,%d)\n", 
             m_pByteMap->shape()[0],
             m_pByteMap->shape()[1],
             m_pByteMap->shape()[2]);
             //m_pByteMap->getColumns(),
             //m_pByteMap->getRows(),
             //m_pByteMap->getSections());
  LOG_DPRINT("  map start : (%d,%d,%d)\n",
            m_nStartCol, m_nStartRow, m_nStartSec);
  LOG_DPRINT("  map intrv : (%d,%d,%d)\n",
             m_nColInt, m_nRowInt, m_nSecInt);
  LOG_DPRINT("  unit cell a=%.2fA, b=%.2fA, c=%.2fA,\n",
             m_xtalInfo.a(), m_xtalInfo.b(), m_xtalInfo.c());
  LOG_DPRINT("    alpha=%.2fdeg, beta=%.2fdeg, gamma=%.2fdeg,\n",
             m_xtalInfo.alpha(), m_xtalInfo.beta(), m_xtalInfo.gamma());
  LOG_DPRINT("  map minimum density  : %f\n", m_dMinMap);
  LOG_DPRINT("  map maximum density  : %f\n", m_dMaxMap);
  LOG_DPRINT("  map mean density     : %f\n", m_dMeanMap);
  LOG_DPRINT("  map density r.m.s.d. : %f\n", m_dRmsdMap);
}
*/

double DensityMap::getValueAt(const Vector4D &pos) const
{
  Vector4D tv(pos);

  const Matrix4D &xfm = getXformMatrix();
  if (!xfm.isIdent()) {
    // apply inv of xformMat
    qlib::Matrix3D rmat = xfm.getMatrix3D();
    rmat = rmat.invert();
    Vector4D tr = xfm.getTransPart();
    tv -= tr;
    tv = rmat.mulvec(tv);
  }

  m_xtalInfo.orthToFrac(tv);
  
  tv.x() *= double(getColInterval());
  tv.y() *= double(getRowInterval());
  tv.z() *= double(getSecInterval());

  tv.x() -= double(m_nStartCol);
  tv.y() -= double(m_nStartRow);
  tv.z() -= double(m_nStartSec);

  //if (tv.x()<0.0 || tv.x()>m_pByteMap->getColumns() ||
  //tv.y()<0.0 || tv.y()>m_pByteMap->getRows() ||
  //tv.z()<0.0 || tv.z()>m_pByteMap->getSections())

  const int i = int(tv.x());
  const int j = int(tv.y());
  const int k = int(tv.z());

  if (!isInBoundary(i,j,k)) return 0.0;
  return atFloat( i,j,k );
}

bool DensityMap::isInRange(const Vector4D &pos) const
{
  Vector4D tv(pos);

  const Matrix4D &xfm = getXformMatrix();
  if (!xfm.isIdent()) {
    // apply inv of xformMat
    qlib::Matrix3D rmat = xfm.getMatrix3D();
    rmat = rmat.invert();
    Vector4D tr = xfm.getTransPart();
    tv -= tr;
    tv = rmat.mulvec(tv);
  }

  m_xtalInfo.orthToFrac(tv);
  
  tv.x() *= double(getColInterval());
  tv.y() *= double(getRowInterval());
  tv.z() *= double(getSecInterval());

  tv.x() -= double(m_nStartCol);
  tv.y() -= double(m_nStartRow);
  tv.z() -= double(m_nStartSec);

  //if (tv.x()<0.0 || tv.x()>m_pByteMap->getColumns() ||
  //tv.y()<0.0 || tv.y()>m_pByteMap->getRows() ||
  //tv.z()<0.0 || tv.z()>m_pByteMap->getSections())

  const int i = int(tv.x());
  const int j = int(tv.y());
  const int k = int(tv.z());
  
  return isInBoundary(i,j,k);
}

Vector4D DensityMap::convToOrth(const Vector4D &index) const
{
  Vector4D tv = index;

  tv.x() += double(m_nStartCol);
  tv.y() += double(m_nStartRow);
  tv.z() += double(m_nStartSec);

  // interval == 1/(grid spacing)
  tv.x() /= double(getColInterval());
  tv.y() /= double(getRowInterval());
  tv.z() /= double(getSecInterval());

  // tv is now fractional coord.
  // conv frac-->orth
  m_xtalInfo.fracToOrth(tv);

  // tv is now in orthogonal coord.

  const Matrix4D &xfm = getXformMatrix();
  if (!xfm.isIdent()) {
    // Apply xformMat
    tv.w() = 1.0;
    tv = xfm.mulvec(tv);
  }

  return tv;
}

//////////////////////////////////////////////////////////

double DensityMap::getRmsdDensity() const
{
  return m_dRmsdMap;
}

double DensityMap::getLevelBase() const
{
  return m_dLevelBase;
}

double DensityMap::getLevelStep() const
{
  return m_dLevelStep;
}

bool DensityMap::isInBoundary(int i, int j, int k) const
{
  /*
  if (i<0 || m_pByteMap->shape()[0]<=i)
    return false;
  if (j<0 || m_pByteMap->shape()[1]<=j)
    return false;
  if (k<0 || m_pByteMap->shape()[2]<=k)
    return false;
*/

  if (i<0 || m_pByteMap->getColumns()<=i)
    return false;
  if (j<0.0 || m_pByteMap->getRows()<=j)
    return false;
  if (k<0.0 || m_pByteMap->getSections()<=k)
    return false;

  return true;
}

qbyte DensityMap::atByte(int i, int j, int k) const
{
//  if (m_bUseBndry)
//    return getAtWithBndry(i, j, k);
//  else 
//    return (*m_pByteMap)[i][j][k];
  //return (*m_pByteMap)[i][j][k];
  return m_pByteMap->at(i,j,k);
}

double DensityMap::atFloat(int i, int j, int k) const
{
  if (m_pFloatMap!=NULL)
    return m_pFloatMap->at(i,j,k);

  qbyte b = atByte(i,j,k);
  return double(b)*m_dLevelStep + m_dLevelBase;
}

/*
unsigned char DensityMap::getAtWithBndry(int nx, int ny, int nz) const
{
  Vector4D tv(nx, ny, nz);

  tv.x() += double(m_nStartCol);
  tv.y() += double(m_nStartRow);
  tv.z() += double(m_nStartSec);

  // interval == 1/(grid spacing)
  tv.x() /= double(getColInterval());
  tv.y() /= double(getRowInterval());
  tv.z() /= double(getSecInterval());

  // tv is now fractional coord.
  // conv frac-->orth
  m_xtalInfo.fracToOrth(tv);

  // tv is now in orthogonal coord.

  //if (!m_boundary.collChk(tv, m_dBndryRng)) {
  //return 0;
  //}

  //return (*m_pByteMap)[nx][ny][nz];
  return m_pByteMap->at(nx,ny,nz);
}
*/

Vector4D DensityMap::getOrigin() const
{
  return Vector4D(0,0,0);
}

double DensityMap::getColGridSize() const
{
  return (getXtalInfo().a()) / double(m_nColInt);
}

double DensityMap::getRowGridSize() const
{
  return (getXtalInfo().b()) / double(m_nRowInt);
}

double DensityMap::getSecGridSize() const
{
  return (getXtalInfo().c()) / double(m_nSecInt);
}

//////////

LString DensityMap::getDataChunkReaderName(int nQdfVer) const
{
  return LString("qdfmap");
}

void DensityMap::writeDataChunkTo(qlib::LDom2OutStream &oos) const
{
  QdfDenMapWriter writer;
  writer.setVersion(oos.getQdfVer());
  writer.setEncType(oos.getQdfEncType());

  DensityMap *pthis = const_cast<DensityMap *>(this);
  writer.attach(qsys::ObjectPtr(pthis));
  writer.write(oos);
  writer.detach();
}


#if 0
LString DensityMap::getHistogramJSON(double min, double max, int nbins)
{
  double dbinw = (max-min)/double(nbins);
  //int nbins = int( (m_dMaxMap-m_dMinMap)/dbinw );
  MB_DPRINTLN("DenMap.hist> nbins=%d", nbins);

  int ni = m_pByteMap->getColumns();
  int nj = m_pByteMap->getRows();
  int nk = m_pByteMap->getSections();

  std::vector<int> histo(nbins);
  for (int i=0; i<nbins; ++i)
    histo[i] = 0;
  
  for (int i=0; i<ni; ++i)
    for (int j=0; j<nj; ++j)
      for (int k=0; k<nk; ++k) {
        double rho = atFloat(i,j,k);
        int ind = (int) ::floor( (rho-min)/dbinw );
        if (ind<0 || ind>=nbins) {
          //MB_DPRINTLN("ERROR!! invalid density value at (%d,%d,%d)=%f", i,j,k,rho);
        }
        else {
          histo[ind]++;
        }
      }
        
  int nmax = 0;
  for (int i=0; i<nbins; ++i)
    nmax = qlib::max(histo[i], nmax);

  LString rval = "{";
  rval += LString::format("\"min\":%f,\n", m_dMinMap/m_dRmsdMap);
  rval += LString::format("\"max\":%f,\n", m_dMaxMap/m_dRmsdMap);
  rval += LString::format("\"nbin\":%d,\n", nbins);
  rval += LString::format("\"nmax\":%d,\n", nmax);
  rval += LString::format("\"sig\":%f,\n", m_dRmsdMap);
  rval += "\"histo\":[";
  for (int i=0; i<nbins; ++i) {
    MB_DPRINTLN("%d %d", i, histo[i]);
    if (i>0)
      rval += ",";
    rval += LString::format("%d", histo[i]);
  }
  rval += "]}\n";
  
  return rval;
}
#endif

using qlib::Vector4D;
using qlib::Matrix4D;
using qlib::Matrix3D;
using qlib::Box3D;

void DensityMap::fitView(const qsys::ViewPtr &pView, bool dummy) const
{
  qlib::LQuat rotq = pView->getRotQuat();
  Matrix4D ecmat = Matrix4D::makeRotMat(rotq);

  Box3D bbox, ecbbox;

  {
    Vector4D vpos;

    // get object xform
    Matrix4D xform = getXformMatrix();

    // get frac-->orth matrix
    Matrix3D orthmat = getXtalInfo().getOrthMat();
    xform.matprod( Matrix4D(orthmat) );

    bbox.merge( xform.mulvec(Vector4D(0,0,0,1)) );
    bbox.merge( xform.mulvec(Vector4D(1,0,0,1)) );
    bbox.merge( xform.mulvec(Vector4D(0,1,0,1)) );
    bbox.merge( xform.mulvec(Vector4D(0,0,1,1)) );
    bbox.merge( xform.mulvec(Vector4D(1,1,0,1)) );
    bbox.merge( xform.mulvec(Vector4D(0,1,1,1)) );
    bbox.merge( xform.mulvec(Vector4D(1,0,1,1)) );
    bbox.merge( xform.mulvec(Vector4D(1,1,1,1)) );

    // xform = xform * ecmat
    xform.matprod( ecmat );

    ecbbox.merge( xform.mulvec(Vector4D(0,0,0,1)) );
    ecbbox.merge( xform.mulvec(Vector4D(1,0,0,1)) );
    ecbbox.merge( xform.mulvec(Vector4D(0,1,0,1)) );
    ecbbox.merge( xform.mulvec(Vector4D(0,0,1,1)) );
    ecbbox.merge( xform.mulvec(Vector4D(1,1,0,1)) );
    ecbbox.merge( xform.mulvec(Vector4D(0,1,1,1)) );
    ecbbox.merge( xform.mulvec(Vector4D(1,0,1,1)) );
    ecbbox.merge( xform.mulvec(Vector4D(1,1,1,1)) );
  }
  
  MB_DPRINTLN("map bbox start: (%f,%f,%f)", bbox.vstart.x(), bbox.vstart.y(), bbox.vstart.z());
  MB_DPRINTLN("map bbox   end: (%f,%f,%f)", bbox.vend.x(), bbox.vend.y(), bbox.vend.z());

  MB_DPRINTLN("map ec bbox start: (%f,%f,%f)", ecbbox.vstart.x(), ecbbox.vstart.y(), ecbbox.vstart.z());
  MB_DPRINTLN("map ec bbox   end: (%f,%f,%f)", ecbbox.vend.x(), ecbbox.vend.y(), ecbbox.vend.z());

  /*{
    // inflate box by 20%
    Vector4D dv = (bbox.vend - bbox.vstart).scale(0.2);
    bbox.vend += dv;
    bbox.vstart -= dv;
  }*/

  pView->setViewCenter( bbox.center() );

  Vector4D ecboxst = ecbbox.vstart - ecbbox.center();
  Vector4D ecboxen = ecbbox.vend - ecbbox.center();
  
  int cx = pView->getWidth();
  int cy = pView->getHeight();
  double fasp = double(cx)/double(cy);

  double mx = qlib::abs(ecboxen.x()-ecboxst.x());
  double my = qlib::abs(ecboxen.y()-ecboxst.y());
  double masp = mx / my;

  MB_DPRINTLN("mx: %f", mx);
  MB_DPRINTLN("my: %f", my);
  MB_DPRINTLN("fasp: %f", fasp);
  MB_DPRINTLN("masp: %f", masp);

  double zoom;
  if (fasp>1.0) {
    if (masp>fasp) {
      zoom = mx/fasp;
    }
    else {
      zoom = my;
    }
  }
  else {
    if (masp>fasp) {
      zoom = mx/fasp;
    }
    else {
      zoom = my;
    }
  }

  // MB_DPRINTLN("Zoom: %f", zoom);
  pView->setZoom(zoom);

  double sd = qlib::abs(ecboxen.z()-ecboxst.z());
  if (pView->getSlabDepth()>sd)
    return;
  
  if (pView->getViewDist()*2>sd)
    pView->setSlabDepth(sd);
  else {
    // slab depth is wider than the view distance
    // --> have to change the view distance to enough accomodate the slab depth
    pView->setViewDist(sd/2);
    pView->setSlabDepth(sd);
  }

}

gfx::Texture *DensityMap::getMapTex() const
{
  if (m_pMapTex!=NULL)
    return m_pMapTex;

  m_pMapTex = MB_NEW gfx::Texture();
#ifdef USE_TBO
  m_pMapTex->setup(gfx::Texture::DIM_DATA,
                   gfx::Texture::FMT_R,
                   gfx::Texture::TYPE_UINT8);
#else
  m_pMapTex->setup(gfx::Texture::DIM_3D,
                   gfx::Texture::FMT_R,
                   gfx::Texture::TYPE_UINT8);
#endif
  
  int ni = m_pByteMap->getColumns();
  int nj = m_pByteMap->getRows();
  int nk = m_pByteMap->getSections();

#ifdef USE_TBO
  m_pMapTex->setData(ni*nj*nk, 1, 1, m_pByteMap->data());
#else
  m_pMapTex->setData(ni, nj, nk, m_pByteMap->data());
#endif
  

  return m_pMapTex;
}


void DensityMap::setHKLList(HKLList *pHKLList)
{
  if (m_pHKLList!=NULL)
    delete m_pHKLList;
  m_pHKLList = pHKLList;

  m_nCols = m_pHKLList->m_na;
  m_nRows = m_pHKLList->m_nb;
  m_nSecs = m_pHKLList->m_nc;
  setMapParams(0,0,0,m_nCols,m_nRows,m_nSecs);

  CompArray recip_ary;
  m_pHKLList->convToArrayHerm(recip_ary, 0.0f);

  if (m_pFloatMap!=NULL)
    delete m_pFloatMap;
  m_pFloatMap = MB_NEW FloatArray(m_nCols,m_nRows,m_nSecs);

  FFTUtil fft;
  fft.doit(recip_ary, *m_pFloatMap);

  MB_DPRINTLN("FFT OK");

  calcMapStats();

  if (m_pByteMap!=NULL)
    delete m_pByteMap;
  m_pByteMap = MB_NEW ByteArray(m_nCols,m_nRows,m_nSecs);

  createByteMap();

  if (m_pMapTex!=NULL)
    delete m_pMapTex;
  m_pMapTex = NULL;
}

void DensityMap::calcMapStats()
{
  MB_ASSERT(m_pFloatMap!=NULL);
  
  calcMapStats(*m_pFloatMap,m_dMinMap,m_dMaxMap,m_dMeanMap,m_dRmsdMap);

  // map truncation
  m_dLevelStep = (m_dMaxMap-m_dMinMap)/256.0;
  m_dLevelBase = m_dMinMap;

  LOG_DPRINTLN("Map statistics:");
  LOG_DPRINTLN("   minimum: %f", m_dMinMap);
  LOG_DPRINTLN("   maximum: %f", m_dMaxMap);
  LOG_DPRINTLN("   mean   : %f", m_dMeanMap);
  LOG_DPRINTLN("   r.m.s.d: %f", m_dRmsdMap);
}

void DensityMap::calcMapStats(const FloatArray &map, double &aMinMap, double &aMaxMap, double &aMeanMap, double &aRmsdMap)
{
  const int ntotal = map.size();

  // calculate the statistics of the map
  double
    rhomax=MAP_FLOAT_MIN, rhomin=MAP_FLOAT_MAX,
    rhomean=0.0, sqmean=0.0,
    rhodev=0.0;

  for (int i=0; i<ntotal; i++) {
    double rho = double( map.at(i) );
    rhomean += rho;
    sqmean += rho*rho;
    rhomax = qlib::max(rhomax, rho);
    rhomin = qlib::min(rhomin, rho);
  }

  rhomean /= double(ntotal);
  sqmean /= double(ntotal);

  rhodev = sqrt(sqmean-rhomean*rhomean);

  aMinMap = rhomin;
  aMaxMap = rhomax;
  aMeanMap = rhomean;
  aRmsdMap = rhodev;
}

void DensityMap::createByteMap()
{
  createByteMap(*m_pFloatMap, *m_pByteMap, m_dLevelBase, m_dLevelStep);
}

void DensityMap::createByteMap(const FloatArray &fmap, ByteArray &bmap,
                               double base, double step)
{
  int i;
  const int ntotal = fmap.size();
  
  for (i=0; i<ntotal; i++) {
    const double drho = fmap.at(i);
    const double rho = qlib::clamp((drho-base)/step, 0.0, 255.0);
    bmap.at(i) = qbyte(rho);
  }
}

void DensityMap::updateByteMap()
{
  if (m_pMapTex!=NULL) {

    int ni = m_pByteMap->getColumns();
    int nj = m_pByteMap->getRows();
    int nk = m_pByteMap->getSections();
    
#ifdef USE_TBO
    m_pMapTex->setData(ni*nj*nk, 1, 1, m_pByteMap->data());
#else
    m_pMapTex->setData(ni, nj, nk, m_pByteMap->data());
#endif

    
  }
  else {
    getMapTex();
  }

  // cuda_test1();

  fireMapPreviewChgEvent();
}

void DensityMap::fireMapPreviewChgEvent()
{
  // notify update
  qsys::ObjectEvent obe;
  obe.setType(qsys::ObjectEvent::OBE_CHANGED_DYNAMIC);
  obe.setTarget(getUID());
  obe.setDescr("densityModified");
  fireObjectEvent(obe);
}

void DensityMap::fireMapChgEvent()
{
  // notify update
  qsys::ObjectEvent obe;
  obe.setType(qsys::ObjectEvent::OBE_CHANGED);
  obe.setTarget(getUID());
  obe.setDescr("densityModified");
  fireObjectEvent(obe);
}


#if 0
void launchTestKernel(unsigned char *input, unsigned char *output, int len);
//void launchTestKernel(const gfx::ComputeArray *input, gfx::ComputeArray *output);

#include <sysdep/OglTexture.hpp>
#include <sysdep/CudartCompContext.hpp>

void convToTex(const gfx::ComputeArray *pCA_in, gfx::Texture *pTex_out)
{
  const sysdep::CudartCompArray *pcin = static_cast<const sysdep::CudartCompArray *>(pCA_in);
  unsigned char *input = (unsigned char *) pcin->getHandle();

  //

  sysdep::OglTextureRep *pRep = dynamic_cast<sysdep::OglTextureRep *>(pTex_out->getRep());
  GLuint vbo = pRep->getBufID();

  //

  cudaGraphicsResource *vbo_cuda;
  chkCudaErr(
    cudaGraphicsGLRegisterBuffer( &vbo_cuda, vbo, cudaGraphicsMapFlagsWriteDiscard ));
  chkCudaErr(
    cudaGraphicsMapResources(1, &vbo_cuda, 0));
  
  unsigned char* v;
  size_t bytes;
  chkCudaErr(
    cudaGraphicsResourceGetMappedPointer((void**)&v, &bytes, vbo_cuda)
    );
  
  launchTestKernel(input, v, pCA_in->getElemCount());
  //chkCudaErr(
  //cudaMemcpy(v, input, pCA_in->getElemCount() * pCA_in->getElemSize(), cudaMemcpyDeviceToDevice)
  //);
    

  chkCudaErr(
    cudaGraphicsUnmapResources(1, &vbo_cuda, 0)
    );

}

void DensityMap::cuda_test1()
{
  if (m_pCCtxt==NULL) {
    MB_DPRINTLN("CUDA context is not created.");
    return;
  }

  if (m_pMapTex==NULL) {
    getMapTex();
  }

  gfx::ComputeArray *pCA_in = m_pCCtxt->createArray();
  pCA_in->initWith(*m_pByteMap);

  convToTex(pCA_in, m_pMapTex);
  
  /*
  FloatArray map2(m_pFloatMap->cols(), m_pFloatMap->rows(), m_pFloatMap->secs());
  gfx::ComputeArray *pCA_out = m_pCCtxt->createArray();
  pCA_out->alloc(map2.size(), sizeof(FloatArray::value_type));
  
  {
    //launchTestKernel(pin, pout, nlen);
    launchTestKernel(pCA_in, pCA_out);
  }

    pCA_out->copyTo(map2);

    delete pCA_in;
    delete pCA_out;
  }*/

  delete pCA_in;
}
#endif

#if 0
#include "FFTUtil.hpp"
void DensityMap::setRecipArray(const CompArray &data, int na, int nb, int nc)
{
  m_nCols = na;
  m_nRows = nb;
  m_nSecs = nc;
  setMapParams(0,0,0,na,nb,nc);

  if (m_pRecipAry!=NULL)
    delete m_pRecipAry;
  m_pRecipAry = MB_NEW CompArray(data);

  if (m_pFloatMap!=NULL)
    delete m_pFloatMap;
  m_pFloatMap = MB_NEW FloatArray(m_nCols,m_nRows,m_nSecs);

  FFTUtil fft;
  //fft.doit(*m_pRecipAry, *m_pFloatMap);
  fft.doit(const_cast<CompArray&>(data), *m_pFloatMap);

  MB_DPRINTLN("FFT OK");

  calcMapStats();

  if (m_pByteMap!=NULL)
    delete m_pByteMap;
  m_pByteMap = MB_NEW ByteArray(m_nCols,m_nRows,m_nSecs);

  createByteMap();

  if (m_pMapTex!=NULL)
    delete m_pMapTex;
  m_pMapTex = NULL;

}
#endif

#if 0
void DensityMap::sharpenMapPreview(double b_factor)
{
  /*if (m_pCCtxt!=NULL) {
    gfx::ComputeArray *pCA_in = m_pCCtxt->createArray();
    pCA_in->initWith(*m_pFloatMap);

    FloatArray map2(m_pFloatMap->cols(), m_pFloatMap->rows(), m_pFloatMap->secs());
    gfx::ComputeArray *pCA_out = m_pCCtxt->createArray();
    pCA_out->alloc(map2.size(), sizeof(FloatArray::value_type));

    {
      //launchTestKernel(pin, pout, nlen);
      launchTestKernel(pCA_in, pCA_out);
    }

    pCA_out->copyTo(map2);

    delete pCA_in;
    delete pCA_out;
  }*/

  const double vol = m_xtalInfo.volume();

  const double a = m_xtalInfo.a();
  const double b = m_xtalInfo.b();
  const double c = m_xtalInfo.c();

  const double alpha = qlib::toRadian( m_xtalInfo.alpha() );
  const double beta = qlib::toRadian( m_xtalInfo.beta() );
  const double gamma = qlib::toRadian( m_xtalInfo.gamma() );

  const double a_star = b*c*sin(alpha)/vol;
  const double b_star = c*a*sin(beta)/vol;
  const double c_star = a*b*sin(gamma)/vol;

  const double alph_star = acos( (cos(gamma)*cos(beta)-cos(alpha)) /(sin(beta)*sin(gamma)) );
  const double beta_star = acos( (cos(alpha)*cos(gamma)-cos(beta)) /(sin(gamma)*sin(alpha)) );
  const double gamm_star = acos( (cos(beta)*cos(alpha)-cos(gamma)) /(sin(alpha)*sin(beta)) );

  const double m00 = a_star*a_star;
  const double m11 = b_star*b_star;
  const double m22 = c_star*c_star;
  const double m01 = 2.0*a_star*b_star*cos(gamm_star);
  const double m02 = 2.0*a_star*c_star*cos(beta_star);
  const double m12 = 2.0*b_star*c_star*cos(alph_star);

  const int nh = m_pRecipAry->cols();
  const int nk = m_pRecipAry->rows();
  const int nl = m_pRecipAry->secs();

  CompArray hkldata( nh, nk, nl );

  int h, k, l;

  if (qlib::isNear4(b_factor, 0.0)) {
    for (l=0; l<nl; ++l)
      for (k=0; k<nk; ++k)
        for (h=0; h<nh; ++h)
          hkldata.at(h, k, l) = m_pRecipAry->at(h, k, l);
  }
  else {
    int i=0;
    for (l=0; l<nl; ++l)
      for (k=0; k<nk; ++k)
        for (h=0; h<nh; ++h) {
          float fp = abs( m_pRecipAry->at(h, k, l) );
          if (qlib::isNear4(fp, 0.0f)) {
            hkldata.at(h, k, l) = 0.0f;
          }
          else {
            // Hermitian case: h index is already restricted in 0-na/2 range
            float dh = h;
            //float dh = (h>nh/2) ? (h-nh) : h;
            
            float dk = (k>nk/2) ? (k-nk) : k;
            float dl = (l>nl/2) ? (l-nl) : l;

            float irs = dh*(dh*m00 + dk*m01 + dl*m02) + dk*(dk*m11 + dl*m12) + dl*(dl*m22);
            
            float scl = float(exp(-b_factor * irs * 0.25));
            
            hkldata.at(h, k, l) = m_pRecipAry->at(h, k, l) * scl;
          }
        }
  }

  FFTUtil fft;
  fft.doit(hkldata, *m_pFloatMap);

  MB_DPRINTLN("FFT OK");

  calcMapStats();

  createByteMap();

  updateByteMap();
}
#endif

/*
void DensityMap::calcHKLfromMap()
{
  if (m_pFloatMap==NULL) {
    LOG_DPRINTLN("Float map required for HKLList calculation.");
    MB_THROW(qlib::RuntimeException, "Cannot calc HKLList for map");
    return;
  }

  CompArray recip;
  
  recip.resize(m_nCols, m_nRows, m_nSecs);

  FFTUtil fft;
  fft.doit(*m_pFloatMap, recip);

  
  if (m_pHKLList!=NULL)
    delete m_pHKLList;
  m_pHKLList = MB_NEW HKLList();

  m_pHKLList->m_ci = m_xtalInfo;
}
*/

