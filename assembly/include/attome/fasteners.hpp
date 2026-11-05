// Fasteners (Level 2.2): ISO metric bolts, their design resistances, and the loads on a bolt group.
//
// Resistances follow EN 1993-1-8 (Eurocode 3, joints), Table 3.4, for bolts in tension and shear:
//   Ft,Rd = k2 fub As / gM2,  k2 = 0.9 (0.63 for countersunk heads)
//   Fv,Rd = av fub As / gM2,  av = 0.6 for grades 4.6, 5.6, 8.8; 0.5 for 4.8, 5.8, 6.8, 10.9 (shear plane through the thread)
//   combined: Fv,Ed / Fv,Rd + Ft,Ed / (1.4 Ft,Rd) <= 1,  gM2 = 1.25
// Thread data (stress area As) from ISO 898-1. A bolt's grade is stated in the assembly; when it is not, the lowest
// common grade (4.6) is assumed, because an unstated value may only make the check stricter (decision D9).
//
// A bolt set into a wall plug fails first in the wall, not in the steel: plug pull-out depends on the plug and the
// masonry, and is checked only against a datasheet value given in the assembly ("pullout_kN"); otherwise it is
// reported as not checked, never assumed.
#pragma once
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "attome/assembly.hpp"

namespace attome {

struct Thread {
  double d, pitch, As;  // nominal diameter, coarse pitch, tensile stress area (mm, mm, mm^2)
};
struct Grade {
  std::string name;
  double fub, fyb;  // ultimate and yield strength (MPa)
  double alpha_v;   // shear factor, thread in the shear plane
};

Thread iso_thread(double d);           // coarse thread M3..M30; throws for other sizes
Grade bolt_grade(const std::string& name);  // "4.6", "4.8", "5.6", "5.8", "6.8", "8.8", "10.9", "12.9"

struct Resistance {
  double Ft_Rd, Fv_Rd;  // N
};
Resistance en1993_resistance(const Thread& t, const Grade& g, double gamma_M2 = 1.25);

// Interaction ratio of EN 1993-1-8 Table 3.4 (<= 1 passes).
double interaction(double Fv_Ed, double Ft_Ed, const Resistance& r);

// The bolts that hold a part to a fixed wall, checked for the weight of everything that rests on it.
// Loads: every part with "mass_kg" in the assembly description (index matches `parts`) whose weight passes through
// the part the bolts hold. Statics of a wall plate bolted with horizontal bolts, pivoting about its lowest edge:
//   moment M = W e (e = horizontal distance from the wall face to the centre of the load)
//   bolt tension T_i = M (z_i - z_p) / sum_j (z_j - z_p)^2,  shear V_i = W / n
nlohmann::json wall_bolt_check(const std::vector<Part>& parts, const nlohmann::json& description);

}  // namespace attome
