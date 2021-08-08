/*!
 * \file CFEMStandardQuadAdjacentHexSol.hpp
 * \brief Class for the FEM standard surface quadrilateral element
 *        adjacent to a hexahedron for the solution.
 *        The functions are in the <i>CFEMStandardQuadAdjacentHexSol.cpp</i> file.
 * \author E. van der Weide
 * \version 7.1.1 "Blackbird"
 *
 * SU2 Project Website: https://su2code.github.io
 *
 * The SU2 Project is maintained by the SU2 Foundation
 * (http://su2foundation.org)
 *
 * Copyright 2012-2020, SU2 Contributors (cf. AUTHORS.md)
 *
 * SU2 is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * SU2 is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with SU2. If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once 

#include "CFEMStandardHexBase.hpp"
#include "CGemmFaceHex.hpp"

/*!
 * \class CFEMStandardQuadAdjacentHexSol.
 * \brief Class which defines the variables and methods for the quadrilateral
 *        standard surface element adjacent to a hexahedron for the solution.
 * \author E. van der Weide
 * \version 7.1.1 "Blackbird"
 */
class CFEMStandardQuadAdjacentHexSol final: public CFEMStandardHexBase {

public:

  /*-----------------------------------------------------------------------------------*/
  /*---                     Constructors and destructors.                           ---*/
  /*-----------------------------------------------------------------------------------*/

  /*!
   * \brief Default constructor of the class, deleted to make sure the
   *        overloaded constructor is always used.
   */
  CFEMStandardQuadAdjacentHexSol() = delete;

  /*!
   * \overload
   * \param[in] val_nPoly       - Polynomial degree of the grid for this element.
   * \param[in] val_orderExact  - Polynomial degree that must be integrated exactly.
   * \param[in] val_faceID_Elem - This is the face ID of the adjacent volume element
   *                              to which this surface element corresponds.
   * \param[in] val_orientation - Orientation of this surface element relative to
   *                              the adjacent volume element.
   * \param[in] val_gemm_1      - Pointer to the gemm type used to carry out the
   *                              first gemm functionality for this standard face.
   * \param[in] val_gemm_2      - Pointer to the gemm type used to carry out the
   *                              second gemm functionality for this standard face.
   */
  CFEMStandardQuadAdjacentHexSol(const unsigned short val_nPoly,
                                 const unsigned short val_orderExact,
                                 const unsigned short val_faceID_Elem,
                                 const unsigned short val_orientation,
                                 CGemmBase           *val_gemm_1,
                                 CGemmBase           *val_gemm_2);

  /*!
   * \brief Destructor. Nothing to be done.
   */
  ~CFEMStandardQuadAdjacentHexSol() = default;

  /*-----------------------------------------------------------------------------------*/
  /*---                  Inline public member functions.                            ---*/
  /*-----------------------------------------------------------------------------------*/

  /*!
   * \brief Function, that returns the padded number of solution DOFs of the volume element.
   * \return The padded number of solution DOFs of the adjacent volume element.
   */
  inline unsigned short GetNSolDOFsPad(void) const override {
   const unsigned short nSol = nDOFs1D*nDOFs1D*nDOFs1D;
   return PaddedValue(nSol);
  }

  /*-----------------------------------------------------------------------------------*/
  /*---                     Public member functions.                                ---*/
  /*-----------------------------------------------------------------------------------*/

  /*!
   * \brief Function, that computes the gradient of the solution in integration points.
   * \param[in]  matSolDOF     - Matrix that contains the modal solution DOFs.
   * \param[out] matGradSolInt - Vector of matrices the contains the gradients of the
   *                             solution in the integration points.
   */
  void GradSolIntPoints(ColMajorMatrix<su2double>          &matSolDOF,
                        vector<ColMajorMatrix<su2double> > &matGradSolInt) override;

  /*!
   * \brief Function that computes the solution in integration points.
   * \param[in]  matSolDOF - Matrix that contains the modal solution DOFs.
   * \param[out] matSolInt - Matrix that contains the solution in the integration points.
   */
  void SolIntPoints(ColMajorMatrix<su2double> &matSolDOF,
                    ColMajorMatrix<su2double> &matSolInt) override;

  /*!
   * \brief Function, that updates the residuals of the DOFs with the integral of the
   *        product of the given scalar data and the basis function. The integral is
   *        approximated by the weighted sum of the data in the integration points.
   * \param[in]     scalarDataInt - The scalar data in the integration points that must
   *                                be multiplied by the basis functions.
   * \param[in,out] resDOFs       - The residual of the DOFs that must be updated.
   */
  void ResidualBasisFunctions(ColMajorMatrix<su2double> &scalarDataInt,
                              ColMajorMatrix<su2double> &resDOFs) override;

private:

  unsigned short faceID_Elem;       /*!< \brief Face ID of the adjacent quad, which corresponds to this face. */
  unsigned short orientation;       /*!< \brief Orientation of this face relative to the adjacent quad. */
  bool           swapTangInTensor;  /*!< \brief Whether or not the tangential directions of the face, created
                                                in the the tensor product, must be swapped. */

  CGemmFaceHex *gemmDOFs2Int = nullptr; /*!< \brief Pointer to the gemm type used to to compute the data in the
                                                    integration points of the face from the volume DOFs. */
  CGemmFaceHex *gemmInt2DOFs = nullptr; /*!< \brief Pointer to the gemm type used to to compute the data in the
                                                    volume DOFs from the integration points of the face. */

  vector<ColMajorMatrix<passivedouble> > tensorSol;     /*!< \brief The three 1D components of the tensor to compute
                                                                    the solution on the face of the hexahedron. */
  vector<ColMajorMatrix<passivedouble> > tensorDSolDr;  /*!< \brief The three 1D components of the tensor to compute
                                                                    the derivative in r-direction of the solution
                                                                    on the face of the hexahedron. */
  vector<ColMajorMatrix<passivedouble> > tensorDSolDs;  /*!< \brief The three 1D components of the tensor to compute
                                                                    the derivative in s-direction of the solution
                                                                    on the face of the hexahedron. */
  vector<ColMajorMatrix<passivedouble> > tensorDSolDt;  /*!< \brief The three 1D components of the tensor to compute
                                                                    the derivative in t-direction of the solution
                                                                    on the face of the hexahedron. */

  vector<ColMajorMatrix<passivedouble> > tensorSolTranspose; /*!< \brief The three 1D components of the transpose of
                                                                         tensorSol. Needed to compute the residuals
                                                                         of the volume DOFs from the surface fluxes. */
};