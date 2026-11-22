#ifndef FREEARC_ARC_INI_H
#define FREEARC_ARC_INI_H

#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <limits.h>
#endif

extern "C" {
  int AddExternalCompressor(char *params);
  void ClearExternalCompressorsTable(void);
}

class ArcIni {
public:
  struct DefaultRule {
    std::vector<std::string> commands;
    std::vector<std::string> options;
  };

  struct ConfigScanResult {
    std::string custom_cfg;
    bool cfg_disabled;
    bool print_config;

    ConfigScanResult() : cfg_disabled(false), print_config(false) {}
  };

  std::string filepath;
  bool loaded;
  std::vector<std::string> global_options;
  std::vector<DefaultRule> default_rules;
  std::map<std::string, std::string> compression_methods;
  std::vector<std::string> external_compressor_sections;

  ArcIni() : loaded(false) {}

  static std::string trim(const std::string &s) {
    size_t start = 0;
    while (start < s.size() && (s[start] == ' ' || s[start] == '\t' || s[start] == '\r' || s[start] == '\n'))
      start++;
    size_t end = s.size();
    while (end > start && (s[end - 1] == ' ' || s[end - 1] == '\t' || s[end - 1] == '\r' || s[end - 1] == '\n'))
      end--;
    return s.substr(start, end - start);
  }

  static std::string to_lower(const std::string &s) {
    std::string r = s;
    for (size_t i = 0; i < r.size(); i++)
      r[i] = (char)tolower((unsigned char)r[i]);
    return r;
  }

  static std::string replace_all(const std::string &str, const std::string &from, const std::string &to) {
    if (from.empty()) return str;
    std::string result;
    size_t start_pos = 0;
    size_t find_pos;
    while ((find_pos = str.find(from, start_pos)) != std::string::npos) {
      result.append(str, start_pos, find_pos - start_pos);
      result.append(to);
      start_pos = find_pos + from.length();
    }
    result.append(str, start_pos, str.length() - start_pos);
    return result;
  }

  static std::vector<std::string> tokenize(const std::string &s) {
    std::vector<std::string> tokens;
    std::string cur;
    char in_quote = 0;
    for (size_t i = 0; i < s.size(); i++) {
      char c = s[i];
      if (in_quote) {
        if (c == in_quote) {
          in_quote = 0;
        } else {
          cur += c;
        }
      } else {
        if (c == '"' || c == '\'') {
          in_quote = c;
        } else if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
          if (!cur.empty()) {
            tokens.push_back(cur);
            cur.clear();
          }
        } else {
          cur += c;
        }
      }
    }
    if (!cur.empty()) {
      tokens.push_back(cur);
    }
    return tokens;
  }

  static std::string strip_comment(const std::string &line) {
    std::string trimmed = trim(line);
    if (trimmed.empty() || trimmed[0] == ';' || (trimmed[0] == '#' && trimmed.find('=') == std::string::npos))
      return "";

    std::string clean;
    char in_quote = 0;
    for (size_t i = 0; i < line.size(); i++) {
      char c = line[i];
      if (in_quote) {
        clean += c;
        if (c == in_quote) in_quote = 0;
      } else {
        if (c == '"' || c == '\'') {
          in_quote = c;
          clean += c;
        } else if (c == ';') {
          break; // inline comment starting with ';'
        } else {
          clean += c;
        }
      }
    }
    return trim(clean);
  }

  static std::string get_executable_dir(const char *argv0) {
#ifdef _WIN32
    char buf[MAX_PATH];
    DWORD len = GetModuleFileNameA(NULL, buf, sizeof(buf) - 1);
    if (len > 0) {
      buf[len] = '\0';
      std::string s(buf);
      size_t slash = s.find_last_of("\\/");
      if (slash != std::string::npos) return s.substr(0, slash + 1);
    }
#else
    char buf[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (len > 0) {
      buf[len] = '\0';
      std::string s(buf);
      size_t slash = s.find_last_of('/');
      if (slash != std::string::npos) return s.substr(0, slash + 1);
    }
#endif
    if (argv0 && *argv0) {
      std::string s(argv0);
      size_t slash = s.find_last_of("/\\");
      if (slash != std::string::npos) return s.substr(0, slash + 1);
    }
    return "./";
  }

  static std::string find_arc_ini(const char *argv0, const std::string &custom_cfg, bool cfg_disabled) {
    if (cfg_disabled) return "";
    if (!custom_cfg.empty()) {
      return custom_cfg;
    }

    // 1. Next to executable
    std::string exe_dir = get_executable_dir(argv0);
    std::string exe_ini = exe_dir + "arc.ini";
    FILE *f = fopen(exe_ini.c_str(), "rb");
    if (f) { fclose(f); return exe_ini; }

    // 2. Current working directory
    std::string cwd_ini = "arc.ini";
    f = fopen(cwd_ini.c_str(), "rb");
    if (f) { fclose(f); return cwd_ini; }

    // 3. /etc/arc.ini on Unix
#ifndef _WIN32
    std::string etc_ini = "/etc/arc.ini";
    f = fopen(etc_ini.c_str(), "rb");
    if (f) { fclose(f); return etc_ini; }
#endif

    return "";
  }

  static ConfigScanResult scan_config_options(int argc, char **argv) {
    ConfigScanResult res;
    for (int i = 1; i < argc; i++) {
      const char *a = argv[i];
      if (!a) continue;
      if (strcmp(a, "--") == 0) break;
      if (strcmp(a, "--print-config") == 0) {
        res.print_config = true;
      } else if (strcmp(a, "-cfg-") == 0 || strcmp(a, "--config-") == 0 || strcmp(a, "--no-config") == 0) {
        res.cfg_disabled = true;
      } else if (strncmp(a, "-cfg=", 5) == 0) {
        res.custom_cfg = a + 5;
      } else if (strncmp(a, "--config=", 9) == 0) {
        res.custom_cfg = a + 9;
      } else if (strcmp(a, "-cfg") == 0 && i + 1 < argc && argv[i + 1][0] != '-') {
        res.custom_cfg = argv[++i];
      } else if (strcmp(a, "--config") == 0 && i + 1 < argc && argv[i + 1][0] != '-') {
        res.custom_cfg = argv[++i];
      } else if (strncmp(a, "-cfg", 4) == 0 && a[4] != '\0' && a[4] != '-') {
        res.custom_cfg = a + 4;
      }
    }
    return res;
  }

  bool load(const std::string &path) {
    if (path.empty()) return false;
    FILE *f = fopen(path.c_str(), "rb");
    if (!f) return false;

    filepath = path;
    enum SectionType { SEC_GLOBAL, SEC_DEFAULT_OPTIONS, SEC_COMPRESSION_METHODS, SEC_EXTERNAL, SEC_OTHER };
    SectionType sec = SEC_GLOBAL;
    std::string ext_current_sec;

    char line_buf[4096];
    while (fgets(line_buf, sizeof(line_buf), f)) {
      std::string raw_line(line_buf);
      std::string clean = strip_comment(raw_line);

      // Section header detection
      if (!clean.empty() && clean[0] == '[' && clean.find(']') != std::string::npos) {
        if (sec == SEC_EXTERNAL && !ext_current_sec.empty()) {
          external_compressor_sections.push_back(ext_current_sec);
          ext_current_sec.clear();
        }

        size_t close_bracket = clean.find(']');
        std::string section_name = trim(clean.substr(1, close_bracket - 1));
        std::string sec_lower = to_lower(section_name);

        if (sec_lower == "default options" || sec_lower == "options" || sec_lower == "general" || sec_lower == "settings") {
          sec = SEC_DEFAULT_OPTIONS;
        } else if (sec_lower == "compression methods") {
          sec = SEC_COMPRESSION_METHODS;
        } else if (sec_lower.find("external compressor:") == 0) {
          sec = SEC_EXTERNAL;
          ext_current_sec = clean + "\n";
        } else {
          sec = SEC_OTHER;
        }
        continue;
      }

      if (clean.empty() && sec != SEC_EXTERNAL)
        continue;

      if (sec == SEC_GLOBAL) {
        std::vector<std::string> opts = tokenize(clean);
        global_options.insert(global_options.end(), opts.begin(), opts.end());
      } else if (sec == SEC_DEFAULT_OPTIONS) {
        size_t eq = clean.find('=');
        if (eq != std::string::npos) {
          std::string left = trim(clean.substr(0, eq));
          std::string right = trim(clean.substr(eq + 1));
          std::string left_lower = to_lower(left);

          // Support key-value style configuration for nobitexact / bitexact:
          if (left_lower == "nobitexact" || left_lower == "max-ratio" || left_lower == "max_ratio") {
            std::string rv = to_lower(right);
            if (rv == "1" || rv == "true" || rv == "yes" || rv == "on" || rv.empty()) {
              global_options.push_back("-nobitexact");
            } else if (rv == "0" || rv == "false" || rv == "no" || rv == "off") {
              global_options.push_back("-nobitexact-");
            }
            continue;
          }
          if (left_lower == "bitexact") {
            std::string rv = to_lower(right);
            if (rv == "1" || rv == "true" || rv == "yes" || rv == "on" || rv.empty()) {
              global_options.push_back("-nobitexact-");
            } else if (rv == "0" || rv == "false" || rv == "no" || rv == "off") {
              global_options.push_back("-nobitexact");
            }
            continue;
          }
          if (left_lower == "mode") {
            std::string rv = to_lower(right);
            if (rv == "nobitexact" || rv == "max-ratio" || rv == "max_ratio") {
              global_options.push_back("-nobitexact");
            } else if (rv == "bitexact") {
              global_options.push_back("-nobitexact-");
            }
            continue;
          }

          std::vector<std::string> cmds = tokenize(to_lower(left));
          std::vector<std::string> opts = tokenize(right);
          if (!cmds.empty() && !opts.empty()) {
            DefaultRule rule;
            rule.commands = cmds;
            rule.options = opts;
            default_rules.push_back(rule);
          }
        } else if (clean[0] == '-') {
          // Bare option line like "-nobitexact" or "-mx" under [Default options]
          std::vector<std::string> opts = tokenize(clean);
          global_options.insert(global_options.end(), opts.begin(), opts.end());
        }
      } else if (sec == SEC_COMPRESSION_METHODS) {
        size_t eq = clean.find('=');
        if (eq != std::string::npos) {
          std::string alias = trim(clean.substr(0, eq));
          std::string val = trim(clean.substr(eq + 1));
          if (!alias.empty() && !val.empty()) {
            if (alias.find('#') != std::string::npos) {
              for (char d = '1'; d <= '9'; d++) {
                std::string k = replace_all(alias, "#", std::string(1, d));
                std::string v = replace_all(val, "#", std::string(1, d));
                compression_methods[to_lower(trim(k))] = trim(v);
              }
            } else {
              compression_methods[to_lower(alias)] = val;
            }
          }
        }
      } else if (sec == SEC_EXTERNAL) {
        ext_current_sec += raw_line;
      }
    }

    if (sec == SEC_EXTERNAL && !ext_current_sec.empty()) {
      external_compressor_sections.push_back(ext_current_sec);
      ext_current_sec.clear();
    }

    fclose(f);
    loaded = true;
    return true;
  }

  void register_external_compressors() const {
    if (external_compressor_sections.empty()) return;
    ClearExternalCompressorsTable();
    for (size_t i = 0; i < external_compressor_sections.size(); i++) {
      std::string sec = external_compressor_sections[i];
      AddExternalCompressor((char*)sec.c_str());
    }
  }

  static bool command_matches(const std::string &rule_cmd, char cmd, const std::string &cmd_word) {
    if (rule_cmd == "*") return true;
    std::string rc = to_lower(rule_cmd);
    std::string cw = to_lower(cmd_word);
    if (!cw.empty() && rc == cw) return true;
    char c = (char)tolower((unsigned char)cmd);
    if (c != ' ' && rc.size() == 1 && rc[0] == c) return true;
    if (rc == "arc") {
      // Matches archiver commands in arc: a, c, u, f, m, d, s
      return (c == 'a' || c == 'u' || c == 'f' || c == 'm' || c == 'd' || c == 's' || c == 'c');
    }
    if (rc == "unarc") {
      // Matches extractor commands in unarc: x, e, t, l, v, p
      return (c == 'x' || c == 'e' || c == 't' || c == 'l' || c == 'v' || c == 'p');
    }
    if (c == 'a' && (rc == "a" || rc == "add" || rc == "c" || rc == "create")) return true;
    if (c == 'x' && (rc == "x" || rc == "extract")) return true;
    if (c == 'e' && rc == "e") return true;
    if (c == 't' && (rc == "t" || rc == "test")) return true;
    if (c == 'l' && (rc == "l" || rc == "list")) return true;
    if (c == 'v' && (rc == "v" || rc == "view")) return true;
    if (c == 'u' && (rc == "u" || rc == "update")) return true;
    if (c == 'f' && (rc == "f" || rc == "freshen")) return true;
    if (c == 'm' && (rc == "m" || rc == "move")) return true;
    if (c == 'd' && (rc == "d" || rc == "delete")) return true;
    if (c == 's' && (rc == "s" || rc == "sfx")) return true;
    if (c == 'p' && rc == "p") return true;
    return false;
  }

  std::vector<std::string> get_default_options(char cmd, const std::string &cmd_word = "") const {
    std::vector<std::string> opts = global_options;
    for (size_t i = 0; i < default_rules.size(); i++) {
      const DefaultRule &rule = default_rules[i];
      bool matched = false;
      for (size_t j = 0; j < rule.commands.size(); j++) {
        if (command_matches(rule.commands[j], cmd, cmd_word)) {
          matched = true;
          break;
        }
      }
      if (matched) {
        opts.insert(opts.end(), rule.options.begin(), rule.options.end());
      }
    }
    return opts;
  }

  std::string resolve_method(const std::string &m, int depth = 0) const {
    if (m.empty() || depth > 16) return m;

    // If compound method with '/', resolve each branch
    if (m.find('/') != std::string::npos) {
      std::string out;
      size_t start = 0;
      while (start < m.size()) {
        size_t slash = m.find('/', start);
        std::string part = (slash == std::string::npos) ? m.substr(start) : m.substr(start, slash - start);
        if (!out.empty()) out += " / ";
        out += resolve_method(trim(part), depth + 1);
        if (slash == std::string::npos) break;
        start = slash + 1;
      }
      return out;
    }

    // If compound method with '+', resolve each part
    if (m.find('+') != std::string::npos) {
      std::string out;
      size_t start = 0;
      while (start < m.size()) {
        size_t plus = m.find('+', start);
        std::string part = (plus == std::string::npos) ? m.substr(start) : m.substr(start, plus - start);
        if (!out.empty()) out += "+";
        out += resolve_method(trim(part), depth + 1);
        if (plus == std::string::npos) break;
        start = plus + 1;
      }
      return out;
    }

    // Direct lookup in compression_methods
    std::map<std::string, std::string>::const_iterator it = compression_methods.find(to_lower(m));
    if (it != compression_methods.end()) {
      return resolve_method(it->second, depth + 1);
    }

    return m;
  }

  void print_config(FILE *out = stdout) const {
    fprintf(out, "\n;You can insert these lines into ARC.INI\n");
    fprintf(out, "[Compression methods]\n");
    fprintf(out, ";Built-in method definitions\n");
    fprintf(out, "0  = storing\n");
    fprintf(out, "1  = tor:4\n");
    fprintf(out, "2  = tor:6\n");
    fprintf(out, "3  = lzma:96m:fast:32:mc4\n");
    fprintf(out, "4  = 4x4:lzma:96m:normal:16:mc8\n");
    fprintf(out, "5  = 4x4:lzma:96m:normal:32:mc32\n");
    fprintf(out, "1x = tor:4\n");
    fprintf(out, "2x = tor:6\n");
    fprintf(out, "3x = lzma:96m:fast:32:mc4\n");
    fprintf(out, "4x = lzma:96m:normal:16:mc8\n");
    fprintf(out, "5x = lzma:96m:normal:32:mc32\n");
    fprintf(out, "lzma = lzma:8m\n");
    fprintf(out, "tor  = tor:3\n");
    fprintf(out, "ppmd = ppmd:8:1m\n");
    fprintf(out, "bcj  = exe\n");
    fprintf(out, "exe  = exe\n");
    fprintf(out, "rep  = rep:64m\n");
    fprintf(out, "delta = delta\n");
    fprintf(out, "dict = dict\n");
    fprintf(out, "lzp  = lzp\n");
    fprintf(out, "mm   = mm\n");
    fprintf(out, "grzip = grzip:a\n");
    fprintf(out, "tta  = tta\n");

    if (!compression_methods.empty()) {
      fprintf(out, "\n;Custom compression methods from %s\n", filepath.c_str());
      for (std::map<std::string, std::string>::const_iterator it = compression_methods.begin();
           it != compression_methods.end(); ++it) {
        fprintf(out, "%s = %s\n", it->first.c_str(), it->second.c_str());
      }
    }
    if (!external_compressor_sections.empty()) {
      fprintf(out, "\n;External compressor sections loaded: %u\n", (unsigned)external_compressor_sections.size());
    }
    fprintf(out, "\nAll OK\n");
  }
};

#endif // FREEARC_ARC_INI_H

