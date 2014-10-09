// Demonstration of calculating Elmore delay with a graph
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

#include <boost/graph/visitors.hpp>
#include <boost/graph/undirected_dfs.hpp>
#include <boost/units/systems/si/time.hpp>

#include "ckt_graph.h"

// For Elmore we need two visitors:
// 1) To calculate total capacitance at and "below" each node
// 2) To sum resistor delays to each node from a chosen input

template<typename Graph>
struct cap_summing_visitor : boost::default_dfs_visitor {
    typedef typename boost::graph_traits<Graph>::edge_descriptor edge_t;
    typedef typename boost::graph_traits<Graph>::vertex_descriptor vertex_t;

    // This edge variant visitor returns the correct contribution for an edge
    // to a downstream node: the value on the edge, if a capacitor, or
    // the supplied downstream capacitance, if a resistor.
    struct cap_summer : boost::static_visitor<capacitor_value_t> {
        capacitor_value_t operator()(resistor_value_t const&) const {
            return downstream_;   // count downstream, not edge
        }
        capacitor_value_t operator()(capacitor_value_t const& c) const {
            return c;             // count edge but not downstream (i.e., lump to gnd)
        }
        cap_summer(capacitor_value_t downstream) : downstream_(downstream) {}
    private:
        capacitor_value_t downstream_;
    };

    void tree_edge(edge_t e, Graph const& g) {
        // remember the predecessor so later we can avoid summing up capacitance from back edges
        predecessors_[target(e, g)] = source(e, g);
    }

    void finish_vertex(vertex_t u, Graph const& g) {
        // We want to sum only *downstream* capacitance
        // However, circuits are undirected, and the downstreamness of our search
        // is only due to the order in which we encountered vertices
        // a "tree edge" is downstream, and a "back edge" is upstream

        downstream_caps_[u] = capacitor_value_t();
        for (auto e : boost::make_iterator_range(out_edges(u, g))) {
            // detect back edge, i.e., an edge to an already-visited node
            // this won't work correctly for circuits with resistive loops
            if ((predecessors_[target(e, g)] == u) || (target(e, g) == g.gnd())) {
                // our first time at this node, OR the node is gnd, in which case this is normal
                capacitor_value_t contr = boost::apply_visitor(cap_summer(downstream_caps_[target(e, g)]), g[e]);
                downstream_caps_[u] += contr;
            }
        }
    }

    // finish_edge never seems to get called

    cap_summing_visitor(std::vector<capacitor_value_t>& capsvec)
        : downstream_caps_(capsvec), predecessors_(capsvec.size()) {}

private:
    std::vector<capacitor_value_t>& downstream_caps_;
    std::vector<vertex_t> predecessors_;
};

typedef quantity<si::time> delay_t;

// adds up delays.  To be run on filtered (R-only) graph
template<typename Graph>
struct delay_calculating_visitor : boost::default_dfs_visitor {
    typedef typename boost::graph_traits<Graph>::edge_descriptor edge_t;
    typedef typename boost::graph_traits<Graph>::vertex_descriptor vertex_t;

    void start_vertex(vertex_t u, Graph const&) {
        // this hook is called once at the beginning;
        // we can use it to initialize the delay from the tree root
        delays_[u] = delay_t();
    }

    struct res_summer : boost::static_visitor<resistor_value_t> {
        resistor_value_t operator()(resistor_value_t const& r) const {
            return r;
        }
        resistor_value_t operator()(capacitor_value_t const&) const {
            assert(0);   // program bug - we should operate on a filtered graph
            return resistor_value_t();
        }
    };

    void tree_edge(edge_t e, Graph const& g) {
        delays_[target(e, g)] = delays_[source(e, g)] +
            boost::apply_visitor(res_summer(), g[e]) *
            downstream_caps_[target(e, g)];
    }

    void back_edge(edge_t, Graph const&) {
        // Resistive loops will break this algorithm - you could put a check here
    }

    delay_calculating_visitor(std::vector<capacitor_value_t>& capsvec,
                              std::vector<delay_t>& delays)
        : downstream_caps_(capsvec), delays_(delays) {}

private:
    std::vector<capacitor_value_t>& downstream_caps_;   // bottom-up
    std::vector<delay_t>&          delays_;            // top-down
};

int main() {
    using namespace boost;
    using namespace std;

    quantity<si::resistance> kohm(1.0 * units::si::kilo  * units::si::ohms);
    quantity<si::capacitance> ff(1.0 * units::si::femto * units::si::farads);

    // Coupling test case
    ckt_graph_t coupling_test;
    auto gnd = coupling_test.gnd();

    auto vagg = add_vertex("vagg", coupling_test);   // driver voltage source
    auto n1   = add_vertex("n1",   coupling_test);
    add_edge(vagg, n1, 0.1*kohm,   coupling_test);   // driver impedance
    auto n2   = add_vertex("n2",   coupling_test);   // central node - where coupling occurs
    add_edge(n1, n2, 1.0*kohm,     coupling_test);   // first "pi" model, aggressor side
    add_edge(n1, gnd, 50.0*ff,     coupling_test);   // caps for first "pi"
    add_edge(n2, gnd, 50.0*ff,     coupling_test);
    auto n3   = add_vertex("n3",   coupling_test);   // aggressor-side receiver
    add_edge(n2, n3, 1.0*kohm,     coupling_test);   // second "pi" model, aggressor side
    add_edge(n2, gnd, 50.0*ff,     coupling_test);   // caps for first "pi"
    add_edge(n3, gnd, 50.0*ff,     coupling_test);
    add_edge(n3, gnd, 20.0*ff,     coupling_test);   // aggressor side receiver load

    // then the same thing over again for the victim side
    auto vvic = add_vertex("vvic", coupling_test);   // driver voltage source
    auto n5   = add_vertex("n5",   coupling_test);
    add_edge(vvic, n5, 0.1*kohm,   coupling_test);   // driver impedance
    auto n6   = add_vertex("n6",   coupling_test);   // central node
    add_edge(n5, n6, 1.0*kohm,     coupling_test);   // first "pi" model
    add_edge(n5, gnd, 50.0*ff,     coupling_test);
    add_edge(n6, gnd, 50.0*ff,     coupling_test);
    auto n7   = add_vertex("n7",   coupling_test);   // victim-side receiver
    add_edge(n6, n7, 1.0*kohm,     coupling_test);   // second "pi" model
    add_edge(n6, gnd, 50.0*ff,     coupling_test);
    add_edge(n7, gnd, 50.0*ff,     coupling_test);
    add_edge(n7, gnd, 20.0*ff,     coupling_test);   // victim side receiver load

    // coupling capacitor between the two signal traces
    add_edge(n2, n6, 100.0*ff,     coupling_test);

    // calculate Elmore delay
    // Create visitors
    vector<capacitor_value_t> downstream_caps(num_vertices(coupling_test));
    cap_summing_visitor<ckt_graph_t> capvis(downstream_caps);

    // undirected DFS requires edge color map
    map<ckt_graph_t::edge_descriptor, default_color_type> edge_colors;
    auto cpmap = make_assoc_property_map(edge_colors);
    
    // perform first pass and sum capacitances
    undirected_dfs(coupling_test, edge_color_map(cpmap).visitor(capvis).root_vertex(vagg));

    typedef filtered_graph<ckt_graph_t, resistors_only> resonly_graph_t;
    resonly_graph_t res_graph(coupling_test, resistors_only(&coupling_test));

    vector<delay_t>          delays(num_vertices(coupling_test));
    delay_calculating_visitor<resonly_graph_t>
                                     delvis(downstream_caps, delays);

    // create *vertex* color map for depth_first_visit
    auto vindex_map = typed_identity_property_map<size_t>();
    vector<default_color_type> colorvec(num_vertices(coupling_test));   // underlying storage
    auto cvpmap = make_iterator_property_map(colorvec.begin(), vindex_map);

    depth_first_visit(res_graph, vagg, delvis, cvpmap);

    cout << "Elmore delay of aggressor net: " << delays.at(n3) << endl;

    // If you want to calculate the delay from a different node you have to recalculate
    // both capacitance and delays starting with that new node
}
