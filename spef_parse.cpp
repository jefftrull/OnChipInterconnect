// Demonstration of parsing parts of the SPEF interconnect parasitic format
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
#include <string>
#include <map>

#define BOOST_SPIRIT_USE_PHOENIX_V3
#include <boost/spirit/include/qi.hpp>
#include <boost/spirit/include/phoenix.hpp>

#include "ckt_graph.h"

// parse "name map" and res/cap entries
std::string name_map_text = R"NMT(
*NAME_MAP
*100 SOME/HIERARCHICAL/PATH/NAME
*101 other_name_at_top_level
)NMT";

// resistance entries
std::string res_section_text = R"RST(
*RES
1 *100:1 *100:2 3.14
2 *101:2 *100:3 2.71
*END
)RST";

struct resistor_t {
    std::string n1;
    std::string n2;
    resistor_value_t value;
};

struct ckt_builder {
    void add_component(std::string const& nname1,
                       std::string const& nname2,
                       resistor_value_t value) {
        using namespace boost;
        // map names back to vertices
        ckt_graph_t::vertex_descriptor n1, n2;
        auto n1_it = node_map_.find(nname1);
        if (n1_it == node_map_.end()) {
            n1 = add_vertex(nname1, g_);
            node_map_.emplace(nname1, n1);
        } else {
            n1 = n1_it->second;
        }
        auto n2_it = node_map_.find(nname2);
        if (n2_it == node_map_.end()) {
            n2 = add_vertex(nname2, g_);
            node_map_.emplace(nname2, n2);
        } else {
            n2 = n2_it->second;
        }
        add_edge(n1, n2, value, g_);
    }

    ckt_builder(ckt_graph_t& g) : g_(g) {}
private:
    ckt_graph_t& g_;
    std::map<std::string, ckt_graph_t::vertex_descriptor> node_map_;
};


int main() {
    using namespace std;
    using namespace boost;

    stringstream nmstream(name_map_text);
    nmstream.unsetf(ios::skipws);                  // we *do* want the whitespace
    spirit::istream_iterator nmbeg(nmstream), end; // end is a "sentinel", takes no data
    
    using spirit::lit;
    namespace phx = boost::phoenix;
    using namespace boost::spirit::qi;
    symbols<char, std::string> name_map_symtab;

    typedef rule<spirit::istream_iterator, std::string()> name_rule;
    name_rule nm_num  = '*' >> +digit;
    name_rule nm_name = +char_("a-zA-Z0-9/_");
    phrase_parse(nmbeg, end,                                    // input range
                 lit("*NAME_MAP") >>                            // grammar begins
                 *(nm_num >> nm_name)[
                     phx::bind(name_map_symtab.add, _1, _2)],   // action for line
                 space);                                        // "skipper"

    cout << "contents of name map:\n";
    name_map_symtab.for_each([name_map_symtab](std::string const& s, std::string const& d) {
            cout << s << " => " << d << endl;
        });

    ckt_graph_t parsed_circuit;
    ckt_builder builder(parsed_circuit);

    // Now parse some representative resistors using the symbol table
    quantity<si::resistance> r_unit(1.0 * units::si::kilo  * units::si::ohms);
    stringstream rstream(res_section_text);
    rstream.unsetf(ios::skipws);
    spirit::istream_iterator rbeg(rstream), rend;
    phrase_parse(rbeg, rend,
                 lit("*RES") >>
                 *(omit[uint_] >>
                   '*' >> as_string[name_map_symtab >> char_(':') >> lexeme[+alnum]] >>
                   '*' >> as_string[name_map_symtab >> char_(':') >> lexeme[+alnum]] >>
                   double_)[phx::bind(&ckt_builder::add_component, phx::ref(builder),
                                      _1, _2, _3 * phx::cref(r_unit))]
                 >> lit("*END"),
                 space);

    cout << "circuit graph edges:" << endl;
    for (auto e : make_iterator_range(edges(parsed_circuit))) {
        cout << parsed_circuit[source(e, parsed_circuit)] << " -> ";
        cout << parsed_circuit[target(e, parsed_circuit)] << " value ";
        cout << parsed_circuit[e] << endl;
    }
}
