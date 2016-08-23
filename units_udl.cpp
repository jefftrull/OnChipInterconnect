// Quick demo of using user-defined literals with electrical units

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

#include <iostream>

#include <boost/units/systems/si/resistance.hpp>
#include <boost/units/systems/si/capacitance.hpp>
#include <boost/units/systems/si/prefixes.hpp>
#include <boost/units/systems/si/io.hpp>
#include <boost/units/base_units/us/pound_force.hpp>

using boost::units::quantity;
using namespace boost::units::si;
quantity<resistance> operator"" _kohm(long double v) {
    return quantity<resistance>(v * kilo * ohms);
}

quantity<capacitance> operator"" _ff(long double v) {
    return quantity<capacitance>(v * femto * farads);
}

quantity<boost::units::si::time> operator"" _ms(long double v) {
    return quantity<boost::units::si::time>(v * milli * seconds);
}

quantity<boost::units::si::force>
operator"" _lbf(long double v) {
    using lbf = boost::units::us::pound_force_base_unit::unit_type;
    return quantity<boost::units::si::force>(v * lbf());
}

quantity<boost::units::si::force>
operator"" _N(long double v) {
    return quantity<boost::units::si::force>(v * newton);
}

int main() {
    using boost::units::si::time;
    quantity<time> t = 2._kohm * 6._ff;
    using boost::units::engineering_prefix;
    std::cout << "time constant = " << engineering_prefix << t << std :: endl ;
    auto f = 1._lbf + 1._N;
    std::cout << "force = " << f << "\n";

}


