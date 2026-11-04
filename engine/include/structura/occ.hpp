// Small OpenCASCADE helpers shared across the engine.
#pragma once
#include <stdexcept>
#include <string>
#include <vector>

#include <Bnd_Box.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Shape.hxx>
#include <gp_Dir.hxx>
#include <gp_Pnt.hxx>

namespace structura {

std::vector<TopoDS_Face> faces_of(const TopoDS_Shape& s);   // unique faces
std::vector<TopoDS_Edge> edges_of(const TopoDS_Shape& s);   // unique edges
std::vector<TopoDS_Shape> solids_of(const TopoDS_Shape& s);

double volume(const TopoDS_Shape& s);
double area(const TopoDS_Shape& s);
gp_Pnt centre(const TopoDS_Face& f);     // centre of mass of the face
gp_Dir normal(const TopoDS_Face& f);     // outward normal at the face's UV centre, orientation-aware
bool is_valid(const TopoDS_Shape& s);
std::vector<double> bbox(const TopoDS_Shape& s, bool optimal = true);  // xmin ymin zmin xmax ymax zmax
double edge_length(const TopoDS_Edge& e);

// Turn an OCCT exception raised inside f() into std::runtime_error with its message.
std::string occ_message();

}  // namespace structura
