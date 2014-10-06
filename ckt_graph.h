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
typedef quantity<si::resistance, double> resistor_edge_t;
typedef quantity<si::capacitance, double> capacitor_edge_t;
typedef boost::variant<resistor_edge_t, capacitor_edge_t> edge_property_t;

// the circuit itself
typedef boost::adjacency_list<boost::vecS, boost::vecS, boost::undirectedS,
                              vertex_property_t, edge_property_t> ckt_graph_t;

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

