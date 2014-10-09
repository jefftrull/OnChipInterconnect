// Demonstration of representing/analyzing circuits as MNA matrices
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

#include <iostream>

#include "ckt_matrix.h"

// Create my standard 2-signal coupling testcase
struct coupling_circuit_t {
    coupling_circuit_t() {
        // MNA
        // MNA - we will have 10 state variables:
        // 8 for node voltages (vagg, n1, n2, n3, vvic, n5, n6, n7)
        // 2 for input currents (iagg, ivic)
        using namespace Eigen;
        typedef Matrix<double, 10, 10> state_matrix_t;
        state_matrix_t G, C;   // conductance and time derivative matrices
        G = state_matrix_t::Zero();
        C = state_matrix_t::Zero();

        double const kohm = 1000;
        double const ff   = 1e-15;
        double rdrv       = 0.1*kohm;
        double pi_r       = 1.0*kohm;
        double pi_c       = 100*ff;
        double coupl_c    = 100*ff;
        double rcvr_c     = 20*ff;

        stamp_i(G, 0, 8);                // aggressor driver current
        stamp(G, 0, 1, 1.0 / rdrv);      // aggressor driver impedance
        stamp(C, 1,    pi_c / 2);        // begin first "pi"
        stamp(G, 1, 2, 1.0 / pi_r);
        stamp(C, 2,    pi_c / 2);        // central node
        stamp(C, 2,    pi_c / 2);        // second "pi"
        stamp(G, 2, 3, 1.0 / pi_r);
        stamp(C, 3,    pi_c / 2);
        stamp(C, 3,    rcvr_c);          // aggressor receiver

        stamp_i(G, 4, 9);                // victim driver current
        stamp(G, 4, 5, 1.0 / rdrv);      // victim driver impedance
        stamp(C, 5,    pi_c / 2);        // begin first "pi"
        stamp(G, 5, 6, 1.0 / pi_r);
        stamp(C, 6,    pi_c / 2);        // central node
        stamp(C, 6,    pi_c / 2);        // second "pi"
        stamp(G, 6, 7, 1.0 / pi_r);
        stamp(C, 7,    pi_c / 2);
        stamp(C, 7,    rcvr_c);          // victim receiver

        stamp(C, 2, 6, coupl_c);         // coupling cap

        // We also have 2 inputs, two outputs
        typedef Matrix<double, 10, 2> io_matrix_t;
        io_matrix_t B = io_matrix_t::Zero();
        B(8, 0) = -1;                    // connect input 0 to vagg
        B(9, 1) = -1;                    // connect input 1 to vvic
        io_matrix_t L = io_matrix_t::Zero();
        L(3, 0) = 1;                     // connect agg rcvr to output 0
        L(7, 1) = 1;                     // connect vic rcvr to output 1

        // cross-check: compute moments
        Matrix<double, 2, 2> E = Matrix<double, 2, 2>::Zero();   // feedthrough term we don't have
        auto block_moments = moments(G, C, B, L, E, 2);
        std::cout << "moment 0=\n" << block_moments[0] << std::endl;
        std::cout << "moment 1=\n" << block_moments[1] << std::endl;
    }
};

int main() {
    coupling_circuit_t coupling_test;
}
