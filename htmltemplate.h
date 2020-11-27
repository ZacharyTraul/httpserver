#ifndef HTMLTEMPLATE_H
#define HTMLTEMPLATE_H
#include <iostream>
#include <fstream>
#include <string>
#include <map>
#include <vector>
#include <cstring>

struct template_args{
	std::map<std::string, std::vector<std::string>> loop_vars;
	std::map<std::string, std::string> replace_vars;
	std::map<std::string, bool> cond_vars;
	std::map<std::string, std::string> import_vars;
};

class HTMLTemplate{
	private:
		static std::string str_from_file(std::string path);
		static std::string handle_replace(std::string input, std::map<std::string, std::string> replace_vars);
		static std::string handle_imports(std::string input, std::map<std::string, std::string> import_vars);
		static std::string handle_conds(std::string input, std::map<std::string, bool> cond_vars, size_t start, size_t end);
		static std::string handle_loops(std::string input, std::map<std::string, std::vector<std::string>> loop_vars, size_t start, size_t end);
	public:
		char * process_template(std::string path, template_args args);
		size_t length;
};
#endif
