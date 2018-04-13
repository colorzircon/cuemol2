// -*-Mode: C++;-*-
//
// Generate/Render mesh contours of ScalarObject (ver. 3)
//
// $Id$

#ifndef XTAL_MAP_MESH3_RENDERER_HPP_INCLUDED
#define XTAL_MAP_MESH3_RENDERER_HPP_INCLUDED

#include "xtal.hpp"
#include "Map3Renderer.hpp"

#include <qlib/Vector3I.hpp>
#include <qlib/Array.hpp>

class MapMesh3Renderer_wrap;

namespace xtal {

  using qlib::Vector3I;
  using qlib::Vector3F;

  using gfx::DisplayContext;
  using qsys::ScalarObject;
  class DensityMap;

  class MapMesh3Renderer : public Map3Renderer
  {
    MC_SCRIPTABLE;
    MC_CLONEABLE;

  private:
    typedef Map3Renderer super_t;
    friend class ::MapMesh3Renderer_wrap;

    typedef qlib::Array3D<qbyte> ByteMap;
    
    ///////////////////////////////////////////
    // properties

    /// Drawing line width (in pixel unit)
  private:
    double m_lw;

  public:
    void setLineWidth(double f) {
      m_lw = f;
      super_t::invalidateDisplayCache();
    }
    double getLineWidth() const { return m_lw; }

/*
  private:
    /// Internal buffer size (default: 100x100x100 points)
    int m_nBufSize;
  public:
    
    int getBufSize() const { return m_nBufSize; }
    void setBufSize(int nsize) {
      m_nMaxGrid = nsize;
    }
*/
  public:
    // for compatibility
    int getBufSize() const { return getMaxGrids(); }
    void setBufSize(int nsize) { setMaxGrids(nsize); }

    /// Use spherical extent
  private:
    bool m_bSphExt;

  public:
    bool isSphExt() const { return m_bSphExt; }
    void setSphExt(bool b) { m_bSphExt = b; }

    ///////////////////////////////////////////
    // work area

    /*
  protected:
    /// size of section array
    int m_nColCrs, m_nRowCrs, m_nSecCrs;
     */
    
  protected:
    qlib::Array3D<qbyte> m_maptmp;

    Vector3I m_texStPos;

  public:

    ///////////////////////////////////////////
    // constructors / destructor

    /// default constructor
    MapMesh3Renderer();

    /// destructor
    virtual ~MapMesh3Renderer();

    ///////////////////////////////////////////

    virtual const char *getTypeName() const;

    virtual void setSceneID(qlib::uid_t nid);

    virtual qlib::uid_t detachObj();

    ///////////////////////////////////////////

    virtual void render(DisplayContext *pdl);
    virtual void preRender(DisplayContext *pdc);
    virtual void postRender(DisplayContext *pdc) {}

    virtual bool isTransp() const { return true; }

    // /// Generate contour level lines
    // bool generate(ScalarObject *pMap, DensityMap *pXtal);

    ///////////////////////////////////////////////////////////////


    virtual void objectChanged(qsys::ObjectEvent &ev);

    ///////////////////////////////////////////////////////////////

  public:

  protected:

    unsigned char getContSec(unsigned int s0,
                             unsigned int s1,
                             unsigned int lv);

    unsigned char getMap(ScalarObject *pMap, int x, int y, int z) const
    {
      if (m_bPBC) {
        const int xx = (x+10000*m_mapSize.x())%m_mapSize.x();
        const int yy = (y+10000*m_mapSize.y())%m_mapSize.y();
        const int zz = (z+10000*m_mapSize.z())%m_mapSize.z();
        return pMap->atByte(xx,yy,zz);
      }
      else {
        if (pMap->isInBoundary(x,y,z))
          return pMap->atByte(x,y,z);
        else
          return 0;
      }
    }

    ///////////////////////////////////////////////////////////////


    Vector3I m_ivdel[12];

    int m_idel[12][3];
    
    int m_triTable[16][2];

    Vector3F calcVecCrs(const Vector3I &tpos, int iv0, float crs0, int ivbase);
    
    inline Vector3F calcVecCrs(int i, int j, int k, int iv0, float crs0, int ivbase){
      Vector3F v0(float(i + m_idel[ivbase+iv0][0]),
                  float(j + m_idel[ivbase+iv0][1]),
                  float(k + m_idel[ivbase+iv0][2]));
      
      Vector3F v1(float(i + m_idel[ivbase+(iv0+1)%4][0]),
                  float(j + m_idel[ivbase+(iv0+1)%4][1]),
                  float(k + m_idel[ivbase+(iv0+1)%4][2]));
      
      return v0 + (v1-v0).scale(crs0);
    }
    
  };

}

#endif