// Run an external program (gmsh, ccx) as a separate process with a timeout. GPL tools are only ever used this way.
#pragma once
#include <map>
#include <string>
#include <vector>

namespace structura {

struct ProcessResult {
  int exit_code = -1;
  bool timed_out = false;
  std::string output;  // stdout and stderr combined
  double seconds = 0;
};

ProcessResult run_process(const std::vector<std::string>& args, const std::string& cwd, double timeout_s,
                          const std::map<std::string, std::string>& env = {});

// Locate an executable: $STRUCTURA_<NAME>, then the conda prefix (Library/bin or bin), then PATH.
std::string find_exe(const std::string& name);

}  // namespace structura
