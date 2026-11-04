// Internal helpers shared by the CalculiX deck writers (fea.cpp, analyses.cpp). Not part of the public API.
#pragma once
#include <array>
#include <filesystem>
#include <map>
#include <ostream>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "structura/mesh.hpp"

namespace structura::detail {

std::string num(double v);
std::string set_name(const std::string& group);
std::string read_file(const std::filesystem::path& p);
double tet_volume(const Mesh& m, const std::vector<int>& t);
extern const int kFaces[4][3];  // CalculiX C3D10 faces by local corner nodes (1-based)
std::vector<std::pair<int, int>> element_faces(const Mesh& m, const std::string& group);  // (element, face number)

void write_nodes_elements(std::ostream& d, const Mesh& m, const std::string& element_type);
std::vector<int> write_nset(std::ostream& d, const Mesh& m, const std::string& group);  // returns the node ids
void write_id_list(std::ostream& d, const std::vector<int>& ids);

}  // namespace structura::detail
