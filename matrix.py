#!/usr/bin/python
# Demonstration of representing/analyzing circuits as MNA matrices
# to accompany "Analyzing On-Chip Interconnect with Modern C++"
# but using Python!
# Author: Jeff Trull <edaskel@att.net>

# Copyright (c) 2014 Jeffrey E. Trull
# 
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
# 
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
# 
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.

import sys
import numpy as np
import scipy as sp
import scipy.linalg
import scipy.integrate

# functions to construct MNA matrices

# stamp conductance between "i" and "j" nodes
# (resistor -> G, or capacitor -> C)
def stamp(m, i, *args):
    # args can be either:
    if len(args) == 1:
        # for a lumped-to-ground conductance
        m[i, i] += args[0]
    else:
        # OR node2, conductance
        j = args[0]
        g = args[1]
        m[i, i] += g
        m[j, j] += g
        m[i, j] -= g
        m[j, i] -= g

# stamp a voltage source connection into G matrix
def stamp_i(m, v, i):
    m[v, i] = 1
    m[i, v] = -1

# Analysis functions
def isSingular(m):
    if m.shape[0] != m.shape[1]:
        raise ValueError('Singularity test only works on square matrices')

    return np.linalg.matrix_rank(m) != m.shape[0]  # not "full rank"

def moments(G, C, B, L, N):
    # calculate the first N moments of the system
    if isSingular(G):
        raise ValueError('Supplied G matrix is singular, cannot invert')

    A = np.linalg.solve(-G, C)   # runs G decomposition twice - can be optimized
    R = np.linalg.solve(G, B)

    return [np.matrix.transpose(L).dot(np.linalg.matrix_power(A, i)).dot(R)
            for i in xrange(0, N)]

def regularize(G, C, B, L):

    # 1. Generate permutation matrix to move zero rows to the bottom
    zero_rows = np.all(C == 0, axis=1)

    P  = np.identity(10)
    j  = 9
    i  = 0
    while i < j:
        # search for an exchange point
        while i < 10 and not zero_rows[i]:
            i += 1
        while (j > 0) and zero_rows[j]:
            j -= 1
        if i < j:
            P[:, [i, j]] = P[:, [j, i]]  # exchange columns
            i += 1
            j -= 1
            
    # 2. Apply permutation to MNA matrices
    CP = P.dot(C).dot(P)     # P * C * P
    GP = P.dot(G).dot(P)
    BP = P.dot(B)
    LP = P.dot(L)

    # 3. Produce reduced equations following Su (Proc. 15th ASP-DAC, 2002)
    nonzero_count = sum(1 for r in zero_rows if not r)
    G11 = GP[0:nonzero_count, 0:nonzero_count]   # upper left
    G12 = GP[0:nonzero_count, nonzero_count:10]  # upper right
    G21 = GP[nonzero_count:10, 0:nonzero_count]  # upper left
    G22 = GP[nonzero_count:10, nonzero_count:10] # upper left

    L1  = LP[0:nonzero_count]
    L2  = LP[nonzero_count:10]

    B1  = BP[0:nonzero_count]
    B2  = BP[nonzero_count:10]

    Creg = CP[0:nonzero_count, 0:nonzero_count]

    if isSingular(G22):
        raise ValueError("Cannot invert G22")
    g22lu     = sp.linalg.lu_factor(G22)        # create LU object for reuse
    G22invG21 = sp.linalg.lu_solve(g22lu, G21)
    G22invB2  = sp.linalg.lu_solve(g22lu, B2)
    Greg      = G11 - G12.dot(G22invG21)

    Lreg      = (L1.transpose() - L2.transpose().dot(G22invG21)).transpose()
    Breg      = B1 - G12.dot(G22invB2)

    return (Greg, Creg, Breg, Lreg)

def func(y, t0):
    # derivative is linear to produce a parabolic output
    return [t0]

class mna_ckt_fun(object):
    # class to compute dX/dt given X and t
    # that's C^-1*(-G) + C^-1*B*u
    def __init__(self, G, C, B):
        # precompute C^-1*(-G) and C^-1*B
        C_lu = sp.linalg.lu_factor(C)
        self.drift  = sp.linalg.lu_solve(C_lu, -G)
        self.inTerm = sp.linalg.lu_solve(C_lu, B)

    def __call__(self, x, t):
        # return dx/dt given x and t
        u = [1.0, 0]    # vAgg step at time zero; vVic constant 0
        return self.drift.dot(x) + self.inTerm.dot(u)

if __name__ == "__main__":
    # construct C*dX/dt = -G * X + B * u via MNA

    G = np.zeros(shape=(10, 10), dtype=float)
    C = np.zeros(shape=(10, 10), dtype=float)

    kohm       = 1000.0
    ff         = 1e-15
    rdrv       = 0.1*kohm
    pi_r       = 1.0*kohm
    pi_c       = 100*ff
    coupl_c    = 100*ff
    rcvr_c     = 20*ff

    stamp_i(G, 0, 8)                # aggressor driver current
    stamp(G, 0, 1, 1.0 / rdrv)      # aggressor driver impedance
    stamp(C, 1,    pi_c / 2)        # begin first "pi"
    stamp(G, 1, 2, 1.0 / pi_r)
    stamp(C, 2,    pi_c / 2)        # central node
    stamp(C, 2,    pi_c / 2)        # second "pi"
    stamp(G, 2, 3, 1.0 / pi_r)
    stamp(C, 3,    pi_c / 2)
    stamp(C, 3,    rcvr_c)          # aggressor receiver

    stamp_i(G, 4, 9)                # victim driver current
    stamp(G, 4, 5, 1.0 / rdrv)      # victim driver impedance
    stamp(C, 5,    pi_c / 2)        # begin first "pi"
    stamp(G, 5, 6, 1.0 / pi_r)
    stamp(C, 6,    pi_c / 2)        # central node
    stamp(C, 6,    pi_c / 2)        # second "pi"
    stamp(G, 6, 7, 1.0 / pi_r)
    stamp(C, 7,    pi_c / 2)
    stamp(C, 7,    rcvr_c)          # victim receiver

    stamp(C, 2, 6, coupl_c)         # coupling cap

    # finally, construct B (voltage input to state variables)
    B = np.zeros(shape=(10, 2), dtype=float)
    B[8, 0] = -1
    B[9, 1] = -1

    # and L to extract outputs (via Y = L * X)
    L = np.zeros(shape=(10, 2), dtype=float)
    L[3, 0] = 1
    L[7, 1] = 1
    
    block_moments = moments(G, C, B, L, 2)
    print >> sys.stderr, "moment 0="
    print >> sys.stderr, str(block_moments[0])
    print >> sys.stderr, "moment 1="
    print >> sys.stderr, str(block_moments[1])

    (Greg, Creg, Breg, Lreg) = regularize(G, C, B, L)

    ckt_fun = mna_ckt_fun(Greg, Creg, Breg)

    timepoints = np.arange(0, 1e-9, 1e-12)
    xarr = sp.integrate.odeint(ckt_fun,                     # dX/dt functor
                               [0, 0, 0, 0, 0, 0],          # initial state
                               timepoints)

    for (t, x) in zip(timepoints, xarr):
        y = Lreg.transpose().dot(x)
        print t, y[0], y[1]
