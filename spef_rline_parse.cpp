// A very small portion of a SPEF parser
// just does resistor lines

/*
Copyright (c) 2016 Jeffrey E. Trull

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

#include <boost/spirit/include/qi.hpp>
#include <boost/fusion/include/adapt_struct.hpp>

struct resistor_t {
    unsigned     idx;
    unsigned     net1;
    std::string  node1;
    unsigned     net2;
    std::string  node2;
    double       value;
};

BOOST_FUSION_ADAPT_STRUCT(
    resistor_t,
    (unsigned,    idx)
    (unsigned,    net1)
    (std::string, node1)
    (unsigned,    net2)
    (std::string, node2)
    (double,      value)
)    


int main() {
    using namespace std;

    string testspef(
        "*RES\n"
        "1 *1087:4 *223:B 1.2\n"
        "2 *1087:3 *1087:4 3.12\n"
    );

    using namespace boost::spirit::qi;
    auto beg = testspef.begin();
    rule<string::iterator,
         resistor_t(),
         ascii::space_type> rline =
        uint_ >>
        "*" >> uint_ >> ":" >> +ascii::alnum >>
        "*" >> uint_ >> ":" >> +ascii::alnum >>
        double_ ;
        
    vector<resistor_t> resistors;
    phrase_parse(
        beg, testspef.end(),
        lit("*RES") >> *rline,
        ascii::space,
        resistors);

    for (auto const& r : resistors) {
        cout << r.value << " from " << r.net1 << ":" << r.node1;
        cout            << " to "   << r.net2 << ":" << r.node2 << "\n";
    }
}

