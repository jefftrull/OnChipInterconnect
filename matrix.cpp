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
#include <boost/numeric/odeint.hpp>

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
        std::cerr << "moment 0=\n" << block_moments[0] << std::endl;
        std::cerr << "moment 1=\n" << block_moments[1] << std::endl;

        // Now regularize.  Results are of dynamic (not initially known) size
        MatrixXd Greg, Creg;             // regularized versions of C and G
        Matrix<double, Dynamic, 2> Breg, Lreg;
        std::tie(Greg, Creg, Breg, Lreg) = regularize(G, C, B, L);

        // Finally, put in a form suitable for simulation, by transforming
        // C*dX/dt = -G*X + B*u
        // into
        // dX/dt   = -C.inv()*G*X + C.inv()*B*u
        // Wikipedia says Cholesky decomposition "roughly twice as efficient" as LU:
        assert(canLDLTDecompose(Creg));            // make sure we can use it
        drift_  = Creg.ldlt().solve(-1.0 * Greg);  // -Creg.inv()*Greg
        input_  = Creg.ldlt().solve(Breg);         // Creg.inv()*Breg
        output_ = Lreg.transpose();

    }

    // perform dX/dt calculation for ODEInt
    typedef std::vector<double> state_t;
    void operator()(state_t const& x, state_t& dxdt, double) const {
        using namespace Eigen;
        // need to wrap std::vector state types for Eigen to use
        Map<const Matrix<double, Dynamic, 1>> xvec(x.data(), x.size());
        Map<Matrix<double, Dynamic, 1>>       result(dxdt.data(), x.size());

        // simulating step function at time 0 for simplicity
        Matrix<double, 2, 1> u; u << 1.0, 0.0;   // aggressor voltage 1V, victim quiescent
        result = drift_ * xvec + input_ * u;
    }
        
    // turns internal state into output by applying transformed L matrix
    std::vector<double> state2output(state_t const& x) const {
        using namespace Eigen;
        std::vector<double> result(2);
        Map<const Matrix<double, Dynamic, 1> > xvec(x.data(), x.size());
        Map<Matrix<double, 2, 1> >             ovec(result.data());
        ovec = output_ * xvec;
        return result;
    }

    size_t statecnt() const {
        return drift_.rows();
    }

private:
    Eigen::Matrix<double, Eigen::Dynamic, 2>              input_;
    Eigen::Matrix<double, Eigen::Dynamic, Eigen::Dynamic> drift_;
    Eigen::Matrix<double, 2, Eigen::Dynamic>             output_;
};

int main() {
    using namespace std;

    // instantiate circuit
    coupling_circuit_t coupling_test;

    // simulate
    typedef coupling_circuit_t::state_t state_t;
    state_t         x(coupling_test.statecnt(), 0.0);   // initial conditions = all zero

    using boost::numeric::odeint::integrate;

    integrate( coupling_test, x, 0.0, 1e-9, 1e-12,
               [&](state_t const& x, double t) {
                   auto outputs = coupling_test.state2output(x);
                   cout << t << " " << outputs[0] << " " << outputs[1] << endl;
               });
}
