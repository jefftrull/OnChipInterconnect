# Graph representation of RC circuits
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

import graph_tool
import quantities as pq

# Create a special graph for circuits: undirected, with special gnd node with no out edges

class CktGraph(graph_tool.Graph):
    "storage for circuits"

    # Monkey patch out_edges method for ground node
    def __gnd_out_edges(self):
        return []

    def __init__(self):
        graph_tool.Graph.__init__(self, directed=False)
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

