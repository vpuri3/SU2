/*!
 * \file linear_solvers_structure.hpp
 * \brief Headers for the classes related to linear solvers (CG, FGMRES, etc)
 *        The subroutines and functions are in the <i>linear_solvers_structure.cpp</i> file.
 * \author J. Hicken, F. Palacios, T. Economon
 * \version 7.0.4 "Blackbird"
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

#include "../mpi_structure.hpp"

#include <cmath>
#include <vector>
#include <iostream>
#include <cstdlib>
#include <iomanip>
#include <string>

#include "CSysVector.hpp"

class CConfig;
class CGeometry;
template<class T> class CSysMatrix;
template<class T> class CMatrixVectorProduct;
template<class T> class CPreconditioner;

using namespace std;

/*--- Relative tolerance, target residual is tol*||b-Ax||,
 *    Absolute tolerance, target residual is tol*||b||. ---*/
enum class LinearToleranceType {RELATIVE, ABSOLUTE};

/*!
 * \class CSysSolve
 * \brief Class for solving linear systems using classical and Krylov-subspace iterative methods
 * \author J. Hicken.
 *
 * The individual solvers could be stand-alone subroutines, but by
 * creating CSysSolve objects we can more easily assign different
 * matrix-vector products and preconditioners to different problems
 * that may arise in a hierarchical solver (i.e. multigrid).
 *
 * The methods of this class are designed to be called by multiple OpenMP threads.
 * Beware of writes to class member variables, for example "Residual" should only
 * be modified by one thread.
 */
template<class ScalarType>
class CSysSolve {

public:
  /*--- Some typedefs for simplicity ---*/
  typedef CSysVector<ScalarType> VectorType;
  typedef CSysMatrix<ScalarType> MatrixType;
  typedef CMatrixVectorProduct<ScalarType> ProductType;
  typedef CPreconditioner<ScalarType> PrecondType;

private:

  bool mesh_deform;          /*!< \brief Operate in mesh deformation mode, changes the source of solver options. */
  ScalarType Residual=1e-20; /*!< \brief Residual at the end of a call to Solve or Solve_b. */
  unsigned long Iterations=0;/*!< \brief Iterations done in Solve or Solve_b. */

  mutable bool cg_ready;     /*!< \brief Indicate if memory used by CG is allocated. */
  mutable bool bcg_ready;    /*!< \brief Indicate if memory used by BCGSTAB is allocated. */
  mutable bool gmres_ready;  /*!< \brief Indicate if memory used by FGMRES is allocated. */
  mutable bool smooth_ready; /*!< \brief Indicate if memory used by SMOOTHER is allocated. */

  mutable VectorType r;      /*!< \brief Residual in CG and BCGSTAB. */
  mutable VectorType A_x;    /*!< \brief Result of matrix-vector product in CG and BCGSTAB. */
  mutable VectorType p;      /*!< \brief Direction in CG and BCGSTAB. */
  mutable VectorType z;      /*!< \brief Preconditioned residual/direction in CG/BCGSTAB. */

  mutable VectorType r_0;    /*!< \brief The "arbitrary" vector in BCGSTAB. */
  mutable VectorType v;      /*!< \brief BCGSTAB "v" vector (v = A * M^-1 * p). */

  mutable vector<VectorType> W;  /*!< \brief Large matrix used by FGMRES, w^i+1 = A * z^i. */
  mutable vector<VectorType> Z;  /*!< \brief Large matrix used by FGMRES, preconditioned W. */

  VectorType  LinSysSol_tmp;        /*!< \brief Temporary used when it is necessary to interface between active and passive types. */
  VectorType  LinSysRes_tmp;        /*!< \brief Temporary used when it is necessary to interface between active and passive types. */
  VectorType* LinSysSol_ptr;        /*!< \brief Pointer to appropriate LinSysSol (set to original or temporary in call to Solve). */
  const VectorType* LinSysRes_ptr;  /*!< \brief Pointer to appropriate LinSysRes (set to original or temporary in call to Solve). */

  LinearToleranceType tol_type = LinearToleranceType::RELATIVE; /*!< \brief How the linear solvers interpret the tolerance. */

  /*!
   * \brief sign transfer function
   * \param[in] x - value having sign prescribed
   * \param[in] y - value that defined the sign
   *
   * this may already be defined as a global function somewhere, if
   * so, feel free to delete this and replace it as needed with the
   * appropriate global function
   */
  static inline ScalarType Sign(ScalarType x, ScalarType y) {
    if (y == 0.0) return 0.0;
    return fabs(x) * (y < 0.0 ? -1.0 : 1.0);
  }

  /*!
   * \brief applys a Givens rotation to a 2-vector
   * \param[in] s - sine of the Givens rotation angle
   * \param[in] c - cosine of the Givens rotation angle
   * \param[in,out] h1 - first element of 2x1 vector being transformed
   * \param[in,out] h2 - second element of 2x1 vector being transformed
   */
  void ApplyGivens(ScalarType s, ScalarType c, ScalarType & h1, ScalarType & h2) const;

  /*!
   * \brief generates the Givens rotation matrix for a given 2-vector
   * \param[in,out] dx - element of 2x1 vector being transformed
   * \param[in,out] dy - element of 2x1 vector being set to zero
   * \param[in,out] s - sine of the Givens rotation angle
   * \param[in,out] c - cosine of the Givens rotation angle
   *
   * Based on givens() of SPARSKIT, which is based on p.202 of
   * "Matrix Computations" by Golub and van Loan.
   */
  void GenerateGivens(ScalarType & dx, ScalarType & dy, ScalarType & s, ScalarType & c) const;

  /*!
   * \brief finds the solution of the upper triangular system Hsbg*x = rhs
   *
   * \param[in] n - size of the reduced system
   * \param[in] Hsbg - upper triangular matrix
   * \param[in] rhs - right-hand side of the reduced system
   * \param[out] x - solution of the reduced system
   *
   * \pre the upper Hessenberg matrix has been transformed into a
   * triangular matrix.
   */
  void SolveReduced(int n, const vector<vector<ScalarType> > & Hsbg,
                    const vector<ScalarType> & rhs, vector<ScalarType> & x) const;

  /*!
   * \brief Modified Gram-Schmidt orthogonalization
   * \author Based on Kesheng John Wu's mgsro subroutine in Saad's SPARSKIT
   *
   * \param[in] i - index indicating which vector in w is being orthogonalized
   * \param[in,out] Hsbg - the upper Hessenberg begin updated
   * \param[in,out] w - the (i+1)th vector of w is orthogonalized against the
   *                    previous vectors in w
   *
   * \pre the vectors w[0:i] are orthonormal
   * \post the vectors w[0:i+1] are orthonormal
   *
   * Reothogonalization is performed if the cosine of the angle between
   * w[i+1] and w[k], k < i+1, is greater than 0.98.  The norm of the "new"
   * vector is kept in nrm0 and updated after operating with each vector
   *
   */
  void ModGramSchmidt(int i, vector<vector<ScalarType> > & Hsbg, vector<VectorType> & w) const;

  /*!
   * \brief writes header information for a CSysSolve residual history
   * \param[in] solver - string describing the solver
   * \param[in] restol - the target tolerance to solve to
   * \param[in] resinit - the initial residual norm (absolute)
   *
   * \pre the ostream object os should be open
   */
  void WriteHeader(string solver, ScalarType restol, ScalarType resinit) const;

  /*!
   * \brief writes residual convergence data for one iteration to a stream
   * \param[in] iter - current iteration
   * \param[in] res - the residual norm to display
   *
   * \pre the ostream object os should be open
   */
  void WriteHistory(unsigned long iter, ScalarType res) const;

  /*!
   * \brief writes final residual convergence information
   * \param[in] solver - string describing the solver
   * \param[in] iter - current iteration
   * \param[in] res - the residual norm
   */
  void WriteFinalResidual(string solver, unsigned long iter, ScalarType res) const;

  /*!
   * \brief writes the convergence warning
   * \param[in] res_calc - the residual norm computed iteratively
   * \param[in] res_true - the recomputed residual norm
   * \param[in] tol - the residual norm
   */
  void WriteWarning(ScalarType res_calc, ScalarType res_true, ScalarType tol) const;

  /*!
   * \brief Used by Solve for compatibility between passive and active CSysVector, see specializations.
   * \param[in] LinSysRes - Linear system residual
   * \param[in,out] LinSysSol - Linear system solution
   */
  void HandleTemporariesIn(const CSysVector<su2double> & LinSysRes, CSysVector<su2double> & LinSysSol);

  /*!
   * \brief Used by Solve for compatibility between passive and active CSysVector, see specializations.
   * \param[out] LinSysSol - Linear system solution
   */
  void HandleTemporariesOut(CSysVector<su2double> & LinSysSol);

public:

  /*!
   * \brief default constructor of the class.
   * \param[in] mesh_deform_mode - true, to let CSysSolve know it is in a mesh deformation context
   */
  CSysSolve(const bool mesh_deform_mode = false);

  /*! \brief Conjugate Gradient method
   * \param[in] b - the right hand size vector
   * \param[in,out] x - on entry the intial guess, on exit the solution
   * \param[in] mat_vec - object that defines matrix-vector product
   * \param[in] precond - object that defines preconditioner
   * \param[in] tol - tolerance with which to solve the system
   * \param[in] m - maximum size of the search subspace
   * \param[out] residual - final normalized residual
   * \param[in] monitoring - turn on priting residuals from solver to screen.
   * \param[in] config - Definition of the particular problem.
   */
  unsigned long CG_LinSolver(const VectorType & b, VectorType & x, const ProductType & mat_vec,
                             const PrecondType & precond, ScalarType tol, unsigned long m,
                             ScalarType & residual, bool monitoring, CConfig *config) const;

  /*!
   * \brief Flexible Generalized Minimal Residual method
   * \param[in] b - the right hand size vector
   * \param[in,out] x - on entry the intial guess, on exit the solution
   * \param[in] mat_vec - object that defines matrix-vector product
   * \param[in] precond - object that defines preconditioner
   * \param[in] tol - tolerance with which to solve the system
   * \param[in] m - maximum size of the search subspace
   * \param[out] residual - final normalized residual
   * \param[in] monitoring - turn on priting residuals from solver to screen.
   * \param[in] config - Definition of the particular problem.
   */
  unsigned long FGMRES_LinSolver(const VectorType & b, VectorType & x, const ProductType & mat_vec,
                                 const PrecondType & precond, ScalarType tol, unsigned long m,
                                 ScalarType & residual, bool monitoring, CConfig *config) const;

  /*!
   * \brief Biconjugate Gradient Stabilized Method (BCGSTAB)
   * \param[in] b - the right hand size vector
   * \param[in,out] x - on entry the intial guess, on exit the solution
   * \param[in] mat_vec - object that defines matrix-vector product
   * \param[in] precond - object that defines preconditioner
   * \param[in] tol - tolerance with which to solve the system
   * \param[in] m - maximum size of the search subspace
   * \param[out] residual - final normalized residual
   * \param[in] monitoring - turn on priting residuals from solver to screen.
   * \param[in] config - Definition of the particular problem.
   */
  unsigned long BCGSTAB_LinSolver(const VectorType & b, VectorType & x, const ProductType & mat_vec,
                                  const PrecondType & precond, ScalarType tol, unsigned long m,
                                  ScalarType & residual, bool monitoring, CConfig *config) const;

  /*!
   * \brief Generic smoother (modified Richardson iteration with preconditioner)
   * \param[in] b - the right hand size vector
   * \param[in,out] x - on entry the intial guess, on exit the solution
   * \param[in] mat_vec - object that defines matrix-vector product
   * \param[in] precond - object that defines preconditioner
   * \param[in] tol - tolerance with which to solve the system
   * \param[in] m - maximum number of iterations
   * \param[out] residual - final normalized residual
   * \param[in] monitoring - turn on priting residuals from solver to screen.
   * \param[in] config - Definition of the particular problem.
   */
  unsigned long Smoother_LinSolver(const VectorType & b, VectorType & x, const ProductType & mat_vec,
                                   const PrecondType & precond, ScalarType tol, unsigned long m,
                                   ScalarType & residual, bool monitoring, CConfig *config) const;

  /*!
   * \brief Solve the linear system using a Krylov subspace method
   * \param[in] Jacobian - Jacobian Matrix for the linear system
   * \param[in] LinSysRes - Linear system residual
   * \param[in,out] LinSysSol - Linear system solution
   * \param[in] geometry -  Geometrical definition of the problem.
   * \param[in] config - Definition of the particular problem.
   */
  unsigned long Solve(MatrixType & Jacobian, const CSysVector<su2double> & LinSysRes, CSysVector<su2double> & LinSysSol,
                      CGeometry *geometry, CConfig *config);

  /*!
   * \brief Solve the adjoint linear system using a Krylov subspace method
   * \param[in] Jacobian - Jacobian Matrix for the linear system
   * \param[in] LinSysRes - Linear system residual
   * \param[in,out] LinSysSol - Linear system solution
   * \param[in] geometry -  Geometrical definition of the problem.
   * \param[in] config - Definition of the particular problem.
   */
  unsigned long Solve_b(MatrixType & Jacobian, const CSysVector<su2double> & LinSysRes, CSysVector<su2double> & LinSysSol,
                        CGeometry *geometry, CConfig *config);

  /*!
   * \brief Get the number of iterations.
   * \return The number of iterations done by Solve or Solve_b
   */
  inline unsigned long GetIterations(void) const { return Iterations; }

  /*!
   * \brief Get the final residual.
   * \return The residual at the end of Solve or Solve_b
   */
  inline ScalarType GetResidual(void) const { return Residual; }

  /*!
   * \brief Set the type of the tolerance for stoping the linear solvers (RELATIVE or ABSOLUTE).
   */
  inline void SetToleranceType(LinearToleranceType type) {tol_type = type;}

};
