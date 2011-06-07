/**********************************************************************
  XtalComp - Determine if two crystal description represent the same
  structure

  Copyright (C) 2011 by David C. Lonie

  This source code is released under the New BSD License, (the "License").

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.
***********************************************************************/

#include "xtalcomp.h"

#include <algorithm>
#include <assert.h>
#include <limits.h>
#include <stddef.h>
#include <stdio.h>

#define RAD_TO_DEG 57.2957795131
#define DEG_TO_RAD 0.0174532925199

//#define XTALCOMP_DEBUG 1
#undef XTALCOMP_DEBUG

#ifdef XTALCOMP_DEBUG

#define DEBUG_BREAK printf("\n");
#define DEBUG_DIV printf("\
--------------------------------------------------------------------------------\n")
#define DEBUG_ATOM(t,c) printf("%2d: | %9.5f %9.5f %9.5f |\n", t, (c).x(), (c).y(), (c).z())
#define DEBUG_ATOMCF(t,c,f) printf("%2d: | %9.5f %9.5f %9.5f | %9.5f %9.5f %9.5f |\n", \
                                   t, (c).x(), (c).y(), (c).z(),        \
                                   (f).x(), (f).y(), (f).z())
#define DEBUG_VECTOR(v) printf("| %9.5f %9.5f %9.5f |\n", (v).x(), (v).y(), (v).z())
#define DEBUG_MATRIX(m) printf("| %9.5f %9.5f %9.5f |\n"        \
                               "| %9.5f %9.5f %9.5f |\n"        \
                               "| %9.5f %9.5f %9.5f |\n",       \
                               (m)(0,0), (m)(0,1), (m)(0,2),    \
                               (m)(1,0), (m)(1,1), (m)(1,2),    \
                               (m)(2,0), (m)(2,1), (m)(2,2))
#define DEBUG_STRING_VECTOR(s,v) printf("%s: | %9.5f %9.5f %9.5f |\n",  \
                                        s, (v).x(), (v).y(), (v).z())
#define DEBUG_MATRIX4(m) printf("| %9.5f %9.5f %9.5f %9.5f |\n"         \
                                "| %9.5f %9.5f %9.5f %9.5f |\n"         \
                                "| %9.5f %9.5f %9.5f %9.5f |\n"         \
                                "| %9.5f %9.5f %9.5f %9.5f |\n",        \
                                (m)(0,0), (m)(0,1), (m)(0,2), (m)(0,3), \
                                (m)(1,0), (m)(1,1), (m)(1,2), (m)(1,3), \
                                (m)(2,0), (m)(2,1), (m)(2,2), (m)(2,3), \
                                (m)(3,0), (m)(3,1), (m)(3,2), (m)(3,3))
#define DEBUG_STRING(str) printf("%s\n", str)
#endif

// vecs += trans
inline void translateVectorsInPlace(std::vector<XcVector> *vecs,
                                    const XcVector &trans)
{
  for (std::vector<XcVector>::iterator it = vecs->begin(),
         it_end = vecs->end(); it != it_end; ++it) {
    *it += trans;
  }
}

// Calculate a standardized angle between v1 and v2 -- if the
// vectors are mirrored (e.g. an enantiomorphic pair of crystals),
// the angles will vary in a non-obvious way:
//
//  v1   y    v2
//  ^    ^    ^
//   \   |   /
//    \  |  /
//     \ | /
//      \|/
//  -----|--------> x
//
// v1 and v2 are simply mirrored around the y axis. Define:
// a1 = fabs(angle(x, v1))
// a2 = fabs(angle(x, v2))
// b  = fabs(angle(y, v1)) = fabs(angle(y, v2)
//
// In this case, a2 = a1 - 2b.
//
// So check if any of the angles are greater than 90. If so
// correct by calculating b = a-90, then correct with a -= 2b.
static inline double compAngle(const XcVector &v1,
                               const XcVector &v2)
{
  const double angle = fabs( acos( v1.dot(v2) /
                                   sqrt(v1.squaredNorm() *
                                        v2.squaredNorm()))
                             * RAD_TO_DEG);
  if (angle <= 90.0)
    return angle;
  else
    return angle - ( (angle - 90.0)+(angle - 90.0) );
}

class XtalComp::ReducedXtal
{
private:
  unsigned int m_numAtoms;
  std::vector<unsigned int> m_types;
  std::vector<XcVector> m_ccoords;
  std::vector<XcVector> m_fcoords;
  // Fractionation and cell matrices -- ready to use:
  // ccoord = cmat * fcoord
  // fcoord = fmat * ccoord
  XcMatrix m_cmat;
  XcMatrix m_fmat;

public:
  ReducedXtal(const XcMatrix cellMatrix,
              const std::vector<unsigned int> types,
              const std::vector<XcVector> positions) :
    m_numAtoms(types.size()),
    m_types(types),
    m_ccoords(),
    m_fcoords(positions),
    m_cmat(cellMatrix.transpose()),
    m_fmat(m_cmat.inverse())
  {
    // Fill ccoords
    m_ccoords.reserve(m_fcoords.size());
    for (std::vector<XcVector>::const_iterator it = m_fcoords.begin(),
           it_end = m_fcoords.end(); it != it_end; ++it) {
      m_ccoords.push_back(m_cmat * (*it));
    }
  }

  virtual ~ReducedXtal() {}

  unsigned int numAtoms() const {return m_numAtoms;}
  const std::vector<unsigned int> & types() const {return m_types;}
  const std::vector<XcVector> & ccoords() const {return m_ccoords;}
  const std::vector<XcVector> & fcoords() const {return m_fcoords;}
  const XcMatrix & cmat() const {return m_cmat;}
  const XcMatrix & fmat() const {return m_fmat;}

  XcVector v1() const {return m_cmat.col(0);}
  XcVector v2() const {return m_cmat.col(1);}
  XcVector v3() const {return m_cmat.col(2);}

  void frac2Cart(const XcVector &fcoord, XcVector *ccoord) const
  {
    *ccoord = m_cmat * fcoord;
  }

  void cart2Frac(const XcVector &ccoord, XcVector *fcoord) const
  {
    *fcoord = m_fmat * ccoord;
  }

  // Translate member coords by the fractional translation vector fracTrans
  void translateCoords(const XcVector & fracTrans)
  {
    for (size_t i = 0; i < m_numAtoms; ++i) {
      XcVector &fcoord = m_fcoords[i];
      // Translate fractional coord
      fcoord += fracTrans;

      // wrap to cell: Take modulus, then add 1 if modulus is
      // negative
      if ((fcoord(0) = fmod(fcoord(0), 1.0)) < 0) ++fcoord(0);
      if ((fcoord(1) = fmod(fcoord(1), 1.0)) < 0) ++fcoord(1);
      if ((fcoord(2) = fmod(fcoord(2), 1.0)) < 0) ++fcoord(2);

      // Update cartesian coord
      frac2Cart(fcoord, &m_ccoords[i]);
    }
  }
};

bool XtalComp::compare(const XcMatrix &cellMatrix1,
                       const std::vector<unsigned int> &types1,
                       const std::vector<XcVector> &positions1,
                       const XcMatrix &cellMatrix2,
                       const std::vector<unsigned int> &types2,
                       const std::vector<XcVector> &positions2,
                       const double cartTol,
                       const double angleTol)
{
  // First check that types and positions are of the same size
  if (types1.size() != positions1.size() ||
      types2.size() != positions2.size() ){
    fprintf(stderr, "XtalComp::compare was given a structure description with differing "
            "numbers of types and positions:\n\ttypes1: %d positions1: %d"
            "\n\ttypes2: %d positions2: %d\n", types1.size(), positions1.size(),
            types2.size(), positions2.size());
    return false;
  }

  // Next ensure that the two descriptions have the same number of atoms
  if (types1.size() != types2.size()) {
    return false;
  }

  // Check that compositions match
  //  Make copy of types, sort, and compare
  std::vector<unsigned int> types1Comp (types1);
  std::vector<unsigned int> types2Comp (types2);
  std::sort(types1Comp.begin(), types1Comp.end());
  std::sort(types2Comp.begin(), types2Comp.end());
  //  Compare
  for (size_t i = 0; i < types1Comp.size(); ++i) {
    if (types1Comp[i] != types2Comp[i]) {
      return false;
    }
  }

  // Build ReducedXtals
  ReducedXtal x1 (cellMatrix1, types1, positions1);
  ReducedXtal x2 (cellMatrix2, types2, positions2);

  // Niggli reduce cells if needed
  // TODO: Implement niggli algorithm
  // if (!x1.isNiggliReduced()) x1.niggliReduce();
  // if (!x2.isNiggliReduced()) x2.niggliReduce();

  // Check params here. Do not just compare the matrices, this may
  // not catch certain enantiomorphs:
  // Compare volumes. Tolerance is 1% of this->getVolume()
  const double vol1 = cellMatrix1.determinant();
  const double vol2 = cellMatrix2.determinant();
  // Match volumes to within 1%
  const double voltol = 0.01 * 0.5 * (vol1 + vol2);
  if (fabs(vol1 - vol2) > voltol) return false;

  // Normalize and compare lattice params
  const double a1 = x1.v1().squaredNorm();
  const double b1 = x1.v2().squaredNorm();
  const double c1 = x1.v3().squaredNorm();
  const double a2 = x2.v1().squaredNorm();
  const double b2 = x2.v2().squaredNorm();
  const double c2 = x2.v3().squaredNorm();
  // Estimate scaled error, 4 * x * \Delta x
  const double cart2Tol ( 4.0 * sqrt((a1 + b1 + c1 + a2 + b2 + c2)
                                     * 0.166666666667) * cartTol);
  if (fabs(a1 - a2) > cart2Tol) return false;
  if (fabs(b1 - b2) > cart2Tol) return false;
  if (fabs(c1 - c2) > cart2Tol) return false;

  // Angles -- see comment above definition of compAngle for
  // explanation
  const double alpha1 = compAngle(x1.v2(), x1.v3());
  const double beta1  = compAngle(x1.v1(), x1.v3());
  const double gamma1 = compAngle(x1.v1(), x1.v2());
  const double alpha2 = compAngle(x2.v2(), x2.v3());
  const double beta2  = compAngle(x2.v1(), x2.v3());
  const double gamma2 = compAngle(x2.v1(), x2.v2());
  if (fabs(alpha1 - alpha2) > angleTol) return false;
  if (fabs(beta1  - beta2)  > angleTol) return false;
  if (fabs(gamma1 - gamma2) > angleTol) return false;

  // Run the XtalComp algorithm
  XtalComp xc (&x1, &x2, cartTol, angleTol);

  // iterate through comparisons
  while (xc.hasMoreTransforms()) {
    xc.applyNextTransform();
    if (xc.compareCurrent()) {
      // Found a match!
      return true;
    }
  }

  // No match
  return false;
}

XtalComp::~XtalComp()
{
  // ReducedXtals are cleaned up in compare(...)
}

XtalComp::XtalComp(ReducedXtal *x1, ReducedXtal *x2,
                   const double cartTol, const double angleTol) :
  m_rx1(x1),
  m_rx2(x2),
  m_lengthtol(cartTol),
  m_angletol(angleTol),
  m_lfAtomType(UINT_MAX),
  m_lfAtomCount(UINT_MAX),
  m_transformsIndex(0)
{
  setLeastFrequentAtomInfo();
  setReferenceBasis();
  pretranslateRx1();
  buildSuperLfCCoordList2();

#ifdef XTALCOMP_DEBUG
  DEBUG_BREAK;
  DEBUG_DIV;
  printf("Number of atoms: %d and %d\n", x1->numAtoms(), x2->numAtoms());
  printf("There are %d atoms of type %d\n", m_lfAtomCount, m_lfAtomType);
  DEBUG_DIV;
  DEBUG_STRING("Reference Xtal 1 cmat:");
  DEBUG_MATRIX(m_rx1->cmat());
  DEBUG_STRING("Reference Xtal 1 fmat:");
  DEBUG_MATRIX(m_rx1->fmat());
  DEBUG_STRING("Reference Xtal 1 atoms (cart|frac):");
  for (int i = 0; i < m_rx1->ccoords().size(); ++i) {
    DEBUG_ATOMCF(m_rx1->types()[i], m_rx1->ccoords()[i],
                 m_rx1->fcoords()[i]);
  }
  DEBUG_DIV;
  DEBUG_STRING("Reference Xtal 2 cmat:");
  DEBUG_MATRIX(m_rx2->cmat());
  DEBUG_STRING("Reference Xtal 2 fmat:");
  DEBUG_MATRIX(m_rx2->fmat());
  DEBUG_STRING("Original Xtal 2 atoms (cart|frac):");
  for (int i = 0; i < m_rx2->ccoords().size(); ++i) {
    DEBUG_ATOMCF(m_rx2->types()[i], m_rx2->ccoords()[i],
                 m_rx2->fcoords()[i]);
  }
  DEBUG_DIV;
#endif

  findCandidateTransforms();

#ifdef XTALCOMP_DEBUG
  printf("Number of transforms: %d\n", m_transforms.size());
  DEBUG_DIV;
#endif
}

void XtalComp::setLeastFrequentAtomInfo()
{
  // Get list of unique types
  const std::vector<unsigned int> &rx1_types = m_rx1->types();
  const std::vector<unsigned int> &rx2_types = m_rx2->types();
  // Copy
  std::vector<unsigned int> uniqueTypes (rx1_types);
  // Sort before unique'ing
  sort(uniqueTypes.begin(), uniqueTypes.end());
  // Remove consecutive duplicates
  const std::vector<unsigned int>::const_iterator uniqueTypes_end =
    unique_copy(rx1_types.begin(), rx1_types.end(), uniqueTypes.begin());
  // Now the range [uniqueTypes.begin(), uniqueTypes.end()) is
  // sorted and unique.

  // determine least frequent atom type
  for (std::vector<unsigned int>::iterator it = uniqueTypes.begin();
       it != uniqueTypes_end; ++it) {
    ptrdiff_t cur = count(rx1_types.begin(), rx1_types.end(), *it);
    unsigned int ucur = static_cast<unsigned int>(cur);
    if (ucur < m_lfAtomCount) {
      m_lfAtomCount = ucur;
      m_lfAtomType = *it;
    }
  }
}

void XtalComp::setReferenceBasis()
{
  // Just use the cell vectors of rx1
  m_refVec1 = m_rx1->cmat().col(0);
  m_refVec2 = m_rx1->cmat().col(1);
  m_refVec3 = m_rx1->cmat().col(2);
}

void XtalComp::pretranslateRx1()
{
  // Find a translation vector that moves an lfAtom in rx1 to the
  // origin (e.g., the negative of an lfAtom's coordinates)
  assert (m_rx1->fcoords().size() == m_rx1->types().size());
  size_t refTransIndex = 0;
  std::vector<unsigned int>::const_iterator refTransTypeIterator
    = m_rx1->types().begin();

  // Assert that there is at least one atom of type lfAtomType
  assert (find(m_rx1->types().begin(), m_rx1->types().end(), m_lfAtomType)
            != m_rx1->types().end());

  while (*refTransTypeIterator != m_lfAtomType) {
    ++refTransIndex;
    ++refTransTypeIterator;
  }

  XcVector rx1_ftrans = - (m_rx1->fcoords()[refTransIndex]);

  // Translate rx1 by the above vector. This places a lfAtom at the origin.
  m_rx1->translateCoords(rx1_ftrans);
}

void XtalComp::buildSuperLfCCoordList2()
{
  // Find all lfAtoms in rx2 and build a supercell of them
  const std::vector<unsigned int> &types = m_rx2->types();
  const std::vector<XcVector> &ccoords = m_rx2->ccoords();
  m_superLfCCoordList2.clear();
  m_superLfCCoordList2.reserve(8 * m_lfAtomCount);

  assert (ccoords.size() == types.size());

  // Determine the length of the cell diagonal. If it is the same as
  // any vector length, we need to build a 3x3x3
  // supercell. Otherwise, a (faster) 2x2x2 will suffice.
  const XcVector v1 (m_rx2->cmat().col(0)); // 1 0 0
  const XcVector v2 (m_rx2->cmat().col(1)); // 0 1 0
  const XcVector v3 (m_rx2->cmat().col(2)); // 0 0 1
  const double v1SqNorm = v1.squaredNorm();
  const double v2SqNorm = v2.squaredNorm();
  const double v3SqNorm = v3.squaredNorm();
  const double diagSqNorm = (v1+v2+v3).squaredNorm();
  const double normTol = 1e-4;

  // 3x3x3 case:
  if (fabs(diagSqNorm - v1SqNorm) < normTol ||
      fabs(diagSqNorm - v2SqNorm) < normTol ||
      fabs(diagSqNorm - v3SqNorm) < normTol ){
    const XcVector v4 (v1  + v2); // 1 1 0
    const XcVector v5 (v4  + v2); // 1 2 0
    const XcVector v6 (v1  + v3); // 1 0 1
    const XcVector v7 (v6  + v3); // 1 0 2
    const XcVector v8 (v4  + v3); // 1 1 1
    const XcVector v9 (v8  + v3); // 1 1 2
    const XcVector v10(v8  + v2); // 1 2 1
    const XcVector v11(v10 + v3); // 1 2 2
    const XcVector v12(v1  + v1); // 2 0 0
    const XcVector v13(v12 + v2); // 2 1 0
    const XcVector v14(v13 + v2); // 2 2 0
    const XcVector v15(v12 + v3); // 2 0 1
    const XcVector v16(v15 + v3); // 2 0 2
    const XcVector v17(v13 + v3); // 2 1 1
    const XcVector v18(v17 + v3); // 2 1 2
    const XcVector v19(v17 + v2); // 2 2 1
    const XcVector v20(v19 + v3); // 2 2 2
    const XcVector v21(v2  + v3); // 0 1 1
    const XcVector v22(v22 + v3); // 0 1 2
    const XcVector v23(v2  + v2); // 0 2 0
    const XcVector v24(v23 + v3); // 0 2 1
    const XcVector v25(v24 + v3); // 0 2 2
    const XcVector v26(v3  + v3); // 0 0 2

    for (size_t i = 0; i < types.size(); ++i) {
      if (types[i] == m_lfAtomType) {
        const XcVector &tmpVec = ccoords[i];
        // Add to cell
        m_superLfCCoordList2.push_back(tmpVec);
        // Replicate to supercell
        m_superLfCCoordList2.push_back(tmpVec + v1 );
        m_superLfCCoordList2.push_back(tmpVec + v2 );
        m_superLfCCoordList2.push_back(tmpVec + v3 );
        m_superLfCCoordList2.push_back(tmpVec + v4 );
        m_superLfCCoordList2.push_back(tmpVec + v5 );
        m_superLfCCoordList2.push_back(tmpVec + v6 );
        m_superLfCCoordList2.push_back(tmpVec + v7 );
        m_superLfCCoordList2.push_back(tmpVec + v8 );
        m_superLfCCoordList2.push_back(tmpVec + v9 );
        m_superLfCCoordList2.push_back(tmpVec + v10);
        m_superLfCCoordList2.push_back(tmpVec + v11);
        m_superLfCCoordList2.push_back(tmpVec + v12);
        m_superLfCCoordList2.push_back(tmpVec + v13);
        m_superLfCCoordList2.push_back(tmpVec + v14);
        m_superLfCCoordList2.push_back(tmpVec + v15);
        m_superLfCCoordList2.push_back(tmpVec + v16);
        m_superLfCCoordList2.push_back(tmpVec + v17);
        m_superLfCCoordList2.push_back(tmpVec + v18);
        m_superLfCCoordList2.push_back(tmpVec + v19);
        m_superLfCCoordList2.push_back(tmpVec + v20);
        m_superLfCCoordList2.push_back(tmpVec + v21);
        m_superLfCCoordList2.push_back(tmpVec + v22);
        m_superLfCCoordList2.push_back(tmpVec + v23);
        m_superLfCCoordList2.push_back(tmpVec + v24);
        m_superLfCCoordList2.push_back(tmpVec + v25);
        m_superLfCCoordList2.push_back(tmpVec + v26);
      }
    }
  }

  // 2x2x2 case:
  else {
    const XcVector v4 (v1  + v2); // 1 1 0
    const XcVector v5 (v1  + v3); // 1 0 1
    const XcVector v6 (v4  + v3); // 1 1 1
    const XcVector v7 (v2  + v3); // 0 1 1

    for (size_t i = 0; i < types.size(); ++i) {
      if (types[i] == m_lfAtomType) {
        const XcVector &tmpVec = ccoords[i];
        // Add to cell
        m_superLfCCoordList2.push_back(tmpVec);
        // Replicate to supercell
        m_superLfCCoordList2.push_back(tmpVec + v1 );
        m_superLfCCoordList2.push_back(tmpVec + v2 );
        m_superLfCCoordList2.push_back(tmpVec + v3 );
        m_superLfCCoordList2.push_back(tmpVec + v4 );
        m_superLfCCoordList2.push_back(tmpVec + v5 );
        m_superLfCCoordList2.push_back(tmpVec + v6 );
        m_superLfCCoordList2.push_back(tmpVec + v7 );
      }
    }
  }
}

void XtalComp::findCandidateTransforms()
{
  // Find all sets of 4 coordinates that correspond to the reference
  // translation vectors
  //
  // a
  // |  c
  // | /
  // |/
  // o------b
  //
  // o, a, b, and c are cartesian coordinates in
  // m_superLfCCoordList. Define:
  //
  // t1 = a-o
  // t2 = b-o
  // t3 = c-o
  //
  // If:
  //
  // t1.squaredNorm() == m_refVec1.squaredNorm() &&
  // t2.squaredNorm() == m_refVec2.squaredNorm() &&
  // t3.squaredNorm() == m_refVec3.squaredNorm() &&
  // angle(t1,t2) == angle(m_refVec1,refVec2) (== gamma) &&
  // angle(t1,t3) == angle(m_refVec1,refVec3) (== beta) &&
  // angle(t2,t3) == angle(m_refVec2,refVec3) (== alpha)
  //
  // Then we have a valid candidate reference frame. Store -o as the
  // translation and calculate a rotation/reflection matrix that
  // will transform t1, t2, t3 into the refVecs. Use the XcTransform
  // class to store these operations for later retrieval.

  // Set tolerance for vector checks:
  const double tol = m_lengthtol;
  const double squaredTol = tol*tol;

  // Allocate room for all lfAtoms in the translations list, (this
  // is the maximum)
  m_transforms.clear();
  m_transforms.reserve(m_lfAtomCount);

  // Lists to store candidate tX vectors
  std::vector<XcVector> t1_candidates;
  std::vector<XcVector> t2_candidates;
  std::vector<XcVector> t3_candidates;

  // Search for candidate tX vectors:
  //
  // Cache some values:
  const double v1Norm2 = m_refVec1.squaredNorm();
  const double v2Norm2 = m_refVec2.squaredNorm();
  const double v3Norm2 = m_refVec3.squaredNorm();
  const double vAlpha = compAngle(m_refVec2, m_refVec3);
  const double vBeta  = compAngle(m_refVec1, m_refVec3);
  const double vGamma = compAngle(m_refVec1, m_refVec2);
  XcMatrix V;
  V.fillCols(m_refVec1, m_refVec2, m_refVec3);
  //
  // iterate over all "o" atoms.
  for (std::vector<XcVector>::const_iterator
         atm1 = m_superLfCCoordList2.begin(),
         super_end = m_superLfCCoordList2.end();
       atm1 != super_end; ++atm1) {
    //
    // Reset candidate tX lists
    t1_candidates.clear();
    t2_candidates.clear();
    t3_candidates.clear();
    // Search for all a, b, c atoms
    for (std::vector<XcVector>::const_iterator
           atm2 = m_superLfCCoordList2.begin();
         atm2 != super_end; ++atm2) {
      //
      // Get trial vector:
      XcVector t ((*atm2) - (*atm1));
      const double tNorm2 = t.squaredNorm();
      //
      // Compare against reference vectors. Check the norm of their
      // differences to a tolerance.
      if (fabs(tNorm2 - v1Norm2) < squaredTol) t1_candidates.push_back(t);
      if (fabs(tNorm2 - v2Norm2) < squaredTol) t2_candidates.push_back(t);
      if (fabs(tNorm2 - v3Norm2) < squaredTol) t3_candidates.push_back(t);
    }

    // Move to next candidate origin if any candidates are missing:
    if (!t1_candidates.size() ||
        !t2_candidates.size() ||
        !t3_candidates.size()) {
      continue;
    }

#ifdef XTALCOMP_DEBUG
    DEBUG_DIV;
    DEBUG_STRING("Candidate transforms for offset:");
    DEBUG_VECTOR(*atm1);
    DEBUG_DIV;
    DEBUG_STRING("Candidates for t1");
    for (std::vector<XcVector>::const_iterator
           it = t1_candidates.begin(),
           it_end = t1_candidates.end();
         it != it_end; ++it) {
      DEBUG_VECTOR(*it);
    }
    DEBUG_STRING("Candidates for t2");
    for (std::vector<XcVector>::const_iterator
           it = t2_candidates.begin(),
           it_end = t2_candidates.end();
         it != it_end; ++it) {
      DEBUG_VECTOR(*it);
    }
    DEBUG_STRING("Candidates for t3");
    for (std::vector<XcVector>::const_iterator
           it = t3_candidates.begin(),
           it_end = t3_candidates.end();
         it != it_end; ++it) {
      DEBUG_VECTOR(*it);
    }
#endif

    // Search for transforms by comparing angles:
    //
    // Iterate over all t1 candidates
    for (std::vector<XcVector>::const_iterator
           t1 = t1_candidates.begin(),
           t1_end = t1_candidates.end();
         t1 != t1_end; ++t1) {
#ifdef XTALCOMP_DEBUG
      DEBUG_STRING_VECTOR("Using t1", *t1);
#endif
      // Iterate over all t2 candidates
      for (std::vector<XcVector>::const_iterator
             t2 = t2_candidates.begin(),
             t2_end = t2_candidates.end();
           t2 != t2_end; ++t2) {
#ifdef XTALCOMP_DEBUG
        DEBUG_STRING_VECTOR("Using t2", *t2);
        printf("Comparing angles: t1,t2= %f v1,v2= %f\n", compAngle(*t1,*t2), vGamma);
#endif
        // Compare t1.t2 with v1.v2
        if (fabs(compAngle(*t1,*t2) - vGamma) < m_angletol) {
          // They match, so now search for a valid t3
          for (std::vector<XcVector>::const_iterator
                 t3 = t3_candidates.begin(),
                 t3_end = t3_candidates.end();
               t3 != t3_end; ++t3) {
#ifdef XTALCOMP_DEBUG
            DEBUG_STRING_VECTOR("Using t3", *t3);
            printf("Comparing angles: t1,t3= %f v1,v2= %f\n", compAngle(*t1,*t3), vBeta);
            printf("Comparing angles: t2,t3= %f v1,v2= %f\n", compAngle(*t2,*t3), vAlpha);
#endif
            // Compare t1.t3 with v1.v3 and t2.t3 with v2.v3
            if (fabs(compAngle(*t1,*t3) - vBeta)  < m_angletol &&
                fabs(compAngle(*t2,*t3) - vAlpha) < m_angletol) {
              // t1, t2, and t3 correspond to v1, v2, and v3
              //
              // Find rotation matrix that converts matrix T = (t1,
              // t2, t3) into matrix V = (v1, v2, v3). In other
              // words, let's find a matrix R such that:
              //
              // V = R T
              //
              // Thus, R = V T^-1
              XcMatrix T;
              T.fillCols(*t1, *t2, *t3);
              const XcMatrix R (V * T.inverse());
              // Build and store XcTransform:
              XcTransform transform;
              transform.setIdentity();
              transform.rotate(R);
              transform.translate(-(*atm1));
              m_transforms.push_back(transform);
#ifdef XTALCOMP_DEBUG
              DEBUG_STRING("Found transform:");
              DEBUG_STRING("Translation:");
              DEBUG_VECTOR(-(*atm1));
              DEBUG_STRING("Rotation");
              DEBUG_MATRIX(R);
              DEBUG_STRING("transform: rot, trans");
              DEBUG_MATRIX(transform.rotation());
              DEBUG_VECTOR(transform.translation());
#endif
              // Verify that this is a pure rot/ref matrix
              assert(fabs(fabs(R.determinant()) - 1.0) < 1e-3);
            }
          }
        }
      }
    }
  }
}

bool XtalComp::hasMoreTransforms() const
{
  // Are there any more transforms?
  if (m_transformsIndex < m_transforms.size()) {
    return true;
  }
#ifdef XTALCOMP_DEBUG
  DEBUG_DIV;
  DEBUG_STRING("No more transforms");
  DEBUG_DIV;
#endif
  return false;
}

void XtalComp::applyNextTransform()
{
#ifdef XTALCOMP_DEBUG
  DEBUG_DIV;
  printf("Applying transform %d of %d\n", m_transformsIndex+1, m_transforms.size());
#endif

  // Get current transformation
  assert (m_transformsIndex < m_transforms.size());
  m_transform = m_transforms[m_transformsIndex];

#ifdef XTALCOMP_DEBUG
  DEBUG_STRING("m_transform: rot, trans");
  DEBUG_MATRIX(m_transform.rotation());
  DEBUG_VECTOR(m_transform.translation());
#endif

  buildTransformedXtal2();

  // Update transform index
  ++m_transformsIndex;

#ifdef XTALCOMP_DEBUG
  DEBUG_DIV;
  DEBUG_STRING("Transform applied");
  DEBUG_STRING("Transformed cmat:");
  DEBUG_MATRIX(m_transformedCMat);
  DEBUG_STRING("Transformed fmat:");
  DEBUG_MATRIX(m_transformedFMat);
  DEBUG_STRING("Transformed atoms (cart|frac):");
  for (int i = 0; i < m_transformedTypes.size(); ++i) {
    DEBUG_ATOMCF(m_transformedTypes.at(i), m_transformedCCoords.at(i),
                 m_transformedFCoords.at(i));
  }
  DEBUG_DIV;
#endif
}

void XtalComp::buildTransformedXtal2()
{
  // Transform matrices
  m_transformedCMat = m_transform.rotation() * m_rx2->cmat();
  m_transformedFMat = m_transformedCMat.inverse();

  // TODO isUnitary
  //assert((m_transformedFMat * m_transformedCMat).isUnitary(1e-4));

  // Reset transformed types to the original types
  m_transformedTypes.resize(m_rx2->types().size());
  copy(m_rx2->types().begin(), m_rx2->types().end(),
       m_transformedTypes.begin());

  // Set transformed coordinates:
  //
  // First we need to adjust the transformation matrix. The
  // translation in m_transform is in cartesian units, but we want
  // to transform the fractional coordinates, since we will need to
  // expand them before converting to the final cartesian set. So we
  // first make a copy of the current transform:
  XcTransform fracTransform (m_transform);
  // We will be multiplying fractional coords from the right, so
  // append a frac -> cart conversion to the right hand side of the
  // transform:
  fracTransform.rotate(m_rx2->cmat());
  // We then need to convert back into fractional coordinates after the translation
  fracTransform.prerotate(m_transformedFMat);
#ifdef XTALCOMP_DEBUG
  DEBUG_STRING("FracTransform: rot, trans");
  DEBUG_MATRIX(fracTransform.rotation());
  DEBUG_VECTOR(fracTransform.translation());
#endif
  // Now to perform the transformations
  m_transformedFCoords.clear();
  m_transformedFCoords.reserve(m_rx2->fcoords().size());
  for (std::vector<XcVector>::const_iterator
         it = m_rx2->fcoords().begin(),
         it_end = m_rx2->fcoords().end();
       it != it_end; ++it) {
    // Transform each fractional coordinate
    m_transformedFCoords.push_back(fracTransform * (*it));
  }

  // Expand cell near boundaries. This will also wrap the atoms to
  // the cell
  expandFractionalCoordinates(&m_transformedTypes, &m_transformedFCoords,
                              m_transformedCMat, m_lengthtol);

  // Convert to cartesian
  m_transformedCCoords.clear();
  m_transformedCCoords.reserve(m_transformedFCoords.size());
  for (std::vector<XcVector>::iterator
         it = m_transformedFCoords.begin(),
         it_end = m_transformedFCoords.end();
       it != it_end; ++it) {
    m_transformedCCoords.push_back(m_transformedCMat * (*it));
  }
}

void XtalComp::expandFractionalCoordinates(std::vector<unsigned int> *types,
                                           std::vector<XcVector> *fcoords,
                                           const XcMatrix &cmat,
                                           const double tol)
{
  // Wrap translated coordinates and expand translated coordinates
  // (if an atom is very close to a cell boundary, place another
  // atom at the opposite boundary for numerical stability)
  //
  // Definitions using fractional basis:
  //
  //        5------8
  //       /:     /|
  //      / :    / |
  //     /  2.../..7
  //    3--/---6  /
  //    | /    | /
  //    |/     |/
  //    1------4
  //
  // Points:
  // p1 = (0 0 0) | p5 = (1 1 0)
  // p2 = (1 0 0) | p6 = (0 1 1)
  // p3 = (0 1 0) | p7 = (1 0 1)
  // p4 = (0 0 1) | p8 = (1 1 1)
  //
  // Vectors:
  // v1 (a) = p2 - p1 = (1 0 0)
  // v2 (b) = p3 - p1 = (0 1 0)
  // v3 (c) = p4 - p1 = (0 0 1)
  //
  // Planes:
  // #: normal point | points
  // 1:   v1    p1   | 1,3,4,6
  // 2:   v2    p1   | 1,2,4,7
  // 3:   v3    p1   | 1,2,3,5
  // 4:   v1    p8   | 2,5,7,8
  // 5:   v2    p8   | 3,5,6,8
  // 6:   v3    p8   | 4,6,7,8
  //
  // Edges:
  // # | pts | planes   ## | pts | planes
  // 1 | 1,2 |  2,3      7 | 3,6 |  1,5
  // 2 | 1,3 |  1,3      8 | 4,6 |  1,6
  // 3 | 1,4 |  1,2      9 | 4,7 |  2,6
  // 4 | 2,5 |  3,4     10 | 5,8 |  4,5
  // 5 | 2,7 |  2,4     11 | 6,8 |  5,6
  // 6 | 3,5 |  3,5     12 | 7.8 |  4,6
  //
  // Defining cell boundaries as planes, n ( r - r0 ) = 0,
  // n = normal vector
  // r0 = point in plane
  // r = coordinate
  //
  // There are 6 planes, defined using v1, v2, v3 as possible n, and
  // p1, p8 as possible r0:
  //
  // Plane 1:
  //  n = v1; r0 = p1
  //  Plane equ: v1 ( r - p1 ) = 0 = v1[0] * r[0]
  //             r[0] = 0
  //
  // Plane 2:
  //  n = v2; r0 = p1
  //  Plane equ: v2 ( r - p1 ) = 0 = v2[1] * r[1]
  //             r[1] = 0
  //
  // Plane 3:
  //  n = v3; r0 = p1
  //  Plane equ: v3 ( r - p1 ) = 0 = v3[2] * r[2]
  //             r[2] = 0
  //
  // Plane 4:
  //  n = v1; r0 = p8
  //  Plane equ: v1 ( r - p8 ) = 0 = v1[0] * r[0] - v1[1] * r0[0]
  //             r[0] - 1 = 0
  //
  // Plane 5:
  //  n = v2; r0 = p8
  //  Plane equ: v2 ( r - p8 ) = 0 = v2[1] * r[1] - v2[1] * r0[1]
  //             r[1] - 1 = 0
  //
  // Plane 6:
  //  n = v3; r0 = p8
  //  Plane equ: v3 ( r - p8 ) = 0 = v3[2] * r[2] - v3[2] * r0[2]
  //             r[2] - 1 = 0
  //
  // Given a plane defined as
  //
  // a*r[0] + b*r[1] + c*r[2] + d = 0
  //
  // The distance to a point r1 is defined as:
  //
  // D = |a*r1[0] + b*r1[1] + c*r1[2] + d| / sqrt(a*a + b*b + c*c)
  //
  // The shortest vector between point and plane in the fractional
  // basis "delta_f" is the distance above times the normalized
  // normal vector:
  //
  // delta_f = D * n
  //
  // For the six planes, the distance magnitudes and
  //
  // Plane 1:
  //   a = 1   b = 0   c = 0   d = 0
  //   D = r1[0]
  //   delta_f = r1[0] * v1
  //
  // Plane 2:
  //   a = 0   b = 1   c = 0   d = 0
  //   D = r1[1]
  //   delta_f = r1[1] * v2
  //
  // Plane 3:
  //   a = 0   b = 0   c = 1   d = 0
  //   D = r1[2]
  //   delta_f = r1[2] * v3
  //
  // Plane 4:
  //   a = 1   b = 0   c = 0   d = -1
  //   D = r1[0] - 1
  //   delta_f = (r1[0] - 1) * v1
  //
  // Plane 5:
  //   a = 0   b = 0   c = 0   d = -1
  //   D = r1[1] - 1
  //   delta_f = (r1[1] - 1) * v2
  //
  // Plane 6:
  //   a = 0   b = 0   c = 0   d = -1
  //   D = r1[2] - 1
  //   delta_f = (r1[2] - 1) * v3
  //
  // After converting delta_f to the cartesian distance vector
  // delta_c, the cartesian distance is simply the norm of delta_c.
  //
  // Calculate the cartesian distance from each plane to each atom
  // and if there it is below the tolerance, add an new atom
  // translated by the normal vector of the plane to the opposite
  // side of the cell.

  assert (types->size() == fcoords->size());

  const XcVector v1 (1, 0, 0);
  const XcVector v2 (0, 1, 0);
  const XcVector v3 (0, 0, 1);

  XcVector tmpVec1;
  XcVector tmpVec2;
  const double tolSquared = tol * tol;

  double delta_c_1_sqNorm;
  double delta_c_2_sqNorm;
  double delta_c_3_sqNorm;
  double delta_c_4_sqNorm;
  double delta_c_5_sqNorm;
  double delta_c_6_sqNorm;

  const size_t numUnexpandedAtoms = fcoords->size();
  for (size_t i = 0; i < numUnexpandedAtoms; ++i) {
    XcVector &curVecMut = (*fcoords)[i];
    const unsigned int curType = (*types)[i];

    // Wrap:
    if ((curVecMut[0] = fmod(curVecMut[0], 1.0)) < 0) ++curVecMut[0];
    if ((curVecMut[1] = fmod(curVecMut[1], 1.0)) < 0) ++curVecMut[1];
    if ((curVecMut[2] = fmod(curVecMut[2], 1.0)) < 0) ++curVecMut[2];

    // This is necessary to prevent bizarre behavior when expanding
    // the cell (Possibly an aliasing bug in Eigen?) (We no longer
    // use Eigen, but lets keep this as-is just in case I
    // misdiagnosed the problem as Eigen-specific)
    const XcVector curVec =
      const_cast<const XcVector &>(curVecMut);

    // Compute distances to planes. tmpVec1 is delta_f, tmpVec2 is delta_c
    // Plane 1:
    tmpVec1.set(curVec[0], 0.0, 0.0);
    tmpVec2 = cmat * tmpVec1;
    delta_c_1_sqNorm = tmpVec2.squaredNorm();
    // Plane 2:
    tmpVec1.set(0.0, curVec[1], 0.0);
    tmpVec2 = cmat * tmpVec1;
    delta_c_2_sqNorm = tmpVec2.squaredNorm();
    // Plane 3:
    tmpVec1.set(0.0, 0.0, curVec[2]);
    tmpVec2 = cmat * tmpVec1;
    delta_c_3_sqNorm = tmpVec2.squaredNorm();
    // Plane 4:
    tmpVec1.set(curVec[0] - 1.0, 0.0, 0.0);
    tmpVec2 = cmat * tmpVec1;
    delta_c_4_sqNorm = tmpVec2.squaredNorm();
    // Plane 5:
    tmpVec1.set(0.0, curVec[1] - 1.0, 0.0);
    tmpVec2 = cmat * tmpVec1;
    delta_c_5_sqNorm = tmpVec2.squaredNorm();
    // Plane 6:
    tmpVec1.set(0.0, 0.0, curVec[2] - 1.0);
    tmpVec2 = cmat * tmpVec1;
    delta_c_6_sqNorm = tmpVec2.squaredNorm();

    // Check distances to determine near-planes
    bool nearPlane1 = (delta_c_1_sqNorm <= tolSquared);
    bool nearPlane2 = (delta_c_2_sqNorm <= tolSquared);
    bool nearPlane3 = (delta_c_3_sqNorm <= tolSquared);
    bool nearPlane4 = (delta_c_4_sqNorm <= tolSquared);
    bool nearPlane5 = (delta_c_5_sqNorm <= tolSquared);
    bool nearPlane6 = (delta_c_6_sqNorm <= tolSquared);

    // If not near any boundaries, just skip to next iteration
    if (!nearPlane1 && !nearPlane2 && !nearPlane3 &&
        !nearPlane4 && !nearPlane5 && !nearPlane6 ){
      continue;
    }

    // Add translated atoms near enough to a corner, edge, or plane

    // First check for corner atoms:
    // Recall plane definitions:
    //
    // #: normal point | points
    // -:--------------+--------
    // 1:   v1    p1   | 1,3,4,6
    // 2:   v2    p1   | 1,2,4,7
    // 3:   v3    p1   | 1,2,3,5
    // 4:   v1    p8   | 2,5,7,8
    // 5:   v2    p8   | 3,5,6,8
    // 6:   v3    p8   | 4,6,7,8

    // Corner 1:
    if (nearPlane1 && nearPlane2 && nearPlane3) {
      // Place atoms at other corners
      fcoords->push_back(curVec + v1);
      types->push_back(curType);
      fcoords->push_back(curVec + v2);
      types->push_back(curType);
      fcoords->push_back(curVec + v3);
      types->push_back(curType);
      fcoords->push_back(curVec + v1 + v2);
      types->push_back(curType);
      fcoords->push_back(curVec + v2 + v3);
      types->push_back(curType);
      fcoords->push_back(curVec + v1 + v3);
      types->push_back(curType);
      fcoords->push_back(curVec + v1 + v2 + v3);
      types->push_back(curType);
      continue;
    }
    // Corner 2
    if (nearPlane2 && nearPlane3 && nearPlane4) {
      // Place atoms at other corners
      fcoords->push_back(curVec - v1);
      types->push_back(curType);
      fcoords->push_back(curVec + v2);
      types->push_back(curType);
      fcoords->push_back(curVec + v3);
      types->push_back(curType);
      fcoords->push_back(curVec - v1 + v2);
      types->push_back(curType);
      fcoords->push_back(curVec + v2 + v3);
      types->push_back(curType);
      fcoords->push_back(curVec - v1 + v3);
      types->push_back(curType);
      fcoords->push_back(curVec - v1 + v2 + v3);
      types->push_back(curType);
      continue;
    }
    // Corner 3:
    if (nearPlane1 && nearPlane3 && nearPlane5) {
      // Place atoms at other corners
      fcoords->push_back(curVec + v1);
      types->push_back(curType);
      fcoords->push_back(curVec - v2);
      types->push_back(curType);
      fcoords->push_back(curVec + v3);
      types->push_back(curType);
      fcoords->push_back(curVec + v1 - v2);
      types->push_back(curType);
      fcoords->push_back(curVec - v2 + v3);
      types->push_back(curType);
      fcoords->push_back(curVec + v1 + v3);
      types->push_back(curType);
      fcoords->push_back(curVec + v1 - v2 + v3);
      types->push_back(curType);
      continue;
    }
    // Corner 4:
    if (nearPlane1 && nearPlane2 && nearPlane6) {
      // Place atoms at other corners
      fcoords->push_back(curVec + v1);
      types->push_back(curType);
      fcoords->push_back(curVec + v2);
      types->push_back(curType);
      fcoords->push_back(curVec - v3);
      types->push_back(curType);
      fcoords->push_back(curVec + v1 + v2);
      types->push_back(curType);
      fcoords->push_back(curVec + v2 - v3);
      types->push_back(curType);
      fcoords->push_back(curVec + v1 - v3);
      types->push_back(curType);
      fcoords->push_back(curVec + v1 + v2 - v3);
      types->push_back(curType);
      continue;
    }
    // Corner 5:
    if (nearPlane3 && nearPlane4 && nearPlane5) {
      // Place atoms at other corners
      fcoords->push_back(curVec - v1);
      types->push_back(curType);
      fcoords->push_back(curVec - v2);
      types->push_back(curType);
      fcoords->push_back(curVec + v3);
      types->push_back(curType);
      fcoords->push_back(curVec - v1 - v2);
      types->push_back(curType);
      fcoords->push_back(curVec - v2 + v3);
      types->push_back(curType);
      fcoords->push_back(curVec - v1 + v3);
      types->push_back(curType);
      fcoords->push_back(curVec - v1 - v2 + v3);
      types->push_back(curType);
      continue;
    }
    // Corner 6:
    if (nearPlane1 && nearPlane5 && nearPlane6) {
      // Place atoms at other corners
      fcoords->push_back(curVec + v1);
      types->push_back(curType);
      fcoords->push_back(curVec - v2);
      types->push_back(curType);
      fcoords->push_back(curVec - v3);
      types->push_back(curType);
      fcoords->push_back(curVec + v1 - v2);
      types->push_back(curType);
      fcoords->push_back(curVec - v2 - v3);
      types->push_back(curType);
      fcoords->push_back(curVec + v1 - v3);
      types->push_back(curType);
      fcoords->push_back(curVec + v1 - v2 - v3);
      types->push_back(curType);
      continue;
    }
    // Corner 7:
    if (nearPlane2 && nearPlane4 && nearPlane6) {
      // Place atoms at other corners
      fcoords->push_back(curVec - v1);
      types->push_back(curType);
      fcoords->push_back(curVec + v2);
      types->push_back(curType);
      fcoords->push_back(curVec - v3);
      types->push_back(curType);
      fcoords->push_back(curVec - v1 + v2);
      types->push_back(curType);
      fcoords->push_back(curVec + v2 - v3);
      types->push_back(curType);
      fcoords->push_back(curVec - v1 - v3);
      types->push_back(curType);
      fcoords->push_back(curVec - v1 + v2 - v3);
      types->push_back(curType);
      continue;
    }
    // Corner 8:
    if (nearPlane4 && nearPlane5 && nearPlane6) {
      // Place atoms at other corners
      fcoords->push_back(curVec - v1);
      types->push_back(curType);
      fcoords->push_back(curVec - v2);
      types->push_back(curType);
      fcoords->push_back(curVec - v3);
      types->push_back(curType);
      fcoords->push_back(curVec - v1 - v2);
      types->push_back(curType);
      fcoords->push_back(curVec - v2 - v3);
      types->push_back(curType);
      fcoords->push_back(curVec - v1 - v3);
      types->push_back(curType);
      fcoords->push_back(curVec - v1 - v2 - v3);
      types->push_back(curType);
      continue;
    }

    // Check edges
    // Recall edge definitions:
    // Edges:
    // # | pts | planes   ## | pts | planes
    // 1 | 1,2 |  2,3      7 | 3,6 |  1,5
    // 2 | 1,3 |  1,3      8 | 4,6 |  1,6
    // 3 | 1,4 |  1,2      9 | 4,7 |  2,6
    // 4 | 2,5 |  3,4     10 | 5,8 |  4,5
    // 5 | 2,7 |  2,4     11 | 6,8 |  5,6
    // 6 | 3,5 |  3,5     12 | 7.8 |  4,6

    // Edge 1
    if (nearPlane2 && nearPlane3) {
      fcoords->push_back(curVec + v2);
      types->push_back(curType);
      fcoords->push_back(curVec + v3);
      types->push_back(curType);
      fcoords->push_back(curVec + v2 + v3);
      types->push_back(curType);
      continue;
    }
    // Edge 2
    if (nearPlane1 && nearPlane3) {
      fcoords->push_back(curVec + v1);
      types->push_back(curType);
      fcoords->push_back(curVec + v3);
      types->push_back(curType);
      fcoords->push_back(curVec + v1 + v3);
      types->push_back(curType);
      continue;
    }
    // Edge 3
    if (nearPlane1 && nearPlane2) {
      fcoords->push_back(curVec + v1);
      types->push_back(curType);
      fcoords->push_back(curVec + v2);
      types->push_back(curType);
      fcoords->push_back(curVec + v1 + v2);
      types->push_back(curType);
      continue;
    }
    // Edge 4
    if (nearPlane3 && nearPlane4) {
      fcoords->push_back(curVec - v1);
      types->push_back(curType);
      fcoords->push_back(curVec + v3);
      types->push_back(curType);
      fcoords->push_back(curVec - v1 + v3);
      types->push_back(curType);
      continue;
    }
    // Edge 5
    if (nearPlane2 && nearPlane4) {
      fcoords->push_back(curVec - v1);
      types->push_back(curType);
      fcoords->push_back(curVec + v2);
      types->push_back(curType);
      fcoords->push_back(curVec - v1 + v2);
      types->push_back(curType);
      continue;
    }
    // Edge 6
    if (nearPlane3 && nearPlane5) {
      fcoords->push_back(curVec - v2);
      types->push_back(curType);
      fcoords->push_back(curVec + v3);
      types->push_back(curType);
      fcoords->push_back(curVec - v2 + v3);
      types->push_back(curType);
      continue;
    }
    // Edge 7
    if (nearPlane1 && nearPlane5) {
      fcoords->push_back(curVec + v1);
      types->push_back(curType);
      fcoords->push_back(curVec - v3);
      types->push_back(curType);
      fcoords->push_back(curVec + v1 - v3);
      types->push_back(curType);
      continue;
    }
    // Edge 8
    if (nearPlane1 && nearPlane6) {
      fcoords->push_back(curVec + v1);
      types->push_back(curType);
      fcoords->push_back(curVec - v2);
      types->push_back(curType);
      fcoords->push_back(curVec + v1 - v2);
      types->push_back(curType);
      continue;
    }
    // Edge 9
    if (nearPlane2 && nearPlane6) {
      fcoords->push_back(curVec + v2);
      types->push_back(curType);
      fcoords->push_back(curVec - v3);
      types->push_back(curType);
      fcoords->push_back(curVec + v2 - v3);
      types->push_back(curType);
      continue;
    }
    // Edge 10
    if (nearPlane4 && nearPlane5) {
      fcoords->push_back(curVec - v1);
      types->push_back(curType);
      fcoords->push_back(curVec - v2);
      types->push_back(curType);
      fcoords->push_back(curVec - v1 - v2);
      types->push_back(curType);
      continue;
    }
    // Edge 11
    if (nearPlane5 && nearPlane6) {
      fcoords->push_back(curVec - v2);
      types->push_back(curType);
      fcoords->push_back(curVec - v3);
      types->push_back(curType);
      fcoords->push_back(curVec - v2 - v3);
      types->push_back(curType);
      continue;
    }
    // Edge 12
    if (nearPlane4 && nearPlane6) {
      fcoords->push_back(curVec - v1);
      types->push_back(curType);
      fcoords->push_back(curVec - v3);
      types->push_back(curType);
      fcoords->push_back(curVec - v1 - v3);
      types->push_back(curType);
      continue;
    }

    // Check planes:
    // Plane 1:
    if (nearPlane1) {
      tmpVec1 = curVec;
      ++tmpVec1.x();
      fcoords->push_back(tmpVec1);
      types->push_back(curType);
      continue;
    }
    // Plane 2:
    if (nearPlane2) {
      tmpVec1 = curVec;
      ++tmpVec1.y();
      fcoords->push_back(tmpVec1);
      types->push_back(curType);
      continue;
    }
    // Plane 3:
    if (nearPlane3) {
      tmpVec1 = curVec;
      ++tmpVec1.z();
      fcoords->push_back(tmpVec1);
      types->push_back(curType);
      continue;
    }
    // Plane 4:
    if (nearPlane4) {
      tmpVec1 = curVec;
      --tmpVec1.x();
      fcoords->push_back(tmpVec1);
      types->push_back(curType);
      continue;
    }
    // Plane 5:
    if (nearPlane5) {
      tmpVec1 = curVec;
      --tmpVec1.y();
      fcoords->push_back(tmpVec1);
      types->push_back(curType);
      continue;
    }
    // Plane 6:
    if (nearPlane6) {
      tmpVec1 = curVec;
      --tmpVec1.z();
      fcoords->push_back(tmpVec1);
      types->push_back(curType);
      continue;
    }
  }
}

bool XtalComp::compareCurrent()
{
#ifdef XTALCOMP_DEBUG
  DEBUG_DIV;
  DEBUG_STRING("Comparing current...");
#endif

  const double tolSquared = m_lengthtol * m_lengthtol;

  // Get references to rx1's cart coords and types
  const std::vector<XcVector> &rx1_ccoords = m_rx1->ccoords();
  const std::vector<unsigned int> &rx1_types = m_rx1->types();
  assert (rx1_ccoords.size() == rx1_types.size());

  // Set up references to rx2's working transformed cartesian
  // coordinates
  const std::vector<XcVector> &rx2_ccoords = m_transformedCCoords;
  const std::vector<unsigned int> &rx2_types = m_transformedTypes;
  assert(rx2_ccoords.size() == rx2_types.size());

  bool rx1AtomMatched;
  XcVector rx1_xformedCoord;
  XcVector diffVec;

  // Iterate through all atoms in rx1
  for (size_t rx1Ind = 0; rx1Ind < rx1_types.size(); ++rx1Ind) {
    const unsigned int &rx1_type = rx1_types[rx1Ind];
    const XcVector &rx1_ccoord = rx1_ccoords[rx1Ind];
    rx1AtomMatched = false;

#ifdef XTALCOMP_DEBUG
    DEBUG_STRING("Rx1 atom:");
    DEBUG_ATOM(rx1_type, rx1_ccoord);
#endif

    // convert rx1_ccoord to the transformed cell's basis:
    rx1_xformedCoord = m_transformedFMat * rx1_ccoord;

    // Wrap to cell
    if ((rx1_xformedCoord[0] = fmod(rx1_xformedCoord[0], 1.0)) < 0) ++rx1_xformedCoord[0];
    if ((rx1_xformedCoord[1] = fmod(rx1_xformedCoord[1], 1.0)) < 0) ++rx1_xformedCoord[1];
    if ((rx1_xformedCoord[2] = fmod(rx1_xformedCoord[2], 1.0)) < 0) ++rx1_xformedCoord[2];

    // convert back to a cartesian coordinate
    rx1_xformedCoord = m_transformedCMat * rx1_xformedCoord;

#ifdef XTALCOMP_DEBUG
    DEBUG_STRING("Rx1 atom, wrapped to Rx2's cell:");
    DEBUG_ATOM(rx1_type, rx1_xformedCoord);
#endif

    // Iterate through all atoms in rx2
    for (size_t rx2Ind = 0; rx2Ind < rx2_types.size(); ++rx2Ind) {
      // If the types don't match, move to the next rx2 atom
      const unsigned int &rx2_type = rx2_types[rx2Ind];
#ifdef XTALCOMP_DEBUG
      printf("Rx2 type: %d\n", rx2_type);
#endif
      if (rx1_type != rx2_type) {
        continue;
      }

      // If the coordinates don't match, move to the next rx2 atom
      const XcVector &rx2_ccoord = rx2_ccoords[rx2Ind];
#ifdef XTALCOMP_DEBUG
      DEBUG_STRING("Rx2 coords:");
      DEBUG_VECTOR(rx2_ccoord);
#endif
      diffVec = rx1_xformedCoord - rx2_ccoord;
#ifdef XTALCOMP_DEBUG
      DEBUG_STRING("Diffvec:");
      DEBUG_VECTOR(diffVec);
#endif
      // Compare distance squared to squared length tolerance
      if (diffVec.squaredNorm() > tolSquared) {
        continue;
      }

      // Otherwise, the atoms match. move to next atom
      rx1AtomMatched = true;
      break;
    }
    // If the current rx1Atom was not matched, fail:
    if (!rx1AtomMatched) {
#ifdef XTALCOMP_DEBUG
      DEBUG_STRING("Not a match.");
      DEBUG_DIV;
#endif
      return false;
    }

    // otherwise move to next rx1Atom:
#ifdef XTALCOMP_DEBUG
    DEBUG_STRING("Atom matched!");
    DEBUG_DIV;
#endif
    continue;
  }

  // If we make it here, all of the atoms had a match. Return success.
#ifdef XTALCOMP_DEBUG
  DEBUG_STRING("Structure matched!");
  DEBUG_DIV;
#endif
  return true;
}
