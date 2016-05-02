// -*-Mode: C++;-*-
//
//  Povray display context implementation
//
//  $Id: RendIntData.cpp,v 1.16 2011/04/17 10:56:39 rishitani Exp $

#include <common.h>

#include "RendIntData.hpp"
#include <qlib/PrintStream.hpp>
#include <qlib/BSPTree.hpp>
#include <gfx/SolidColor.hpp>
#include <gfx/Mesh.hpp>
#include <qsys/style/StyleMgr.hpp>
#include "FileDisplayContext.hpp"

using namespace render;
using gfx::DisplayContext;
using qsys::StyleMgr;

RendIntData::RendIntData(FileDisplayContext *pdc)
//     : m_pPovOut(NULL), m_pIncOut(NULL)
{
  m_pdc = pdc;
  m_dClipZ = -1.0;
//  m_mesh.m_pPar = this;

  m_pEgMesh = NULL;
  m_bSilhouette = false;
}

RendIntData::~RendIntData()
{
  std::for_each(m_lines.begin(), m_lines.end(), qlib::delete_ptr<Line *>());
  std::for_each(m_cylinders.begin(), m_cylinders.end(), qlib::delete_ptr<Cyl *>());
  std::for_each(m_spheres.begin(), m_spheres.end(), qlib::delete_ptr<Sph *>());
}

/////

//void RendIntData::start(OutStream *fp, OutStream *ifp, const char *name)
void RendIntData::start(const char *name)
{
  m_name = name;

  m_fUseTexBlend = true;
  //m_fUseTexBlend = false;
}

void RendIntData::meshStart()
{
  m_nMeshPivot = m_mesh.getVertexSize();
}

void RendIntData::meshEndTrigs()
{
  int nVerts = m_mesh.getVertexSize() - m_nMeshPivot;
  if (nVerts%3!=0) {
    LOG_DPRINTLN("RendIntData> Trig mesh: nVerts%3!=0 !!");
  }

  int nfmode = MFMOD_MESH;
  const int nPolyMode = m_pdc->getPolygonMode();
  if (nPolyMode == DisplayContext::POLY_FILL_NOEGLN)
    nfmode = MFMOD_OPNCYL;

  int nFaces = nVerts/3;
  int i;
  for (i=0; i<nFaces; i++) {
    m_mesh.addFace(m_nMeshPivot + i*3+0,
                   m_nMeshPivot + i*3+1,
                   m_nMeshPivot + i*3+2, nfmode);
  }
}

void RendIntData::meshEndTrigStrip()
{
  int nVerts = m_mesh.getVertexSize() - m_nMeshPivot;
  if (nVerts<3) {
    LOG_DPRINTLN("RendIntData> TrigStrip mesh: nVerts<3 !!");
  }

  int nfmode = MFMOD_MESH;
  const int nPolyMode = m_pdc->getPolygonMode();
  if (nPolyMode == DisplayContext::POLY_FILL_NOEGLN)
    nfmode = MFMOD_OPNCYL;

  int i;
  for (i=2; i<nVerts; i++) {
    if (i%2==0) {
      m_mesh.addFace(m_nMeshPivot + i-2,
                     m_nMeshPivot + i-1,
                     m_nMeshPivot + i,
                     nfmode);
    }
    else {
      m_mesh.addFace(m_nMeshPivot + i,
                     m_nMeshPivot + i-1,
                     m_nMeshPivot + i-2,
                     nfmode);
    }
  }
}

void RendIntData::meshEndFan()
{
  int nVerts = m_mesh.getVertexSize() - m_nMeshPivot;
  if (nVerts<3) {
    LOG_DPRINTLN("RendIntData> TrigFan mesh: nVerts<3 !!");
  }

  int nfmode = MFMOD_MESH;
  const int nPolyMode = m_pdc->getPolygonMode();
  if (nPolyMode == DisplayContext::POLY_FILL_NOEGLN)
    nfmode = MFMOD_OPNCYL;

  int i;
  for (i=2; i<nVerts; i++) {
    m_mesh.addFace(m_nMeshPivot + i-1,
                   m_nMeshPivot + i,
                   m_nMeshPivot,
                   nfmode);
  }
}

void RendIntData::mesh(const Matrix4D &mat, const gfx::Mesh &rmesh)
{
  int i;
  const int nverts = rmesh.getVertSize();
  const int nfaces = rmesh.getFaceSize();
  const int *pinds = rmesh.getFaces();

  const int ivstart = m_mesh.getVertexSize();
  const int nPolyMode = m_pdc->getPolygonMode();
  
  Vector4D v, n;
  ColorPtr col;
  const double lw = m_pdc->getLineWidth();

  if (nPolyMode == DisplayContext::POLY_FILL ||
      nPolyMode == DisplayContext::POLY_FILL_NOEGLN) {
    int nfmode = MFMOD_MESH;
    if (nPolyMode == DisplayContext::POLY_FILL_NOEGLN)
      nfmode = MFMOD_OPNCYL;

    for (i=0; i<nverts; ++i) {
      v = rmesh.getVertex(i);
      mat.xform3D(v);
      n = rmesh.getNormal(i);
      mat.xform4D(n);

      if (!rmesh.getCol(col, i))
        col = m_pdc->getCurrentColor();

      m_mesh.addVertex(v, n, convCol(col));
    }

    for (i=0; i<nfaces; ++i) {
      m_mesh.addFace(pinds[i*3+0] + ivstart,
                     pinds[i*3+1] + ivstart,
                     pinds[i*3+2] + ivstart, nfmode);
    }

  }
  else if (nPolyMode == DisplayContext::POLY_LINE) {
    // XXX: this impl draw each edge twice
    for (i=0; i<nfaces; ++i) {
      const int i0 = pinds[i*3+0];
      const int i1 = pinds[i*3+1];
      const int i2 = pinds[i*3+2];

      Vector4D v0 = rmesh.getVertex(i0);
      Vector4D v1 = rmesh.getVertex(i1);
      Vector4D v2 = rmesh.getVertex(i2);

      mat.xform3D(v0);
      mat.xform3D(v1);
      mat.xform3D(v2);

      if (!rmesh.getCol(col, i0))
        col = m_pdc->getCurrentColor();
      line(v0, v1, lw, col);

      if (!rmesh.getCol(col, i1))
        col = m_pdc->getCurrentColor();
      line(v1, v2, lw, col);

      if (!rmesh.getCol(col, i2))
        col = m_pdc->getCurrentColor();
      line(v2, v0, lw, col);
    }

  }
  else if (nPolyMode == DisplayContext::POLY_POINT) {

    for (i=0; i<nverts; ++i) {
      v = rmesh.getVertex(i);
      mat.xform3D(v);
      // n = rmesh.getNormal(i);
      // mat.xform4D(n);

      if (!rmesh.getCol(col, i))
        col = m_pdc->getCurrentColor();

      dot(v, lw, col);
      //sphere(v, lw*line_scale, 2, col);
      //m_mesh.addVertex(v, n, col);
    }

  }
}

/// Append line segment
void RendIntData::line(const Vector4D &v1, const Vector4D &v2, double width, const ColorPtr &col)
{
  Line *p = MB_NEW Line;
  p->v1 = v1;
  p->v2 = v2;
  if (col.isnull())
    p->col = convCol();
  else
    p->col = convCol(col);
  p->w = width;
  m_lines.push_back(p);
}

/// Append point
void RendIntData::dot(const Vector4D &v1, double w, const ColorPtr &col)
{
  Sph *p = MB_NEW Sph;
  p->v1 = v1;

  // p->col = convCol();
  if (col.isnull())
    p->col = convCol();
  else
    p->col = convCol(col);

  p->r = w;
  p->ndetail = 0;
  m_dots.push_back(p);
}

/// Append cylinder
void RendIntData::cylinder(const Vector4D &v1, const Vector4D &v2,
                           double w1, double w2, bool bcap,
                           int ndet, const Matrix4D *ptrf,
                           const ColorPtr &col)
{
  Cyl *p = MB_NEW Cyl;
  p->v1 = v1;
  p->v2 = v2;

  if (col.isnull())
    p->col = convCol();
  else
    p->col = convCol(col);

  p->w1 = w1;
  p->w2 = w2;
  p->ndetail = ndet;
  p->bcap = bcap;

  if (ptrf==NULL)
    p->pTransf = NULL;
  else
    p->pTransf = MB_NEW Matrix4D(*ptrf);

  m_cylinders.push_back(p);
}

void RendIntData::sphere(const Vector4D &v1, double w, int ndet, const ColorPtr &col)
{
  Sph *p = MB_NEW Sph;
  p->v1 = v1;

  // p->col = convCol();
  if (col.isnull())
    p->col = convCol();
  else
    p->col = convCol(col);

  p->r = w;
  p->ndetail = ndet;
  m_spheres.push_back(p);
}

/// convert LColor to CLUT entry
RendIntData::ColIndex RendIntData::convCol()
{
  const ColorPtr &pcol = m_pdc->getCurrentColor();
  LString defmtr = m_pdc->getMaterial();
  return m_clut.newColor(pcol, defmtr);
}

/// Convert color to internal representation (2)
RendIntData::ColIndex RendIntData::convCol(const ColorPtr &pcol)
{
  LString defmtr = m_pdc->getMaterial();
  return m_clut.newColor(pcol, defmtr);
}

// Create and return the clipped mesh object by m_dClipZ
Mesh *RendIntData::calcMeshClip()
{
  int i, j;
  int nverts = m_mesh.getVertexSize();
  int nfaces = m_mesh.getFaceSize();

  std::vector<int> vidmap(nverts);
  
  Mesh *pMesh2 = MB_NEW Mesh();
//  pMesh2->m_pPar = this;

  std::vector<MeshVert *> verts(nverts);
  bool bNone = true;
  i=0;
  BOOST_FOREACH (MeshVert *p, m_mesh.m_verts) {
    verts[i] = p;
    if (p->v.z()>=m_dClipZ) {
      // clipped
      vidmap[i] = -1;
      bNone = false;
      ++i;
      continue;
    }

    vidmap[i] = 1;
    ++i;
  }  

  if (bNone) {
    delete pMesh2;
    return NULL;
  }

  i=0; j=0;
  BOOST_FOREACH (MeshVert *p, m_mesh.m_verts) {
    if (vidmap[i]<0) {
      ++i;
      continue;
    }
    vidmap[i] = j;
    pMesh2->copyVertex(p);
    ++j;
    ++i;
  }  

  std::deque<MeshFace>::iterator iter2 = m_mesh.m_faces.begin();
  std::deque<MeshFace>::iterator iend2 = m_mesh.m_faces.end();
  int iv[3];
  int jv[3], nfmode;
  bool b[3];
  for (i=0; iter2!=iend2; iter2++, i++) {
    iv[0] = iter2->iv1;
    iv[1] = iter2->iv2;
    iv[2] = iter2->iv3;
    nfmode = iter2->nmode;

    for (j=0; j<3; ++j)
      jv[j] = vidmap[iv[j]];
    
    for (j=0; j<3; ++j)
      b[j] = (jv[j]<0);
    
    if (b[0] && b[1] && b[2]) {
      // outside clip (remove)
      continue;
    }
    else if (!b[0] && !b[1] && !b[2]) {
      // inside clip
      pMesh2->addFace(jv[0], jv[1], jv[2], nfmode);
      continue;
    }
    
    for (j=0; j<3; ++j) {

      MeshVert *pv1 = verts[ iv[j] ];
      MeshVert *pv2 = verts[ iv[(j+1)%3] ];
      MeshVert *pv3 = verts[ iv[(j+2)%3] ];
      
      // check single clipped triangles
      if (!b[j] && b[(j+1)%3] && b[(j+2)%3]) {
        int jin = jv[j];
        MeshVert *pv1 = verts[ iv[j] ];
        MeshVert *pv2 = verts[ iv[(j+1)%3] ];
        MeshVert *pv3 = verts[ iv[(j+2)%3] ];
        int inv2 = pMesh2->addVertex( cutEdge(pv1, pv2) );
        int inv3 = pMesh2->addVertex( cutEdge(pv1, pv3) );
        pMesh2->addFace(jin, inv2, inv3, nfmode);
        break;
      }

      // check double clipped triangles
      // TO DO: select convex-hull triangles
      if (!b[j] && !b[(j+1)%3] && b[(j+2)%3]) {
        int jin0 = jv[j];
        int jin1 = jv[(j+1)%3];
        int ed1 = pMesh2->addVertex( cutEdge(pv3, pv1) );
        int ed2 = pMesh2->addVertex( cutEdge(pv3, pv2) );

        pMesh2->addFace(jin0, jin1, ed1, nfmode);
        pMesh2->addFace(ed2, ed1, jin1, nfmode);
        break;
      }
      
    }
  }

  return pMesh2;
}

MeshVert *RendIntData::cutEdge(MeshVert *pv1, MeshVert *pv2)
{
  const Vector4D &v1 = pv1->v;
  const Vector4D &v2 = pv2->v;
  
  const Vector4D &n1 = pv1->n;
  const Vector4D &n2 = pv2->n;
  
  MeshVert *pNew = MB_NEW MeshVert;
  const double t = (m_dClipZ-v1.z())/(v2.z()-v1.z());
  pNew->v = v2.scale(t) + v1.scale(1.0-t);
  
  Vector4D norm = n2.scale(t) + n1.scale(1.0-t);
  pNew->n = norm.normalize();
  
  pNew->c = pv1->c;
  return pNew;
}

/// convert dot to sphere
void RendIntData::convDots()
{
  if (m_dots.size()<=0)
    return;

  const double line_scale = m_pdc->getLineScale();

  BOOST_FOREACH (Sph *p, m_dots) {
    p->r *= line_scale;
    p->ndetail = 4;
    m_spheres.push_back(p);
  }

  m_dots.erase(m_dots.begin(), m_dots.end());
}

/// convert line to cylinder
void RendIntData::convLines()
{
  if (m_lines.size()<=0)
    return;
  const double lsc = m_pdc->getLineScale();
  BOOST_FOREACH (RendIntData::Line *p, m_lines) {
    double w = p->w * lsc;

    //cylinder(v1, v2, w, w, 4, NULL, p->);
    Cyl *pc = MB_NEW Cyl;
    pc->v1 = p->v1;
    pc->v2 = p->v2;
    pc->col = p->col;
    pc->w1 = pc->w2 = w;
    pc->ndetail = 4;
    pc->pTransf = NULL;
    m_cylinders.push_back(pc);

    delete p;
  }

  m_lines.erase(m_lines.begin(), m_lines.end());
}

/// convert sphere to mesh
void RendIntData::convSpheres()
{
  if (m_spheres.size()<=0)
    return;

  BOOST_FOREACH (Sph *p, m_spheres) {
    convSphere(p);
    delete p;
  }

  m_spheres.erase(m_spheres.begin(), m_spheres.end());
  return;
}

// convert single sphere to mesh
void RendIntData::convSphere(Sph *pSph)
{
  const Vector4D v1 = pSph->v1;
  const double rad = pSph->r;
  ColIndex col = pSph->col;
  const double dmax = (M_PI*rad)/double(pSph->ndetail+1);

  const int ivstart = m_mesh.getVertexSize();

  int i, j;
  int nLat = pSph->ndetail+1;

  // detail in longitude direction is automatically determined by stack radius
  int nLng;

  MB_DPRINTLN("RendIntData::convSphere v1=(%f,%f,%f) r=%f",
              pSph->v1.x(), pSph->v1.y(), pSph->v1.z(), pSph->r);
  MB_DPRINTLN("sphere R=%f, nLat=%d (%f)", rad, nLat, rad*M_PI/dmax);

  int **ppindx = new int *[nLat+1];

  // generate verteces
  for (i=0; i<=nLat; ++i) {
    int ind;
    //std::list<int> ilst;

    if (i==0) {
      ind = m_mesh.addVertex(Vector4D(0, 0, rad) + v1,
                             Vector4D(0, 0, 1),
                             col);
      //ind = putVert(Vector4D(0, 0, rad) + v1);

      ppindx[i] = new int[1];
      ppindx[i][0] = ind;
    }
    else if (i==nLat) {
      ind = m_mesh.addVertex(Vector4D(0, 0, -rad) + v1,
                             Vector4D(0, 0, -1),
                             col);
      //ind = putVert(Vector4D(0, 0, -rad) + v1);
      //ilst.push_back(ind);

      ppindx[i] = new int[1];
      ppindx[i][0] = ind;
    }
    else {
      Vector4D vec, norm;
      const double th = double(i)*M_PI/double(nLat);
      const double ri = rad*::sin(th);
      vec.z()  = rad*::cos(th);
      nLng = (int) ::ceil(ri*M_PI*2.0/dmax);
      ppindx[i] = new int[nLng+2];
      ppindx[i][0] = nLng;
      const double start_phi = double(i%2) * 3.0 / nLng;
      //MB_DPRINTLN("Lat: %d start phi=%f", i, start_phi);
      for (j=0; j<nLng; ++j) {
        double ph = double(j)*M_PI*2.0/double(nLng) + start_phi;
        vec.x() = ri*::cos(ph);
        vec.y() = ri*::sin(ph);
        norm = vec.normalize();
        ind = m_mesh.addVertex(vec + v1, norm, col);
        //ind = putVert(vec + v1);

        ppindx[i][j+1] = ind;
      }
      ppindx[i][j+1] = ppindx[i][1];
    }
  } // for (i)

  // build faces from verteces
  for (i=0; i<nLat; ++i) {
    if (i==0) {
      int ipiv = ppindx[0][0];
      int nLng = ppindx[1][0];
      for (j=0; j<nLng; ++j) {
        m_mesh.addFace(ipiv, ppindx[1][j+1], ppindx[1][j+2],
		       2);
      }
    }
    else if (i==nLat-1) {
      int ipiv = ppindx[nLat][0];
      int nLng = ppindx[nLat-1][0];
      for (j=0; j<nLng; ++j) {
        m_mesh.addFace(ppindx[nLat-1][j+2], ppindx[nLat-1][j+1], ipiv,
		       2);
      }
    }
    else /*if (i==2)*/ {

      int j = 0, k = 0;
      int bJ;

      int jmax = ppindx[i][0];
      int *piJ = &(ppindx[i][1]);

      int kmax = ppindx[i+1][0];
      int *piK = &(ppindx[i+1][1]);

      //      double am1, am2;
      while (j+1<=jmax || k+1<=kmax) {
        if (j+1>jmax) bJ = 0;
        else if (k+1>kmax) bJ = 1;
        else bJ = selectTrig(piJ[j], piK[k], piJ[j+1], piK[k+1]);

        if (bJ==1) {
          m_mesh.addFace(piJ[j], piK[k], piJ[j+1],
			 2);
          ++j;
        }
        else /*if (bJ==0)*/ {
          m_mesh.addFace(piJ[j], piK[k], piK[k+1],
			 2);
          ++k;
        }
      } // while

    }
  } // for (i)

  for (i=0; i<=nLat; ++i)
    delete [] ppindx[i];
  delete [] ppindx;
}

static
inline Vector4D makenorm(const Vector4D &pos1,
                         const Vector4D &pos2,
                         const Vector4D &pos3)
{
  const Vector4D v12 = pos2 - pos1;
  const Vector4D v23 = pos3 - pos2;
  Vector4D vn = v12.cross(v23);
  const double dnorm = vn.length();
  // if (dnorm<dtol) {
  //   throw error!!
  // }
  vn /= dnorm;
  return vn;
}

int RendIntData::selectTrig(int j, int k, int j1, int k1)
{
  const Vector4D &vj =  m_mesh.m_verts[j]->v;
  const Vector4D &vk =  m_mesh.m_verts[k]->v;
  const Vector4D &vj1 = m_mesh.m_verts[j1]->v;
  const Vector4D &vk1 = m_mesh.m_verts[k1]->v;

  Vector4D nj1 = makenorm(vj, vk, vj1);
  Vector4D nk1 = makenorm(vj, vk, vk1);
  
  double detj = nj1.dot(vk1-vk);
  double detk = nk1.dot(vj1-vj);

  if (detj<0 && detk>=0)
    return 1; // select j1

  if (detj>=0 && detk<0)
    return 0; // select k1

  MB_DPRINTLN("SelectTrig warning; (%d,%d,%d,%d) detj=%f, detk=%f", j, k, j1, k1, detj, detk);
  return 2;
  /*
  double am1 = angmax(m_verts[j].v3d(), m_verts[k].v3d(), m_verts[j1].v3d());
  double am2 = angmax(m_verts[j].v3d(), m_verts[k].v3d(), m_verts[k1].v3d());
  return (am1<am2);
   */
}

/////////////////////////////

/// convert cylinders to mesh
void RendIntData::convCylinders()
{
  if (m_cylinders.size()<=0)
    return;

  BOOST_FOREACH (Cyl *p, m_cylinders) {
    convCyl(p);
    delete p;
  }

  m_cylinders.erase(m_cylinders.begin(), m_cylinders.end());
  return;
}

/// convert cylinder to mesh
void RendIntData::convCyl(Cyl *pCyl)
{
  Vector4D cylv1(pCyl->v1), cylv2(pCyl->v2);
  ColIndex col = pCyl->col;

  MB_DPRINTLN("=== RendIntData::convCyl ===");

  Vector4D nn = cylv1 - cylv2;
  double len = nn.length();
  if (len<=F_EPS4) {
    // ignore a degenerated cylinder
    return;
  }
  
  nn = cylv1 - cylv2;
  len = nn.length();
  nn = nn.scale(1.0/len);
  
  MB_DPRINTLN("nn: (%f,%f,%f)", nn.x(), nn.y(), nn.z());
  MB_DPRINTLN("v1: (%f,%f,%f)", cylv1.x(), cylv1.y(), cylv1.z());
  MB_DPRINTLN("v2: (%f,%f,%f)", cylv2.x(), cylv2.y(), cylv2.z());

  const Vector4D ex(1,0,0), ey(0,1,0), ez(0,0,1);
  Vector4D n1, n2;
  if (qlib::abs(nn.dot(ex)) < 0.9) {
    n1 = nn.cross(ex);
  }
  else if (qlib::abs(nn.dot(ey)) < 0.9) {
    n1 = nn.cross(ey);
  }
  else if (qlib::abs(nn.dot(ez)) < 0.9) {
    n1 = nn.cross(ez);
  }
  else {
    LOG_DPRINTLN("ConvCYL fatal error !!");
    return;
  }
  n1 = n1.normalize();
  Matrix4D mat = Matrix4D::makeRotMat(nn, n1);

  //
  // generate verteces
  //

  int i, j;
  double th;
  const double w2 = pCyl->w1;
  const double w1 = pCyl->w2;
  const bool bcap = pCyl->bcap;
  
  const int NDIVR = 2*(pCyl->ndetail+1);
  const double dth = (M_PI*2.0)/NDIVR;
  
  //const int NDIVV = qlib::max(2, (int) ::floor(len/((pCyl->w1)*dth)));
  const int NDIVV = 2;
  const double dw = (w2-w1)/double(NDIVV-1);
  const double dlen = len/double(NDIVV-1);
  
  MB_DPRINTLN("cyl ndiv r,v =(%d, %d)", NDIVR, NDIVV);
  
  Matrix4D xfm;
  if (pCyl->pTransf!=NULL)
    xfm = *(pCyl->pTransf);
  
  xfm.matprod( Matrix4D::makeTransMat(cylv2) );
  xfm.matprod( mat );

  // bottom terminal vertex (at the center of the disk)
  const int ivbot = 
    m_mesh.addVertex(Vector4D(0, 0, 0),
                     Vector4D(0, 0, -1),
                     col, xfm);
  
  for (j=0; j<NDIVV; ++j) {
    const double ww = w1 + dw*double(j);
    const double zz = dlen*double(j);
    for (th=0.0, i=0; i<NDIVR; ++i, th += dth) {
      const double costh = ::cos(th);
      const double sinth = ::sin(th);
      const double xx = ww * costh;
      const double yy = ww * sinth;
      // putVert(Vector4D(xx, yy, zz));
      m_mesh.addVertex(Vector4D(xx, yy, zz),
                       Vector4D(costh, sinth, 0),
                       col, xfm);
    }
  }
  
  // top terminal vertex (at the center of the disk)
  const int ivtop = 
    m_mesh.addVertex(Vector4D(0,0,len),
                     Vector4D(0, 0, 1),
                     col, xfm);

  //MB_DPRINTLN("vbot: (%f,%f,%f)", m_verts[ivbot].x(), m_verts[ivbot].y(), m_verts[ivbot].z());
  //MB_DPRINTLN("vtop: (%f,%f,%f)", m_verts[ivtop].x(), m_verts[ivtop].y(), m_verts[ivtop].z());

  //
  // connect verteces & make faces
  //

  int nfmode = bcap ? MFMOD_CLSCYL : MFMOD_OPNCYL;

  // bottom disk
  if (bcap) {
    for (i=0; i<=NDIVR; ++i) {
      const int ii = i%NDIVR;
      const int jj = (i+1)%NDIVR;
      m_mesh.addFace(ivbot,
                     ivbot + 1 + jj,
                     ivbot + 1 + ii,
		     nfmode);
    }
  }
  
  // cylinder body
  for (j=0; j<NDIVV-1; ++j) {
    const int u = 1 + j*NDIVR;
    const int v = 1 + (j+1)*NDIVR;
    for (i=0; i<NDIVR; ++i) {
      const int ii = i%NDIVR;
      const int jj = (i+1)%NDIVR;
      m_mesh.addFace(ivbot + u + ii,
                     ivbot + u + jj,
                     ivbot + v + jj,
                     nfmode);
      m_mesh.addFace(ivbot + u + ii,
                     ivbot + v + jj,
                     ivbot + v + ii,
                     nfmode);
    }
  }


  // top disk
  if (bcap) {
    for (i=0; i<=NDIVR; ++i) {
      const int ii = i%NDIVR;
      const int jj = (i+1)%NDIVR;
      
      m_mesh.addFace(ivtop,
                     ivbot + 1 + (NDIVV-1)*NDIVR + ii,
                     ivbot + 1 + (NDIVV-1)*NDIVR + jj,
		     nfmode);
    }
  }

}

void RendIntData::end()
{
}

// nmode==0: vertex compare
// nmode==1: vertex&norm compare
// nmode==2: vertex&norm&color compare
bool RendIntData::isVertNear(const MeshVert &p1,
                             const MeshVert &p2,
                             int nmode)
{
  if (!(qlib::isNear4(p1.v.x(), p2.v.x()) &&
        qlib::isNear4(p1.v.y(), p2.v.y()) &&
        qlib::isNear4(p1.v.z(), p2.v.z())))
    return false;
  
  if (nmode==0)
    return true;
  
  if (!(qlib::isNear4(p1.n.x(), p2.n.x()) &&
        qlib::isNear4(p1.n.y(), p2.n.y()) &&
        qlib::isNear4(p1.n.z(), p2.n.z())))
    return false;

  if (nmode==1)
    return true;

  if (! p1.c.equals(p2.c) )
    return false;
  
  return true;
}

namespace {
  inline double triArea(const Vector4D &v1, const Vector4D &v2, const Vector4D &v3)
  {
    return ( (v2-v1).cross(v3-v1) ).length();
  }
}

/// Create a simplified mesh object by eliminating the degenerated points
/// nmode==0: vertex compare
/// nmode==1: vertex&norm compare
/// nmode==2: vertex&norm&color compare
Mesh *RendIntData::simplifyMesh(Mesh *pMesh, int nmode /*=2*/)
{
  int i, j;

  const int nverts = pMesh->getVertexSize();
  const int nfaces = pMesh->getFaceSize();

  // convert mesh list to array
  std::vector<MeshVert*> vertvec(nverts);
  std::copy(pMesh->m_verts.begin(), pMesh->m_verts.end(), vertvec.begin());

  // make positional alias map
  qlib::BSPTree<int> bsp;
  bsp.alloc(nverts);
  for (i=0; i<nverts; ++i) {
    vertvec[i]->v.w() = 0.0;
    bsp.setAt(i, vertvec[i]->v, i);
  }
  bsp.build();

  // std::vector<int> valias(nverts);
  std::vector<int> vuid(nverts);

  for (i=0; i<nverts; ++i) {
    // valias[i] = -1;
    vuid[i] = -1;
  }
  
  Mesh *pMesh2 = MB_NEW Mesh();

  std::vector<int> resvec;
  const double eps = 1.0e-4;
  int iuniq=0;
  for (i=0; i<nverts; ++i) {
    //MB_DPRINTLN("bsp vtx%d", i);
    if (vuid[i]>=0) {
      // already has alias vert
      continue;
    }

    // i is uniq/representative vert
    iuniq = pMesh2->copyVertex(vertvec[i]);
    vuid[i] = iuniq;

    Vector4D v = vertvec[i]->v;
    int nfound = bsp.findAround(v, eps, resvec);
    //MB_DPRINTLN("   -> col=%d", nfound);

    for (j=0; j<nfound; ++j) {
      const int jv = resvec[j];
      if (jv==i) {
        continue; // identical vert
      }

      if (!isVertNear(*vertvec[jv], *vertvec[i], nmode)) {
        //writePointMark(ips, vertvec[jv]->v, 2);
        //writeLineMark(ips, vertvec[jv]->v,vertvec[jv]->v + (vertvec[jv]->n).scale(0.1), 1);
        MB_DPRINTLN("Is not near (norm): %d <--> %d", jv, i);
        continue;
      }

      //Vector4D va = vertvec[jv]->v;
      //Vector4D vo = vertvec[i]->v;
      //MB_DPRINTLN("alias %d %s --> %d %s",
      //jv, va.toString().c_str(),
      //i, vo.toString().c_str());

      if (vuid[jv]>=0)
        continue; // already has alias vert

      vuid[jv] = iuniq;
    }
  }
  LOG_DPRINTLN("Build aliasmap done, nvert=%d, uniqvert=%d", nverts, pMesh2->getVertexSize());

  Mesh::FCIter iter2 = pMesh->m_faces.begin();
  Mesh::FCIter iend2 = pMesh->m_faces.end();

  for (int fid=0; iter2!=iend2; iter2++, fid++) {
    const int io1 = iter2->iv1;
    const int io2 = iter2->iv2;
    const int io3 = iter2->iv3;

    const int i1 = vuid[ io1 ];
    const int i2 = vuid[ io2 ];
    const int i3 = vuid[ io3 ];

    MB_ASSERT(i1>=0 && i2>=0 && i3>=0);

    if (i1==i2 || i2==i3 ||i3==i1) {
      MB_DPRINTLN("ID Degen: (%d,%d,%d)", i1, i2, i3);
      continue;
    }
    
    const double S = triArea(vertvec[io1]->v,
                             vertvec[io2]->v,
                             vertvec[io3]->v);
    if (S<eps) {
      MB_DPRINTLN("TriArea Degen: (%d,%d,%d)", i1, i2, i3);
      continue;
    }

    pMesh2->addFace(i1, i2, i3, iter2->nmode);
  }

  return pMesh2;
}

namespace {
  bool checkSilEdge(const Vector4D &vwvec, const Vector4D &n1, const Vector4D &n2, double norm_limit)
  {
    double dot1 = vwvec.dot(n1);
    double dot2 = vwvec.dot(n2);
    if (dot1*dot2<0)
      return true;

    double ang = ::acos( n1.dot(n2) );
    if (std::abs(ang)>norm_limit)
      return true;

    return false;
  }

  inline Vector4D calcNorm(const Vector4D &v1, const Vector4D &v2, const Vector4D &v3)
  {
    Vector4D tmp = (v2-v1).cross(v3-v1);
    return tmp.normalize();
  }
  
}

void RendIntData::calcSilEdgeLines(double dViewDist, double dnangl)
{
  // set view position
  m_dViewDist = dViewDist;

  // simplify degenerated verteces
  // nmode==0 (-->only account for verteces)
  m_pEgMesh = simplifyMesh(&m_mesh, 0);

  const int nverts = m_pEgMesh->getVertexSize();
  const int nfaces = m_pEgMesh->getFaceSize();

  // convert vertex list to array
  m_vertvec.resize(nverts);
  std::copy(m_pEgMesh->m_verts.begin(), m_pEgMesh->m_verts.end(), m_vertvec.begin());

  // convert face list to array (and calc face norms)
  // make edge set
  SEEdgeSet eset;
  m_facevec.resize(nfaces);

  {
    Mesh::FCIter iter2 = m_pEgMesh->m_faces.begin();
    Mesh::FCIter iend2 = m_pEgMesh->m_faces.end();
    SEFace ff;
    for (int fid=0; iter2!=iend2; iter2++, fid++) {
      int ia1 = iter2->iv1;
      int ia2 = iter2->iv2;
      int ia3 = iter2->iv3;

      ff.nmode = iter2->nmode;
      ff.iv1 = ia1;
      ff.iv2 = ia2;
      ff.iv3 = ia3;
      ff.n = calcNorm(m_vertvec[ia1]->v,m_vertvec[ia2]->v,m_vertvec[ia3]->v);

      m_facevec[fid] = ff;
      // MB_DPRINTLN(" (%d,%d,%d)", ia1, ia2, ia3);

      eset.insertEdge(ia1, ia2, fid);
      eset.insertEdge(ia2, ia3, fid);
      eset.insertEdge(ia3, ia1, fid);
    }
  }

  MB_DPRINTLN("Build triedges done");

  // current view point
  Vector4D vcam(0,0,m_dViewDist);

  // Limit angle of normals for creating crease lines
  // const double dnangl = m_dCreaseLimit;

  Vector4D v1, v2, n1, n2;

  // Select crease & silhouette lines (m_silEdges) from mesh edges (eset)
  BOOST_FOREACH (const SEEdge &elem, eset) {
    // MB_DPRINTLN("edge <%d, %d> f=(%d,%d)", elem.iv1, elem.iv2, elem.if1, elem.if2);

    v1 = m_vertvec[elem.iv1]->v;
    v2 = m_vertvec[elem.iv2]->v;

    if (elem.if1<0 || elem.if2<0) {
      // edge is ridge
      MB_DPRINTLN("Ridge <%d, %d> f=(%d,%d)", elem.iv1, elem.iv2, elem.if1, elem.if2);
      int nmode = MFMOD_MESH;
      if (elem.if1>=0)
        nmode = m_facevec[elem.if1].nmode;
      else if (elem.if2>=0)
        nmode = m_facevec[elem.if2].nmode;

      // nmode==1 --> ridge triangle without silhouette line
      if (nmode!=MFMOD_OPNCYL) {
        m_silEdges.insert(elem);
      }
    }
    else if (checkSilEdge(v1-vcam, m_facevec[elem.if1].n, m_facevec[elem.if2].n, dnangl) ||
             checkSilEdge(v2-vcam, m_facevec[elem.if1].n, m_facevec[elem.if2].n, dnangl)) {
      // edge is silhouette/edge line
      m_silEdges.insert(elem);
    }

  }

  // collect corner points
  std::vector<int> idmap(nverts, -1);
  std::deque<int> secpts;
  int idnew = 0;
  //  std::set<int> secpts;
  BOOST_FOREACH (const SEEdge &elem, m_silEdges) {
    if (idmap[elem.iv1]<0) {
      idmap[elem.iv1] = idnew;
      ++idnew;
      secpts.push_back(elem.iv1);
    }
    if (idmap[elem.iv2]<0) {
      idmap[elem.iv2] = idnew;
      ++idnew;
      secpts.push_back(elem.iv2);
    }
    //secpts.insert(elem.iv1);
    //secpts.insert(elem.iv2);
  }

  // calculate the corner point array index
  BOOST_FOREACH (const SEEdge &elem, m_silEdges) {
    int newiv1 = idmap[elem.iv1];
    MB_ASSERT(elem.iv1==secpts[newiv1]);
    int newiv2 = idmap[elem.iv2];
    MB_ASSERT(elem.iv2==secpts[newiv2]);
    elem.setCpIndex(newiv1, newiv2);
  }

  int ncorner = secpts.size();
  m_secpts.resize(ncorner);
  for (int i=0; i<ncorner; ++i) {
    m_secpts[i].iv = secpts[i];
    m_secpts[i].bvis = false;
  }

  MB_DPRINTLN("Silhouette extraction done. (%d edges, %d corners)", m_silEdges.size(), ncorner);
}


#define CGAL_LIB_DIAGNOSTIC
#define CGAL_HAS_NO_THREADS
#define CGAL_DISABLE_ROUNDING_MATH_CHECK
#define CGAL_INTERSECTION_VERSION 1
#include <CGAL/basic.h>

#include <CGAL/AABB_tree.h> // must be inserted before kernel
#include <CGAL/AABB_traits.h>
#include <CGAL/AABB_triangle_primitive.h>

#include <CGAL/Simple_cartesian.h>

namespace {

  typedef CGAL::Simple_cartesian<double> K;
  //typedef K::Point_3 Point;
  class Point : public K::Point_3
  {
    typedef K::Point_3 super_t;
  public:
    Point() : super_t() {}
    Point(const Vector4D &av) : super_t(av.x(), av.y(), av.z()) {}
    int id;
  };
  typedef K::Plane_3 Plane;
  typedef K::Vector_3 Vector;
  typedef K::Segment_3 Segment;
  
  //typedef K::Triangle_3 Triangle;
  class Triangle : public K::Triangle_3
  {
    typedef K::Triangle_3 super_t;
  public:
    Triangle() : super_t() {}
    
    Triangle(const Vector4D &v1,
             const Vector4D &v2,
             const Vector4D &v3,
             int aiv1, int aiv2, int aiv3)
         : super_t(Point(v1),Point(v2),Point(v3)),
           iv1(aiv1), iv2(aiv2), iv3(aiv3) {}
    
    int iv1, iv2, iv3;
  };
  
  typedef std::vector<Triangle> FaceVec;
  typedef FaceVec::iterator FaceVecIterator;
  typedef CGAL::AABB_triangle_primitive<K, FaceVecIterator> Primitive;
  
  typedef CGAL::AABB_traits<K, Primitive> AABB_triangle_traits;
  typedef CGAL::AABB_tree<AABB_triangle_traits> Tree;
  typedef boost::optional< Tree::Object_and_primitive_id > Segment_intrsec;
  typedef std::list<Tree::Object_and_primitive_id>  IntrsecList;
  
  //////////

  bool isVertVisible(Tree &tree,
		    const Vector4D &vcam,
		    const Vector4D &vert,
		    int iv)
  {
    // check vert visibility from vcam
    K::Point_3 pcam(vcam.x(), vcam.y(), vcam.z());
    K::Point_3 pvert(vert.x(), vert.y(), vert.z());
    
    Segment segq(pcam, pvert);
    
    IntrsecList ilst;
    tree.all_intersections(segq, std::back_inserter(ilst));
    
    BOOST_FOREACH (const IntrsecList::value_type &isec, ilst) {
      FaceVecIterator fiter = isec.second;
      if (iv == fiter->iv1 ||
          iv == fiter->iv2 ||
          iv == fiter->iv3)
        continue;
      return false;
    }
    
    return true;
  }

  bool isVertSilVisible(Tree &tree,
                        const Vector4D &vcam,
                        const Vector4D &vert,
                        int iv)
  {
    // check vert visibility from vcam
    K::Point_3 pcam(vcam.x(), vcam.y(), vcam.z());
    K::Point_3 pvert(vert.x(), vert.y(), vert.z());
    
    K::Ray_3 rayq(pcam, pvert);
    
    IntrsecList ilst;
    tree.all_intersections(rayq, std::back_inserter(ilst));
    
    BOOST_FOREACH (const IntrsecList::value_type &isec, ilst) {
      FaceVecIterator fiter = isec.second;
      if (iv == fiter->iv1 ||
          iv == fiter->iv2 ||
          iv == fiter->iv3)
        continue;
      return false;
    }
    
    return true;
  }

  bool contains_id(int iv1, int iv2,
                   int if1, int if2, int if3)
  {
    if (iv1==if1 ||
        iv1==if2 ||
        iv1==if3)
      return true;

    if (iv2==if1 ||
        iv2==if2 ||
        iv2==if3)
      return true;

    return false;
  }

}

void RendIntData::buildVertVisList()
{
  Vector4D vcam(0,0,m_dViewDist);

  MeshVert *pv;

  Tree &tree = *static_cast<Tree *>(m_pTree);

  int nc = m_secpts.size();
  for (int i=0; i<nc; ++i) {
    const int iv = m_secpts[i].iv;
    pv = m_vertvec[iv];
    m_secpts[i].bvis = isVertVisible(tree, vcam, pv->v, iv);

    m_secpts[i].nshow = 0;

    m_secpts[i].bsil = false;
    if (m_bSilhouette && m_secpts[i].bvis) {
      m_secpts[i].bsil = isVertSilVisible(tree, vcam, pv->v, iv);
    }

  }

  MB_DPRINTLN("Vertex visibility list generated.");
}



void RendIntData::calcEdgeIntrsec()
{
  int i;
  MeshVert *pv1, *pv2, *pv3;
  
  FaceVec faces;
  int nverts = m_vertvec.size();
  int nfaces = m_facevec.size();
  
  for (i=0; i<nfaces; ++i) {
    const SEFace &ff = m_facevec[i];
    if (ff.iv1<0 || ff.iv2<0 || ff.iv3<0)
      continue;

    // only handle cyl and sph meshes
    if (ff.nmode==MFMOD_MESH)
      continue;

    pv1 = m_vertvec[ff.iv1];
    pv2 = m_vertvec[ff.iv2];
    pv3 = m_vertvec[ff.iv3];
    
    faces.push_back(Triangle(pv1->v,pv2->v,pv3->v,
                             ff.iv1, ff.iv2, ff.iv3));
  }

  // find occluded verteces by mesh faces
  // --> write only the visible edges
  MB_DPRINTLN("AABB Tree constructing...");
  // Tree tree(faces.begin(), faces.end());
  Tree *ptree = new Tree(faces.begin(), faces.end());
  m_pTree = ptree;
  MB_DPRINTLN("Done.");
  
  buildVertVisList();
  
  BOOST_FOREACH (const SEEdge &elem, m_silEdges) {
    pv1 = m_vertvec[elem.iv1];
    pv2 = m_vertvec[elem.iv2];

    int nmode = MFMOD_MESH;
    if (elem.if1>0)
      nmode = m_facevec[elem.if1].nmode;
    else if (elem.if2>0)
      nmode = m_facevec[elem.if2].nmode;

    if (nmode==MFMOD_MESH ||
        nmode==MFMOD_OPNCYL ||
        nmode==MFMOD_CLSCYL) {
      SEEdge &welem = const_cast<SEEdge &>(elem);
      welem.bForceShow = true;
      continue;
    }

    const int icp1 = elem.icp1;
    const int icp2 = elem.icp2;

    if (!m_secpts[icp1].bvis &&
        !m_secpts[icp2].bvis) {
      // hidden edge line
      continue;
    }

    Segment segq(Point(pv1->v), Point(pv2->v));

    IntrsecList ilst;
    ptree->all_intersections(segq, std::back_inserter(ilst));
    
    if (ilst.empty())
      continue;

    Vector4D v12 = pv2->v - pv1->v;
    double l12 = v12.length();
    K::Point_3 psec;
    BOOST_FOREACH (const IntrsecList::value_type &isec, ilst) {
      FaceVecIterator fiter = isec.second;

      if (contains_id(elem.iv1, elem.iv2,
                      fiter->iv1, fiter->iv2, fiter->iv3))
        continue;

      CGAL::Object obj = isec.first;
      if (CGAL::assign(psec, obj)) {
        Vector4D vsec(psec.x(), psec.y(), psec.z());
        double fsec = (vsec - pv1->v).length()/l12;
        elem.pushIsecList(fsec);
      }
      else {
        MB_DPRINTLN("ERROR assign to pointer failed!!");
      }
    }

  }
  
}


void RendIntData::cleanupSilEdgeLines()
{
  delete m_pEgMesh;
  m_pEgMesh = NULL;

  Tree *ptree = static_cast<Tree *>(m_pTree);
  delete ptree;
  m_pTree = NULL;

  m_vertvec.clear();
  m_facevec.clear();
  m_silEdges.clear();
  m_secpts.clear();
  
  //m_silVertSet.clear();
  //m_vvl.clear();
}

