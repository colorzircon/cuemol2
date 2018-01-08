// -*-Mode: C++;-*-
//
// Molecular model coordinates class
//

#include <common.h>

#include "AnimMol.hpp"
#include "MolArrayMap.hpp"
#include "MolAtom.hpp"
#include <qlib/LTimeValue.hpp>

using namespace molstr;

AnimMol::AnimMol() : m_nValidFlag(CRD_ATOM_VALID), MolCoord()
{
  m_bSelfAnim = false;
}

char AnimMol::getCrdValidFlag() const
{
  return m_nValidFlag;
}

void AnimMol::invalidateCrdArray()
{
  m_nValidFlag = CRD_ATOM_VALID;
  m_indmap.clear();
  // m_crdarray.clear();
}

/*
*/

Vector4D AnimMol::getAtomCrd(int aid) const
{
  if (m_nValidFlag==CRD_ATOM_VALID ||
      m_indmap.size()==0)
    return Vector4D();

  CrdIndexMap::const_iterator iter = m_indmap.find(aid);
  if (iter==m_indmap.end())
    return Vector4D();

  quint32 ind = iter->second;
  AnimMol *pthis = const_cast<AnimMol *>(this);
  qfloat32 *pcrd = pthis->getCrdArrayImpl();

  return Vector4D(pcrd[ind*3+0],
                  pcrd[ind*3+1],
                  pcrd[ind*3+2]);
  
}

void AnimMol::setAtomCrd(int aid, const Vector4D &pos)
{
  if (m_indmap.size()==0) {
    // TO DO: throw exception
    MB_THROW(qlib::RuntimeException, "setAtomArray: crdarray not initialized");
    return;
  }
  
  CrdIndexMap::const_iterator iter = m_indmap.find(aid);
  if (iter==m_indmap.end()) {
    // TO DO: throw exception
    LString msg = LString::format("setAtomArray: setAtomArray AID %d not found in indmap", aid);
    MB_THROW(qlib::RuntimeException, msg);
    return;
  }

  quint32 ind = iter->second;
  qfloat32 *pcrd = getCrdArrayImpl();

  pcrd[ind*3+0] = (float) pos.x();
  pcrd[ind*3+1] = (float) pos.y();
  pcrd[ind*3+2] = (float) pos.z();
}

float *AnimMol::getAtomCrdArray()
{
  if (m_nValidFlag!=CRD_ATOM_VALID)
    return getCrdArrayImpl(); //&m_crdarray[0];

  updateCrdArray();
  return getCrdArrayImpl(); //&m_crdarray[0];
}


void AnimMol::updateCrdArray()
{
  if (m_nValidFlag==CRD_BOTH_VALID)
    return;
  
  quint32 i;
  //const quint32 natoms = m_aidmap.size();
  const quint32 natoms = getAtomSize();
  
  // create the index map
  if (m_indmap.size()!=natoms ||
      m_aidmap.size()!=natoms)
    createIndexMapImpl(m_indmap, m_aidmap);

  if (m_nValidFlag==CRD_ATOM_VALID) {
    // copy from atom to array
    // (Update crdarray)

    // make float array
    qfloat32 *pcrd = getCrdArrayImpl();
    for (i=0; i<natoms; ++i) {
      quint32 aid = m_aidmap[i];
      MolAtomPtr pAtom = getAtom(aid);
      Vector4D pos = pAtom->getRawPos();

      pcrd[i*3+0] = (float) pos.x();
      pcrd[i*3+1] = (float) pos.y();
      pcrd[i*3+2] = (float) pos.z();
    }

    LOG_DPRINTLN("CrdArray/IndMap created: natoms=%d", natoms);
    m_nValidFlag=CRD_BOTH_VALID;
  }
  else if (m_nValidFlag==CRD_ARRAY_VALID) {
    // copy from array to atom
    qfloat32 *pcrd = getCrdArrayImpl();

    for (i=0; i<natoms; ++i) {
      quint32 aid = m_aidmap[i];
      MolAtomPtr pAtom = getAtom(aid);

      Vector4D pos(pcrd[i*3+0],
                   pcrd[i*3+1],
                   pcrd[i*3+2]);
      pAtom->setRawPos(pos);
    }

  }
}

quint32 AnimMol::getCrdArrayInd(int aid) const
{
  if (m_nValidFlag==CRD_ATOM_VALID) {
    AnimMol *pthis = const_cast<AnimMol *>(this);
    pthis->updateCrdArray();
  }

  CrdIndexMap::const_iterator iter = m_indmap.find(aid);
  if (iter==m_indmap.end()) {
    MB_THROW(qlib::RuntimeException, "getCrdArrayInd failed");
    return (quint32) -1;
  }

  return iter->second;
}

////////////////////////////
// Self simple animation

void AnimMol::setSelfAnim(bool b)
{
  if (b==m_bSelfAnim)
    return;

  if (b) {
    // start self anim
    startSelfAnim();
  }
  else {
    // stop self anim
    stopSelfAnim();
  }
}

void AnimMol::startSelfAnim()
{
  qlib::EventManager *pEvMgr = qlib::EventManager::getInstance();
  pEvMgr->setTimer(this, getSelfAnimLen());
  m_bSelfAnim = true;
}

void AnimMol::stopSelfAnim()
{
  qlib::EventManager *pEvMgr = qlib::EventManager::getInstance();
  pEvMgr->removeTimer(this);

  if (m_bSelfAnim) {
    
    // send Fix object change event (to fix changes after the dynamic changes)
    {
      qsys::ObjectEvent obe;
      obe.setType(qsys::ObjectEvent::OBE_CHANGED_FIXDYN);
      obe.setTarget(getUID());
      obe.setDescr("atomsMoved");
      fireObjectEvent(obe);
    }

    m_bSelfAnim = false;
  }
}

void AnimMol::unloading()
{
  stopSelfAnim();
}

qlib::time_value AnimMol::getSelfAnimLen() const
{
  // very long time (one day)
  return qlib::timeval::fromMilliSec(1000*3600*24);
}

