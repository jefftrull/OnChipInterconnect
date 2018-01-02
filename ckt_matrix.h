// Circuit matrix class definition
// to accompany "Analyzing On-Chip Interconnect with Modern C++"
// Author: Jeff Trull <edaskel@att.net>

/*
Copyright (c) 2014 Jeffrey E. Trull

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/

#ifndef CKT_MATRIX_H
#define CKT_MATRIX_H

#include <Eigen/Dense>

// Functions for implementing MNA with a dense Eigen matrix
// Sparse is slightly more complex but doable in a similar way
// and is the recommended approach for medium to large circuits
template<typename M, typename Float>
void stamp(M& matrix, std::size_t i, std::size_t j, Float g)
{
    // General stamp: conductance at [i,i] and [j,j],
    // -conductance at [i,j] and [j,i], summed with existing values

    matrix(i, i) += g;
    matrix(j, j) += g;
    matrix(i, j) -= g;
    matrix(j, i) -= g;

}

// for when the other end of the device is at GND
template<typename M, typename Float>
void stamp(M& matrix, std::size_t i, Float g)
{
    matrix(i, i) += g;
}

// for voltage sources (inputs)
template<typename M>
void stamp_i(M& matrix, std::size_t vnodeno, std::size_t istateno)
{
    // just basically marks the connection between the inductor (or voltage source)
    // and the voltage, because unlike capacitance, both are state variables.

    matrix(vnodeno, istateno) = 1;   // current is taken *into* inductor or vsource
    matrix(istateno, vnodeno) = -1;
}

// Attributes of matrices
template<class M>
bool isSingular(const M& m) {

   assert(m.rows() == m.cols());   // singularity has no meaning for a non-square matrix
   return (m.fullPivLu().rank() != m.rows());   // interpretation: "not of full rank"

}

template<class M>
bool canLDLTDecompose(const M& m) {
    // m must be positive or negative semidefinite, which means its eigenvalues
    // must all be real-valued, and either all non-positive or all non-negative.
    // Use the magic of Eigen reductions to implement:
    auto eigenvalues = Eigen::EigenSolver<M>(m).eigenvalues();
    return (eigenvalues.array().imag() == 0.0).all() &&   // all real
        ((eigenvalues.array().real() >= 0.0).all() ||      // non-negative
         (eigenvalues.array().real() <= 0.0).all());       // or non-positive
}

// Convenience template for using Eigen's special allocator with vectors
template<typename Float, int nrows, int ncols>
using MatrixVector = std::vector<Eigen::Matrix<Float, nrows, ncols>,
                                 Eigen::aligned_allocator<Eigen::Matrix<Float, nrows, ncols> > >;

// Calculate moments of given system in MNA form
template<int icount, int ocount, int scount, typename Float = double>
MatrixVector<Float, ocount, icount>
moments(Eigen::Matrix<Float, scount, scount> const & G,
        Eigen::Matrix<Float, scount, scount> const & C,
        Eigen::Matrix<Float, scount, icount> const & B,
        Eigen::Matrix<Float, scount, ocount> const & L,
        Eigen::Matrix<Float, ocount, icount> const & E,
        size_t count) {
    using namespace Eigen;

    MatrixVector<Float, ocount, icount> result;

    assert(!isSingular(G));
    auto G_QR = G.fullPivHouseholderQr();
    Matrix<Float, scount, scount> A = -G_QR.solve(C);
    Matrix<Float, scount, icount> R = G_QR.solve(B);

    result.push_back(L.transpose() * R + E);   // incorporate feedthrough into first moment
    Matrix<Float, scount, scount> AtotheI = A;
    for (size_t i = 1; i < count; ++i) {
        result.push_back(L.transpose() * AtotheI * R);
        AtotheI = A * AtotheI;
    }

    return result;
}

// take a circuit's linear system description in G, C, B, L form and compress it so
// the resulting C array is non-singular.  Operation depends on runtime data, so
// output array dimensions are "Dynamic"
template<int icount, int ocount, int scount, typename Float = double>
std::tuple<Eigen::Matrix<Float, Eigen::Dynamic, Eigen::Dynamic>,   // G result
           Eigen::Matrix<Float, Eigen::Dynamic, Eigen::Dynamic>,   // C result
           Eigen::Matrix<Float, Eigen::Dynamic, icount>,    // B result
           Eigen::Matrix<Float, Eigen::Dynamic, ocount> >   // L result
regularize(Eigen::Matrix<Float, scount, scount> const & G,
           Eigen::Matrix<Float, scount, scount> const & C,
           Eigen::Matrix<Float, scount, icount> const & B,
           Eigen::Matrix<Float, scount, ocount> const & L) {

    // Use the techniques described in Su (Proc 15th ASP-DAC, 2002) to reduce
    // this set of equations so the state variable derivatives have coefficients
    // Otherwise we cannot integrate to get the time domain result...

    using namespace Eigen;

    // Use Eigen reductions to find zero rows
    auto zero_rows = (C.array() == 0.0).rowwise().all();   // per row "all zeros"
    std::size_t zero_count = zero_rows.count();
    std::size_t nonzero_count = C.rows() - zero_count;

    // 1. Generate permutation matrix to move zero rows to the bottom
    PermutationMatrix<scount, scount, std::size_t> permut;
    permut.setIdentity(C.rows());      // start with null permutation
    Eigen::Index i, j;
    for (i = 0, j=(C.rows()-1); i < j;) {
        // loop invariant: rows > j are all zero; rows < i are not
        while ((i < C.rows()) && !zero_rows(i)) ++i;
        while ((j > 0) && zero_rows(j)) --j;
        if (i < j) {
            // exchange rows i and j via the permutation vector
            permut.applyTranspositionOnTheRight(i, j);
            ++i; --j;
        }
    }

    // 2. Apply permutation to MNA matrices
    typedef Matrix<Float, scount, scount> eqn_matrix_t;
    eqn_matrix_t CP = permut * C * permut;          // permute rows and columns
    eqn_matrix_t GP = permut * G * permut;
    Matrix<Float, Dynamic, icount> BP = permut * B; // permute only rows
    Matrix<Float, Dynamic, ocount> LP = permut * L;
   
    // 3. Produce reduced equations following Su (Proc. 15th ASP-DAC, 2002)

    typedef Matrix<Float, Dynamic, Dynamic> MatrixD;
    auto G11 = GP.topLeftCorner(nonzero_count, nonzero_count);
    auto G12 = GP.topRightCorner(nonzero_count, zero_count);
    MatrixD G21 = GP.bottomLeftCorner(zero_count, nonzero_count);
    MatrixD G22 = GP.bottomRightCorner(zero_count, zero_count);

    auto L1 = LP.topRows(nonzero_count);
    auto L2 = LP.bottomRows(zero_count);

    auto B1 = BP.topRows(nonzero_count);
    auto B2 = BP.bottomRows(zero_count);

    MatrixD Cred = CP.topLeftCorner(nonzero_count, nonzero_count);

    assert(!isSingular(G22));
    auto G22QR = G22.fullPivLu();

    MatrixD G22invG21 = G22QR.solve(G21);
    auto G22invB2 = G22QR.solve(B2);
    MatrixD Gred = G11 - G12 * G22invG21;

    Matrix<Float, Dynamic, ocount> Lred = (L1.transpose() - L2.transpose() * G22invG21).transpose();
    Matrix<Float, Dynamic, icount> Bred = B1 - G12 * G22invB2;

    // This approach presumes no feedthrough (input-to-output) term
    MatrixD D = L2.transpose() * G22invB2;
    assert(D.isZero());

    return std::make_tuple(Gred, Cred, Bred, Lred);
}

#endif // CKT_MATRIX_H
