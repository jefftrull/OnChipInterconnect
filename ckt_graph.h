// Circuit graph class definition
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

// Define vertex and edge properties

// vertex (circuit node) properties just a string containing the name
typedef std::string vertex_property_t;

// edge (circuit component) properties are the type (resistor or capacitor) with the component value
using namespace boost::units;
using resistor_value_t = quantity<si::resistance, double>;
using capacitor_value_t = quantity<si::capacitance, double>;
using edge_property_t = boost::variant<resistor_value_t, capacitor_value_t>;

// the circuit itself
// only difference vs. its parent adjacency_list is ground node is always defined
struct ckt_graph_t :  boost::adjacency_list<boost::vecS, boost::vecS, boost::undirectedS,
                                            vertex_property_t, edge_property_t> {
    ckt_graph_t() {
        gnd_ = add_vertex("gnd", *this);
    }
    vertex_descriptor gnd() const {
        return gnd_;
    }

private:
    vertex_descriptor gnd_;
};

// override out_edges so gnd is always a "sink"
std::pair<ckt_graph_t::out_edge_iterator, ckt_graph_t::out_edge_iterator>
out_edges(ckt_graph_t::vertex_descriptor u, ckt_graph_t const& g) {
    if (u == g.gnd()) {
        // return empty range
        return std::make_pair(ckt_graph_t::out_edge_iterator(),
                              ckt_graph_t::out_edge_iterator());
    } else {
        return boost::out_edges(u, g);
    }
}

// filter predicate for resistor-only graphs
struct is_resistor : boost::static_visitor<bool> {
  bool operator()(resistor_value_t const&) const {
    return true;
  }
  bool operator()(capacitor_value_t const&) const {
    return false;
  }
};

struct resistors_only {
    resistors_only() = default;
    resistors_only(ckt_graph_t const* g) : graph_(g) {}
    bool operator()(ckt_graph_t::edge_descriptor e) const {
        // access edge property (res/cap variant) and apply predicate visitor
        return boost::apply_visitor(is_resistor(), (*graph_)[e]);
    }
private:
    ckt_graph_t const* graph_;   // cannot be ref due to default ctor requirement
};

#endif // CKT_MATRIX_H
