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

// Units are handy for remembering branch components
#include <boost/units/systems/si/resistance.hpp>
#include <boost/units/systems/si/capacitance.hpp>
#include <boost/units/systems/si/prefixes.hpp>
#include <boost/units/systems/si/io.hpp>

#include <boost/variant.hpp>

// Graph library for storing full RC networks
#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/graph_utility.hpp>
#include <boost/property_map/property_map.hpp>
#include <boost/graph/filtered_graph.hpp>
#include <boost/graph/visitors.hpp>
#include <boost/graph/undirected_dfs.hpp>

// Define vertex and edge properties

// vertex (circuit node) properties just a string containing the name
typedef std::string vertex_property_t;

// edge (circuit component) properties are the type (resistor or capacitor) with the component value
using namespace boost::units;
typedef quantity<si::resistance, double> resistor_edge_t;
typedef quantity<si::capacitance, double> capacitor_edge_t;
typedef boost::variant<resistor_edge_t, capacitor_edge_t> edge_property_t;

// the circuit itself
typedef boost::adjacency_list<boost::vecS, boost::vecS, boost::undirectedS,
                              vertex_property_t, edge_property_t> ckt_graph_t;

// for use in algorithms:
template <typename Graph>
struct loop_detect_visitor : boost::default_dfs_visitor {
    typedef typename boost::graph_traits<Graph>::edge_descriptor edge_t;
    typedef typename boost::graph_traits<Graph>::vertex_descriptor vertex_t;

    void tree_edge(edge_t e, Graph const& g) {
        // remember predecessor
        predecessors.emplace(target(e, g), source(e, g));
    }

    void back_edge(edge_t e, Graph const& g) {
        using namespace std;
        cout << "cycle detected: " << g[target(e, g)];
        for (auto v = source(e, g); v != target(e, g); v = predecessors.at(v)) {
            cout << "->" << g[v];
        }
        cout << "->" << g[target(e, g)] << endl;
    }

private:
    std::map<vertex_t, vertex_t> predecessors;
};

// filter predicate for resistor-only graphs
struct is_resistor : boost::static_visitor<bool> {
  bool operator()(resistor_edge_t const&) const {
    return true;
  }
  bool operator()(capacitor_edge_t const&) const {
    return false;
  }
};

struct resistors_only {
    resistors_only() {}
    resistors_only(ckt_graph_t const* g) : graph_(g) {}
    bool operator()(ckt_graph_t::edge_descriptor e) const {
        // access edge property (res/cap variant) and apply predicate visitor
        return boost::apply_visitor(is_resistor(), (*graph_)[e]);
    }
private:
    ckt_graph_t const* graph_;   // cannot be ref due to default ctor requirement
};


int main() {
    using namespace boost;
    using namespace std;

    // Build some interesting circuits
    quantity<si::resistance> kohm(1.0 * units::si::kilo  * units::si::ohms);
    quantity<si::capacitance> ff(1.0 * units::si::femto * units::si::farads);

    // First, one with a resistor loop
    ckt_graph_t r_loop;
    {
        auto gnd = add_vertex("gnd", r_loop);
        auto n1  = add_vertex("n1", r_loop);
        auto n2  = add_vertex("n2", r_loop);     // starting point for resistor loop
        add_edge(n1, n2, 100.*kohm, r_loop);
        add_edge(n2, gnd, 10.*ff, r_loop);
        auto n3  = add_vertex("n3", r_loop);     // ending point of loop
        add_edge(n2, n3, 2.71*kohm, r_loop);     // loop branch 1
        auto n2a  = add_vertex("n2a", r_loop);
        add_edge(n2, n2a, 3.14*kohm, r_loop);    // loop branch 2
        add_edge(n2a, n3, 1.*kohm, r_loop);      // loop branch 2
        add_edge(n3, gnd, 10.*ff, r_loop);
    }

    // perform DFS with loop visitor on filtered (resistor-only) graph
    typedef filtered_graph<ckt_graph_t, resistors_only> resonly_graph_t;
    resistors_only res_filter(&r_loop);
    filtered_graph<ckt_graph_t, resistors_only> res_graph(r_loop, res_filter);
    loop_detect_visitor<resonly_graph_t> findloops;

    // supply edge color map for algorithm
    map<ckt_graph_t::edge_descriptor, default_color_type> edge_colors;
    auto cpmap = make_assoc_property_map(edge_colors);

    undirected_dfs(res_graph,
                   edge_color_map(cpmap)
                  .visitor(findloops));

}
