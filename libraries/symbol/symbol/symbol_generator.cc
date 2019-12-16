
#if defined(_MSC_VER)
#include <ciso646>
#endif

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
#include <set>
#include <sstream>
#include <string>

#define FIRST_LINE_COMMENT "// Generated by the lithium symbol generator."

std::string symbol_definition(std::string s) {
  std::string body;
  if (std::isdigit(s[0])) {
    body = R"cpp(#ifndef LI_SYMBOL___S__
#define LI_SYMBOL___S__
    LI_SYMBOL(__S__)
#endif
)cpp";
    // Check the string is actually a number.
    for (int i = 0; i < int(s.size()); i++)
      if (!std::isdigit(s[i]))
        return "";
  } else
    body = R"cpp(#ifndef LI_SYMBOL___S__
#define LI_SYMBOL___S__
    LI_SYMBOL(__S__)
#endif
)cpp";

  std::regex s_regex("__S__");
  body = std::regex_replace(body, s_regex, s);
  return body;
}

std::vector<std::string> find_symbols_in_files(std::vector<std::string> input_files) {
  std::set<std::string> symbols;
  std::regex symbol_regex(".?s::([a-zA-Z][[:alnum:]_]*)");
  std::set<std::string> keywords = {"alignas",      "alignof",
                                    "and",          "and_eq",
                                    "asm",          "auto",
                                    "bitand",       "bitor",
                                    "bool",         "break",
                                    "case",         "catch",
                                    "char",         "char16_t",
                                    "char32_t",     "class",
                                    "compl",        "const",
                                    "constexpr",    "const_cast",
                                    "continue",     "decltype",
                                    "default",      "delete",
                                    "do",           "double",
                                    "dynamic_cast", "else",
                                    "enum",         "explicit",
                                    "export",       "extern",
                                    "false",        "float",
                                    "for",          "friend",
                                    "goto",         "if",
                                    "inline",       "int",
                                    "long",         "mutable",
                                    "namespace",    "new",
                                    "noexcept",     "not",
                                    "not_eq",       "nullptr",
                                    "operator",     "or",
                                    "or_eq",        "private",
                                    "protected",    "public",
                                    "register",     "reinterpret_cast",
                                    "return",       "short",
                                    "signed",       "sizeof",
                                    "static",       "static_assert",
                                    "static_cast",  "struct",
                                    "switch",       "template",
                                    "this",         "thread_local",
                                    "throw",        "true",
                                    "try",          "typedef",
                                    "typeid",       "typename",
                                    "union",        "unsigned",
                                    "using",        "virtual",
                                    "void",         "volatile",
                                    "wchar_t",      "while",
                                    "xor",          "xor_eq"};

  auto parse_file = [&](std::string filename) {
    std::ifstream f(filename);
    if (!f) {
      std::cerr << "Cannot open file " << filename << " for reading." << std::endl;
    }

    std::string line;
    bool in_raw_string = false;
    while (!f.eof()) {
      getline(f, line);

      std::vector<int> dbl_quotes_pos;
      bool escaped = false;
      for (int i = 0; i < int(line.size()); i++) {
        if (line[i] == '"' and !escaped)
          dbl_quotes_pos.push_back(i);
        else if (line[i] == '\\')
          escaped = !escaped;
        else
          escaped = false;
      }

      auto is_in_string = [&](int p) {
        int i = 0;
        while (i < int(dbl_quotes_pos.size()) and dbl_quotes_pos[i] <= p)
          i++;
        return i % 2;
      };

      std::string::const_iterator start, end;
      start = line.begin();
      end = line.end();
      std::match_results<std::string::const_iterator> what;
      std::regex_constants::match_flag_type flags = std::regex_constants::match_default;
      while (regex_search(start, end, what, symbol_regex, flags)) {
        std::string m = what[0];
        std::string s = what[1];

        bool is_type = s.size() >= 2 and s[s.size() - 2] == '_' and s[s.size() - 1] == 't';

        if (!std::isalnum(m[0]) and !is_in_string(what.position()) and !is_type and
            keywords.find(s) == keywords.end())
          symbols.insert(what[1]);
        start = what[0].second;
      }
    }
  };

  for (auto path : input_files)
    parse_file(path);

  return std::vector<std::string>(symbols.begin(), symbols.end());
}

void write_symbol_file(std::vector<std::string> symbols, std::ostream& os) {

  if (!symbols.empty()) {
    os << FIRST_LINE_COMMENT << std::endl;
    std::ostringstream symbols_content;
    os << "#include <li/symbol/symbol.hh>" << std::endl;
    for (std::string s : symbols) {
      os << symbol_definition(s) << std::endl;
    }
  }
}

std::string get_file_contents(std::string filename) {
  std::ifstream in(filename, std::ios::in | std::ios::binary);
  if (in) {
    std::ostringstream contents;
    contents << in.rdbuf();
    in.close();
    return (contents.str());
  }
  throw(errno);
}
// Iod symbols generator.
//
//    For each variable name starting with underscore, generates a symbol
//    definition.
//
int main(int argc, char* argv[]) {
  using namespace std;

  if (argc < 2) {
    cout << "=================== Lithium symbol generator ===================" << endl << endl;
    cout << "Usage: " << argv[0] << " input_cpp_file1, ..., input_cpp_fileN" << endl;
    cout << "   Output on stdout the definitions of all the symbols used in the input files."
         << endl
         << endl;
    cout << "Usage: " << argv[0] << " project_root" << endl;
    cout << "   For each folder under project root write a symbols.hh file containing the" << endl;
    cout << "   declarations of all symbols used in C++ source and header of this same directory."
         << endl;
    return 1;
  }

  namespace fs = std::filesystem;

  if (fs::is_regular_file(argv[1])) {
    std::vector<std::string> files;
    for (int i = 1; i < argc; i++)
      files.push_back(argv[i]);
    write_symbol_file(find_symbols_in_files(files), std::cout);
  }
  if (fs::is_directory(argv[1])) {
    for (auto& p : fs::recursive_directory_iterator(argv[1]))
      if (fs::is_directory(p.path())) {
        std::vector<std::string> files;
        std::vector<std::string> extentions = {".cc", ".cpp", ".h", ".hh", ".hpp"};

        for (auto p2 : fs::directory_iterator(p.path()))
          if (std::find(extentions.begin(), extentions.end(), p2.path().extension()) !=
              extentions.end())
            files.push_back(p2.path().string());

        auto symbols = find_symbols_in_files(files);
        if (!symbols.empty()) {
          auto symbol_file = p / fs::path("symbols.hh");
          std::ostringstream ss;
          write_symbol_file(symbols, ss);

          if (fs::is_regular_file(symbol_file) &&
              ss.str() == get_file_contents(symbol_file.string()))
            continue;
          else {
            auto of = std::ofstream(symbol_file.string());
            of << ss.str();
          }
        }
      }
  }
}
