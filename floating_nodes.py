#!/usr/bin/python
# Demonstration of analyzing RC trees as graphs
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

# requires "graph_tool" and "quantities" libraries
from graph_tool.all import *
import quantities as pq

# Create a special graph for circuits: undirected, with special gnd node with no out edges

class CktGraph(Graph):
    "storage for circuits"

    # Monkey patch out_edges method for ground node
    def __gnd_out_edges(self):
        return []

    def __init__(self):
        Graph.__init__(self, directed=False)
        self.vname = self.new_vertex_property("string")
        self.gnd = self.add_node("gnd")
        self.gnd.out_edges = self.__gnd_out_edges
        self.ecomp = self.new_edge_property("object")

    def add_node(self, name):
        v = self.add_vertex()
        self.vname[v] = name
        return v

    def add_comp(self, n1, n2, value):
        self.ecomp[self.add_edge(n1, n2)] = value
        
# filtering graph for resistor edges
class isResistor:
    def __init__(self, g):
        self.graph = g

    def __call__(self, e):
        return self.graph.ecomp[e].units == pq.ohm

if __name__ == "__main__":
    # floating node testcase
    float_n = CktGraph()
    d1 = float_n.add_node("d1")
    n2 = float_n.add_node("n2")
    n3 = float_n.add_node("n3")

    kohm = pq.Quantity(1e3,    "ohm")
    ff   = pq.Quantity(1e-15,  "farad")
    float_n.add_comp(d1, n2, kohm)
    n1 = float_n.add_node("n1")
    float_n.add_comp(d1, n1, ff)     # node n1 floats
    float_n.add_comp(n1, n2, ff)
    float_n.add_comp(n2, n3, kohm)
    n4 = float_n.add_node("n4")
    float_n.add_comp(n3, n4, ff)     # n4 floats
    n5 = float_n.add_node("n5")
    float_n.add_comp(n4, n5, kohm)   # n5 floats despite being resistor-connected

    # second driver
    d2 = float_n.add_node("d2")
    n6 = float_n.add_node("n6")
    float_n.add_comp(d2, n6, kohm)
    # n6 couples to n4 of the other net, but that doesn't stop n4 from floating
    float_n.add_comp(n6, n4, ff)

    # Create filtered graph
    fg = GraphView(float_n, efilt=isResistor(float_n))

    # Run the algorithm
    comp, hist = label_components(fg)

    # Find the components containing d1, d2, and gnd
    driven_components = set()
    driven_components.add(comp[d1])
    driven_components.add(comp[d2])
    driven_components.add(comp[float_n.gnd])

    print "\n".join(["node %s is undriven" % float_n.vname[n] for n in fg.vertices()
                     if comp[n] not in driven_components])
    
