// -*-Mode: C++;-*-
//
// Molecular coordinates with animation support
//

#ifndef MOLSTR_ANIMMOL_HPP_INCLUDED
#define MOLSTR_ANIMMOL_HPP_INCLUDED

#include "molstr.hpp"
#include "MolCoord.hpp"

namespace molstr {

  ///
  /// Molecular coordinates with animation support
  ///
  class MOLSTR_API AnimMol : public MolCoord
  {
    MC_SCRIPTABLE;

  public:
    typedef std::map<int, quint32> CrdIndexMap;
    typedef std::vector<quint32> AidIndexMap;

  private:
    /// MolAtom/CrdArray validity flag
    char m_nValidFlag;


    /// Atom ID --> CrdArray index mapping
    CrdIndexMap m_indmap;
    
    /// CrdArray index --> Atom ID mapping
    AidIndexMap m_aidmap;
    
    // std::vector<float> m_crdarray;

  public:
    AnimMol() : m_nValidFlag(CRD_ATOM_VALID), MolCoord()
    {
    }

    /// Get the crdarray/atompos validity flag
    virtual char getCrdValidFlag() const;

    /// Mark as the crdarray is invalid (and cleanup the index mapping)
    virtual void invalidateCrdArray();

    /// Get the implementation of the crdarray
    /// (Implementation is different in the derived classes)
    virtual qfloat32 *getCrdArrayImpl() =0;

    /// Create mappings betwee AID and array index suitable for the implementation
    virtual void createIndexMapImpl(CrdIndexMap &indmap, AidIndexMap &aidmap) =0;

    ////////

    /// get crdarray index from AID
    quint32 getCrdArrayInd(int aid) const;


    /// Get atom's coordinates from CrdArray
    Vector4D getAtomCrd(int aid) const;

    /// Set atom's coordinates to CrdArray
    void setAtomCrd(int aid, const Vector4D &pos);

    /// Get the coordinates array
    /// If the crdarray is not valid (and atom's pos are valid),
    /// update the crdarray and returns the ptr.
    qfloat32 *getAtomCrdArray();

    /// Update crdarray or atom pos to make both data valid
    void updateCrdArray();

    /// Mark as the crdarray is valid (and atom pos are invalid)
    void crdArrayChanged() {
      m_nValidFlag = CRD_ARRAY_VALID;
    }



  };

}

#endif
