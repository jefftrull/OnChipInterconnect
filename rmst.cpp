// Demonstration of estimating an RC network using MST
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
#include <vector>

#include <boost/iterator/counting_iterator.hpp>
#include <boost/graph/prim_minimum_spanning_tree.hpp>
#include <boost/property_map/function_property_map.hpp>

#include "ckt_graph.h"

// Define an implicit graph class
struct pin_distance_graph {
    // For Prim we must model "Vertex List Graph" and "Incidence Graph"
    // Graph requirements
    typedef size_t vertex_descriptor;
    typedef std::pair<size_t, size_t> edge_descriptor;
    typedef boost::undirected_tag directed_category;
    typedef boost::disallow_parallel_edge_tag edge_parallel_category;
    // inherit traversal from both concepts
    struct traversal_category : virtual public boost::vertex_list_graph_tag,
                                virtual public boost::incidence_graph_tag {};

    // Vertex List Graph requirements
    typedef boost::counting_iterator<size_t> vertex_iterator;
    typedef size_t vertices_size_type;

    // Incidence Graph requirements
    // The most complicated part is generating new edges
    // We need to make a standard conforming edge iterator
    // The Boost.Iterator library can help us make a "forward iterator"
    struct  edge_iterator_t : boost::iterator_facade<edge_iterator_t,
                                                     edge_descriptor,
                                                     boost::forward_traversal_tag,
                                                     edge_descriptor const&> {
        // Implementation constrained by two things:
        // 1) We need to provide a default constructor, so sentinel cannot reference graph
        // 2) We need to provide a non-const dereference operator for Prim, so must keep
        //    a true edge descriptor around

        edge_iterator_t()
            : g_(nullptr) {}  // initialize to invalid
        edge_iterator_t(pin_distance_graph const* g, vertex_descriptor u)
            : current_edge_(std::make_pair(u, 0)), g_(g) {
            advance_to_legal();
        }
    private:
        friend class boost::iterator_core_access;
        // iterator_facade requirements:
        void increment() {
            current_edge_.second++;
            advance_to_legal();
        }
        bool equal(edge_iterator_t const& other) const {
            // true if both invalid, or they match source/dest
            if (invalid()) {
                return other.invalid();
            } else {
                return !other.invalid() && (current_edge_ == other.current_edge_);
            }
        }
        edge_descriptor const& dereference() const {
            return current_edge_;
        }
        // implementation details for above:
        bool invalid() const {
            return ((g_ == nullptr) || (current_edge_.second >= num_vertices(*g_)));
        }
        void advance_to_legal() {
            // ensure target and source are not the same
            if (current_edge_.first == current_edge_.second) {
                current_edge_.second++;
            }
        }
        edge_descriptor current_edge_;
        pin_distance_graph const* g_;
    };
                                                     
    typedef edge_iterator_t out_edge_iterator;
    typedef size_t degree_size_type;

    // Vertex List Graph free functions
    friend std::pair<vertex_iterator, vertex_iterator>
    vertices(pin_distance_graph const& g) {
        return std::make_pair(vertex_iterator(0), vertex_iterator(g.points_.size()));
    }

    friend vertices_size_type num_vertices(pin_distance_graph const& g) {
        return g.points_.size();
    }

    // Incidence Graph free functions
    friend vertex_descriptor source(edge_descriptor const& e, pin_distance_graph const&) {
        return e.first;
    }

    friend vertex_descriptor target(edge_descriptor const& e, pin_distance_graph const&) {
        return e.second;
    }

    friend std::pair<out_edge_iterator, out_edge_iterator>
    out_edges(vertex_descriptor u, pin_distance_graph const& g) {
        return std::make_pair(edge_iterator_t(&g, u), edge_iterator_t());
    }

    friend degree_size_type
    out_degree(vertex_descriptor, pin_distance_graph const& g) {
        return g.points_.size() - 1;  // "complete" graph, every node connected to every other
    }
        
    // Main functionality
    typedef std::pair<int, int> point_t;
    template<typename PtIter>
    pin_distance_graph(PtIter beg, PtIter end) : points_(beg, end) {}

    // Provide bracket operator a la adjacency_list
    point_t operator[](vertex_descriptor u) const {
        return points_[u];
    }

private:
    std::vector<point_t> points_;

};

// Run Prim on it
// Prim seems vertex-focused, as opposed to the edge-focused Kruskal
// since our edges are implicit I favored the former

int main() {
    using namespace boost;
    using namespace std;

    // Data for implicit graph - a list of points
    vector<pair<int, int>> pinlocs =
        {{-100, -100}, {-100, 100}, {0, 0}, {100, 100}, {100, -100},
         {-50, 0}, {103, 100}, {100, 90}};

    pin_distance_graph pdg(pinlocs.begin(), pinlocs.end());

    // Create Prim requirements (temporary data structures used in algorithm)

    // vertex index map.  Since we use size_t for vertex descriptors, they
    // are immediately usable as indices:
    auto vindex_map = typed_identity_property_map<size_t>();

    // Predecessor map
    vector<pin_distance_graph::vertex_descriptor> predvec(num_vertices(pdg));   // underlying storage
    auto predpmap = make_iterator_property_map(predvec.begin(), vindex_map);
    
    // Weight Map
    // Another case where we can be "implicit" - the weight of an edge is
    // the Manhattan distance between the vertices
    typedef pin_distance_graph::edge_descriptor edge_t;
    auto weightpmap = make_function_property_map<edge_t>(
        [&pdg](edge_t e) -> int {
            auto coord1 = pdg[source(e, pdg)];
            auto coord2 = pdg[target(e, pdg)];
            return ((coord1.first - coord2.first) * (coord1.first - coord2.first)) +
                ((coord1.second - coord2.second) * (coord1.second - coord2.second));
        });    

    // call Prim
    prim_minimum_spanning_tree(pdg, predpmap,
                               weight_map(weightpmap).
                               vertex_index_map(vindex_map));

    // produce output as SVG
    cout << "<svg xmlns=\"http://www.w3.org/2000/svg\"" << endl;
    cout << "     xmlns:xlink=\"http://www.w3.org/1999/xlink\">" << endl;
    auto vitpair = vertices(pdg);
    for (auto v : make_iterator_range(vitpair.first, vitpair.second)) {
        // SVG wants positive numbers, and the Y coordinate is canvas style (reversed from Cartesian)
        // so we will hack the numbers a bit to make it match our inputs:
        // scale by 3X, mirror Y axis, and add 600 to both
        int x2 = 400 + 2*pdg[v].first;
        int y2 = 400 - 2*pdg[v].second;
        if (predvec[v] == v) {
            // Root.  Make a red circle
            cout << "    <circle cx=\"" << x2 << "\" cy=\"" << y2 << "\" r=\"10\" style=\"fill:#cc0000\"/>" << endl;
        } else {
            // Not Root: a gray one
            cout << "    <circle cx=\"" << x2 << "\" cy=\"" << y2 << "\" r=\"10\" style=\"fill:#cccccc; stroke:#222222\"/>" << endl;
        }
    }
    // Go back and add lines on top of the circles (for visibility)
    for (auto v : make_iterator_range(vitpair.first, vitpair.second)) {
        int x2 = 400 + 2*pdg[v].first;
        int y2 = 400 - 2*pdg[v].second;
        if (predvec[v] != v) {
            int x1 = 400 + 2*pdg[predvec[v]].first;
            int y1 = 400 - 2*pdg[predvec[v]].second;
            // Make a line to the successor node
            cout << "    <line x1=\"" << x1 << "\" y1=\"" << y1 << "\" x2=\"" << x2 << "\" y2=\"" << y2 << "\" style=\"stroke:#666666; stroke-width:3px\"/>" << endl;
        }
    }
    cout << "</svg>" << endl;

    // Future work: turn resulting tree into an implicit graph compatible with ckt_graph_t,
    // with estimated RC values on edges

}
