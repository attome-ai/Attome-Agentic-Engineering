#include "structura/process.hpp"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>

#ifdef _WIN32
#include <windows.h>
#else
#include <csignal>
#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace fs = std::filesystem;

namespace structura {

std::string find_exe(const std::string& name) {
  std::string upper = name;
  std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);
  if (const char* e = std::getenv(("STRUCTURA_" + upper).c_str())) return e;
#ifdef _WIN32
  const std::string exe = name + ".exe";
  const char sep = ';';
#else
  const std::string exe = name;
  const char sep = ':';
#endif
  if (const char* pre = std::getenv("CONDA_PREFIX")) {
    for (auto sub : {"Library/bin", "bin"}) {
      fs::path p = fs::path(pre) / sub / exe;
      if (fs::exists(p)) return p.string();
    }
  }
  if (const char* path = std::getenv("PATH")) {
    std::stringstream ss(path);
    std::string dir;
    while (std::getline(ss, dir, sep)) {
      if (dir.empty()) continue;
      fs::path p = fs::path(dir) / exe;
      if (fs::exists(p)) return p.string();
    }
  }
  throw std::runtime_error(name + " not found (set STRUCTURA_" + upper + " or add it to PATH)");
}

#ifdef _WIN32
static std::wstring widen(const std::string& s) {
  if (s.empty()) return {};
  int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
  std::wstring w(n, 0);
  MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), w.data(), n);
  return w;
}

static std::wstring quote(const std::string& a) {
  std::wstring w = widen(a);
  if (!w.empty() && w.find_first_of(L" \t\"") == std::wstring::npos) return w;
  std::wstring q = L"\"";
  for (wchar_t c : w) {
    if (c == L'"') q += L'\\';
    q += c;
  }
  return q + L"\"";
}

ProcessResult run_process(const std::vector<std::string>& args, const std::string& cwd, double timeout_s,
                          const std::map<std::string, std::string>& env) {
  ProcessResult res;
  fs::path log = fs::path(cwd) / ".process_output.txt";
  SECURITY_ATTRIBUTES sa{sizeof(sa), nullptr, TRUE};
  HANDLE h = CreateFileW(log.wstring().c_str(), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, CREATE_ALWAYS,
                         FILE_ATTRIBUTE_NORMAL, nullptr);
  if (h == INVALID_HANDLE_VALUE) throw std::runtime_error("cannot create process log in " + cwd);
  std::wstring cmd;
  for (auto& a : args) cmd += (cmd.empty() ? L"" : L" ") + quote(a);
  // environment block: current environment plus overrides
  std::wstring block;
  {
    std::map<std::wstring, std::wstring> vars;
    LPWCH cur = GetEnvironmentStringsW();
    for (LPWCH p = cur; *p; p += wcslen(p) + 1) {
      std::wstring kv(p);
      size_t eq = kv.find(L'=', 1);
      if (eq != std::wstring::npos) vars[kv.substr(0, eq)] = kv.substr(eq + 1);
    }
    FreeEnvironmentStringsW(cur);
    for (auto& [k, v] : env) vars[widen(k)] = widen(v);
    for (auto& [k, v] : vars) block += k + L"=" + v + L'\0';
    block += L'\0';
  }
  STARTUPINFOW si{};
  si.cb = sizeof(si);
  si.dwFlags = STARTF_USESTDHANDLES;
  si.hStdOutput = h;
  si.hStdError = h;
  si.hStdInput = nullptr;
  PROCESS_INFORMATION pi{};
  auto t0 = std::chrono::steady_clock::now();
  std::wstring wcwd = widen(cwd);
  BOOL ok = CreateProcessW(nullptr, cmd.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW | CREATE_UNICODE_ENVIRONMENT,
                           block.data(), wcwd.c_str(), &si, &pi);
  if (!ok) {
    CloseHandle(h);
    throw std::runtime_error("cannot start " + args.at(0));
  }
  DWORD wait = WaitForSingleObject(pi.hProcess, static_cast<DWORD>(timeout_s * 1000));
  if (wait == WAIT_TIMEOUT) {
    TerminateProcess(pi.hProcess, 124);
    WaitForSingleObject(pi.hProcess, 5000);
    res.timed_out = true;
  }
  DWORD code = 0;
  GetExitCodeProcess(pi.hProcess, &code);
  res.exit_code = res.timed_out ? -1 : static_cast<int>(code);
  CloseHandle(pi.hProcess);
  CloseHandle(pi.hThread);
  CloseHandle(h);
  res.seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
  std::ifstream in(log, std::ios::binary);
  std::stringstream ss;
  ss << in.rdbuf();
  res.output = ss.str();
  return res;
}
#else
ProcessResult run_process(const std::vector<std::string>& args, const std::string& cwd, double timeout_s,
                          const std::map<std::string, std::string>& env) {
  ProcessResult res;
  fs::path log = fs::path(cwd) / ".process_output.txt";
  auto t0 = std::chrono::steady_clock::now();
  pid_t pid = fork();
  if (pid < 0) throw std::runtime_error("fork failed");
  if (pid == 0) {
    if (chdir(cwd.c_str()) != 0) _exit(127);
    int fd = open(log.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    dup2(fd, 1);
    dup2(fd, 2);
    for (auto& [k, v] : env) setenv(k.c_str(), v.c_str(), 1);
    std::vector<char*> argv;
    for (auto& a : args) argv.push_back(const_cast<char*>(a.c_str()));
    argv.push_back(nullptr);
    execvp(argv[0], argv.data());
    _exit(127);
  }
  int status = 0;
  while (true) {
    pid_t r = waitpid(pid, &status, WNOHANG);
    if (r == pid) break;
    if (std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count() > timeout_s) {
      kill(pid, SIGKILL);
      waitpid(pid, &status, 0);
      res.timed_out = true;
      break;
    }
    usleep(2000);
  }
  res.exit_code = res.timed_out ? -1 : (WIFEXITED(status) ? WEXITSTATUS(status) : -1);
  res.seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
  std::ifstream in(log);
  std::stringstream ss;
  ss << in.rdbuf();
  res.output = ss.str();
  return res;
}
#endif

}  // namespace structura
