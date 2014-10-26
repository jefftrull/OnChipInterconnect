#!/usr/bin/env python
# Demonstration of finding resistor loops via DFS with visitor
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

from graph_tool.search import dfs_search, DFSVisitor
from graph_tool import GraphView
import quantities as pq

from ckt_graph import CktGraph, isResistor

# DFS Visitor for identifying loops
class visitor(DFSVisitor):
    def __init__(self, g):
        self.predecessor = g.new_vertex_property("object")
        self.graph = g

    def tree_edge(self, e):
        # this is a "normal" edge being added to the DFS tree
        # store predecessor info for error reporting
        self.predecessor[e.target()] = e.source()

    def back_edge(self, e):
        # filter out tree edges before reporting error
        if self.predecessor[e.source()] == e.target():
            # already seen as a tree edge
            # (this happens because we have two edges for every component, one in each direction)
            return

        # use predecessor map to report the nodes involved in the loop
        print "cycle detected: " + self.graph.vname[e.target()],
        v = e.source()
        while (v != e.target()):
            sys.stdout.write("->" + self.graph.vname[v])
            v = self.predecessor[v]
        print "->" + self.graph.vname[e.target()]

if __name__ == "__main__":
    kohm = pq.Quantity(1e3,    "ohm")
    ff   = pq.Quantity(1e-15,  "farad")

    # construct graph with a resistive loop
    r_loop = CktGraph()
    gnd  = r_loop.gnd

    n1  = r_loop.add_node("n1")
    n2  = r_loop.add_node("n2")         # starting point for resistor loop
    r_loop.add_comp(n1, n2, 100.*kohm)
    r_loop.add_comp(n2, gnd, 10.*ff)
    n3  = r_loop.add_node("n3")         # ending point of loop
    r_loop.add_comp(n2, n3, 2.71*kohm)  # loop branch 1
    n2a  = r_loop.add_node("n2a")
    r_loop.add_comp(n2, n2a, 3.14*kohm) # loop branch 2
    r_loop.add_comp(n2a, n3, 1.*kohm)   # loop branch 2
    r_loop.add_comp(n3, gnd, 10.*ff)

    # construct filtered graph with capacitors removed
    fg = GraphView(r_loop, efilt=isResistor(r_loop))

    # run DFS with special visitor to report loops
    dfs_search(r_loop, n1, visitor(r_loop))
