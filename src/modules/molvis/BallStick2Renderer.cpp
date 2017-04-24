// -*-Mode: C++;-*-
//
//  Ball & Stick model renderer class version 2
//

#include <common.h>
#include "molvis.hpp"

#include "BallStick2Renderer.hpp"

#include <modules/molstr/MolCoord.hpp>
#include <modules/molstr/MolChain.hpp>
#include <modules/molstr/MolResidue.hpp>
#include <modules/molstr/ResiToppar.hpp>
//#include <modules/molstr/AtomIterator.hpp>
//#include <modules/molstr/BondIterator.hpp>

#include <modules/molstr/ResidIterator.hpp>

#include <gfx/DrawAttrArray.hpp>

using namespace molvis;
using namespace molstr;

using gfx::DisplayContext;
using gfx::ColorPtr;

BallStick2Renderer::BallStick2Renderer()
{
}

BallStick2Renderer::~BallStick2Renderer()
{
  MB_DPRINTLN("BallStick2Renderer destructed %p", this);
}

const char *BallStick2Renderer::getTypeName() const
{
  return "ballstick2";
}

void BallStick2Renderer::invalidateDisplayCache()
{
  super_t::invalidateDisplayCache();
}

void BallStick2Renderer::preRender(DisplayContext *pdc)
{
  MB_DPRINTLN("BallStick2Renderer::preRender setLit TRUE");
  pdc->setLighting(true);
}

void BallStick2Renderer::postRender(DisplayContext *pdc)
{
  MB_DPRINTLN("BallStick2Renderer::postRender setLit FALSE");
  pdc->setLighting(false);
}

////////////

void BallStick2Renderer::beginRend(DisplayContext *pdl)
{
  //if (m_atoms.size()>0)
  //m_atoms.clear();

  pdl->setDetail(m_nDetail);

  m_sphrdat.clear();
  m_cyldat.clear();
}

void BallStick2Renderer::endRend(DisplayContext *pdl)
{
  if ( m_fRing && !qlib::isNear4(m_thickness, 0.0) ) {
    buildRingData();
  }

  MolCoordPtr pCMol = getClientMol();
  MolAtomPtr pAtom;
  
  BOOST_FOREACH (const Sphr &s, m_sphrdat) {
    pAtom = pCMol->getAtom(s.aid);
    pdl->color(ColSchmHolder::getColor(pAtom));
    pdl->sphere(s.rad, pAtom->getPos());
  }

  MolAtomPtr pAtom1, pAtom2;
  ColorPtr pcol1, pcol2;
  
  BOOST_FOREACH (const Cyl &c, m_cyldat) {
    pAtom1 = pCMol->getAtom(c.aid1);
    pAtom2 = pCMol->getAtom(c.aid2);

    const Vector4D &pos1 = pAtom1->getPos();
    const Vector4D &pos2 = pAtom2->getPos();
    
    ColorPtr pcol1 = ColSchmHolder::getColor(pAtom1);
    ColorPtr pcol2 = ColSchmHolder::getColor(pAtom2);

    if ( pcol1->equals(*pcol2.get()) ) {
      pdl->color(pcol1);
      pdl->cylinder(c.rad, pos1, pos2);
    }
    else {
      const Vector4D mpos = (pos1 + pos2).divide(2.0);
      pdl->color(pcol1);
      pdl->cylinder(c.rad, pos1, mpos);
      pdl->color(pcol2);
      pdl->cylinder(c.rad, pos2, mpos);
    }
  }

  BOOST_FOREACH (const Ring &r, m_ringdat) {
    
  }

}

bool BallStick2Renderer::isRendBond() const
{
//  if (m_bDrawRingOnly)
//    return false;

  return true;
}

void BallStick2Renderer::rendAtom(DisplayContext *pdl, MolAtomPtr pAtom, bool)
{
  //checkRing(pAtom->getID());

  //if (m_bDrawRingOnly)
  //return;

  if (m_sphr>0.0) {
    Sphr s;
    s.aid = pAtom->getID();
    s.rad = m_sphr;
    m_sphrdat.push_back(s);
  }
}

void BallStick2Renderer::rendBond(DisplayContext *pdl, MolAtomPtr pAtom1, MolAtomPtr pAtom2, MolBond *pMB)
{
//  if (m_bDrawRingOnly)
//    return;

  //if (m_nVBMode==VBMODE_TYPE1)
  //drawVBondType1(pAtom1, pAtom2, pMB, pdl);
  //else
  //drawInterAtomLine(pAtom1, pAtom2);

  if (m_bondw>0.0) {
    Cyl c;
    c.aid1 = pAtom1->getID();
    c.aid2 = pAtom2->getID();
    c.rad = m_bondw;
    c.btype = pMB->getType();
    m_cyldat.push_back(c);
  }
}

void BallStick2Renderer::buildRingData()
{
  int i, j;
  MolCoordPtr pMol = getClientMol();
  SelectionPtr pSel = getSelection();
  ResidIterator iter(pMol, pSel);
  MolResiduePtr pRes;
  MolAtomPtr pAtom;
  for (iter.first(); iter.hasMore(); iter.next()) {
    pRes = iter.get();

    ResiToppar *pTop = pRes->getTopologyObj();
    if (pTop==NULL)
      continue;

    int nrings = pTop->getRingCount();
    for (i=0; i<nrings; i++) {
      const ResiToppar::RingAtomArray *pmembs = pTop->getRing(i);
      std::deque<int> ring_atoms;

      // Completeness flag of the ring
      bool fcompl = true;

      MB_DPRINTLN("Ring in %s", pRes->toString().c_str());
      // Check completeness of the ring
      for (j=0; j<pmembs->size(); j++) {
        LString nm = pmembs->at(j);
	pAtom = pRes->getAtom(nm);

	if (pAtom.isnull() ||
	    !pSel->isSelected(pAtom)) {
          fcompl = false;
          break;
        }

        ring_atoms.push_back(pAtom->getID());
	MB_DPRINTLN("  Atom: %s", pAtom->toString().c_str());
      }

      // Ignore incomplete rings
      if (!fcompl)
	continue;

      Ring r;
      r.atoms.resize(ring_atoms.size());
      std::deque<int>::const_iterator iter = ring_atoms.begin();
      for (j=0; j<ring_atoms.size(); ++j, ++iter) {
	r.atoms[j] = *iter;
      }
      m_ringdat.push_back(r);
    }

  }

}

void BallStick2Renderer::propChanged(qlib::LPropEvent &ev)
{
  if (ev.getName().equals("bondw") ||
      ev.getName().equals("sphr") ||
      ev.getName().equals("detail") ||
      ev.getName().equals("ring") ||
      ev.getName().equals("thickness") ||
      ev.getName().equals("ringcolor") ||
      ev.getName().equals("glrender_mode")) {
    invalidateDisplayCache();
  }
  else if (ev.getParentName().equals("coloring")||
      ev.getParentName().startsWith("coloring.")) {
    invalidateDisplayCache();
  }

  MolAtomRenderer::propChanged(ev);
}

