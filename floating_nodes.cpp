// Demonstration of analyzing RC trees as graphs
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

#include <boost/graph/connected_components.hpp>

#include "ckt_graph.h"

int main() {
    using namespace boost;
    using namespace std;

    // Build some interesting circuits
    quantity<si::resistance> kohm(1.0 * units::si::kilo  * units::si::ohms);
    quantity<si::capacitance> ff(1.0 * units::si::femto * units::si::farads);

    // A circuit with floating nodes (one dangling, one shared between two other nodes)
    ckt_graph_t float_n;

    auto d1  = add_vertex("d1", float_n);
    auto n2  = add_vertex("n2", float_n);
    auto n3  = add_vertex("n3", float_n);
        
    // we will model two "nets" here to demonstrate a more complex situation
    // a first driver
    add_edge(d1, n2, kohm, float_n);   // initial resistor
    // in parallel with d1->n2, two capacitors in series
    auto n1 = add_vertex("n1", float_n);
    add_edge(d1, n1, ff, float_n);   // node n1 floats
    add_edge(n1, n2, ff, float_n);
    add_edge(n2, n3, kohm, float_n);
    auto n4 = add_vertex("n4", float_n);
    add_edge(n3, n4, ff, float_n);   // n4 floats
    auto n5 = add_vertex("n5", float_n);
    add_edge(n4, n5, kohm, float_n); // n5 floats despite being resistor-connected

    // second driver
    auto d2 = add_vertex("d2", float_n);
    auto n6 = add_vertex("n6", float_n);
    add_edge(d2, n6, kohm, float_n);
    // n6 couples to n4 of the other net, but that doesn't stop n4 from floating
    add_edge(n6, n4, ff, float_n);

    // Floating nodes are unreachable via resistors from gnd or any driver.  That is, they
    // are either disconnected from any edge, or have at least one capacitor in
    // series between themselves and any driver.
    // For a single net, running DFS from the driver on a filtered resistor-only network,
    // and noting any unreachable nodes would do.
    // For a complex graph with multiple nets, this scales poorly.  The "connected components"
    // algorithm is the answer.  We run it on a resistor-only graph;  any nodes not in the same
    // component as a driver or gnd are floating.

    // run connected components on filtered (resistor-only) graph
    typedef filtered_graph<ckt_graph_t, resistors_only> resonly_graph_t;
    resonly_graph_t res_graph(float_n, resistors_only(&float_n));

   // define required color and component property maps
   typedef resonly_graph_t::vertices_size_type comp_number_t;
   typedef map<resonly_graph_t::vertex_descriptor, comp_number_t> CCompsStorageMap;
   CCompsStorageMap comps;      // component map for algorithm results
   typedef map<resonly_graph_t::vertex_descriptor, default_color_type> ColorsStorageMap;
   ColorsStorageMap colors;     // temp storage for algorithm
   
   // adapt std::map for use by Graph algorithms as "property map"
   boost::associative_property_map<CCompsStorageMap> cpmap(comps);
   boost::associative_property_map<ColorsStorageMap> clrpmap(colors);

   // Run the algorithm
   connected_components(res_graph, cpmap, boost::color_map(clrpmap));
    
   // Find the components relating to d1, d2, and gnd
   set<comp_number_t> driven_components;
   for (auto comp_entry : comps) {
       if ((float_n[comp_entry.first] == "d1") ||
           (float_n[comp_entry.first] == "d2") ||
           (comp_entry.first == float_n.gnd())) {
           driven_components.insert(comp_entry.second);
       }
   }
   for (auto comp_entry : comps) {
       if (driven_components.find(comp_entry.second) == driven_components.end()) {
           std::cout << "node " << float_n[comp_entry.first] << " is undriven" << std::endl;
       }
   }

}
