// B3-T3 (C++): designs round-trip through disk with stable hashes.
#include <doctest/doctest.h>

#include <filesystem>

#include "structura/design.hpp"
#include "structura/occ.hpp"

using namespace structura;
namespace fs = std::filesystem;

TEST_CASE("B3-T3: sha256 known answers") {
  CHECK(sha256_hex("") == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
  CHECK(sha256_hex("abc") == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
}

TEST_CASE("B3-T3: design hashes are stable and change with any input") {
  DesignGraph g = load_family("bracket");
  Design a(g, {{"thickness", 5.0}});
  CHECK(a.hash() == Design(g, {{"thickness", 5.0000000001}}).hash());
  CHECK(a.hash() != Design(g, {{"thickness", 5.1}}).hash());
  DesignGraph g2 = g;
  g2.version = "99";
  CHECK(a.hash() != Design(g2, {{"thickness", 5.0}}).hash());
}

TEST_CASE("B3-T3: design round trip and rebuild") {
  DesignGraph g = load_family("bracket");
  Design a(g, {{"thickness", 7}, {"rib_count", 2}});
  fs::path dir = fs::temp_directory_path() / "structura_test_design";
  fs::remove_all(dir);
  a.save(dir.string());
  Design b = Design::load(dir.string());
  CHECK(b.hash() == a.hash());
  CHECK(b.params == a.params);
  CHECK(volume(b.build().shape) == doctest::Approx(volume(a.build().shape)).epsilon(1e-9));
  fs::remove_all(dir);
}
