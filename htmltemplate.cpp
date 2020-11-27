#include "htmltemplate.h"

//Returns a string containing the contents at path
//On error returns empty string.
std::string HTMLTemplate::str_from_file(std::string path){
	std::string out;
	std::string line;
	std::ifstream file (path);
	if(file.is_open()){
		while(getline(file, line)){
			out += line + "\n";
		}
	}
	return out;
}

//Returns a string containing the fully processed template
//Takes in the path of the template and the arguments for that template.
//If there is no file at path returns empty string.
char * HTMLTemplate::process_template(std::string path, template_args args){
	std::string temp = str_from_file(path);

	//There was no file to grab.
	if(temp.empty()) return nullptr;
	
	//Recursively do imports.
	std::string import_token = "{{IMPORT ";
	while(temp.find(import_token) != std::string::npos){
		temp = handle_imports(temp, args.import_vars);
	}

	//Do all the simple replaces.
	temp = handle_replace(temp, args.replace_vars);
	
	//Handle all the conditionals and loops.
	//Search for blocks until we run out
	//When we find one, find the end and run it through the proper function
	std::string if_token = "{{IF ";
	std::string loop_token = "{{LOOP ";
	std::string endif_token = "{{ENDIF}}";
	std::string endloop_token = "{{ENDLOOP}}";
	while((temp.find(if_token) != std::string::npos && temp.find(endif_token) != std::string::npos) ||
	      (temp.find(loop_token) != std::string::npos && temp.find(endloop_token) != std::string::npos)){
		//Block start is the first instance of an if or loop token.
		//Also asign the type for later.
		size_t block_start = temp.find(if_token);
		std::string block_type = "IF";
		if (temp.find(loop_token) < block_start){
		       	block_start = temp.find(loop_token);
			block_type = "LOOP";
		}
		//Find the end of the bloc
		size_t level = 1;
		size_t block_end;
		//So that we don't just keep finding the same token over and over again.
		size_t parse_pos = block_start + 1;
		std::string block_start_token = "{{" + block_type;
		std::string block_end_token = "{{END" + block_type + "}}";
		while(level != 0){
			//Find whether the next block of that type is a start or end block
			size_t next_block = temp.find(block_start_token, parse_pos);
			bool start_block = true; 
			if(temp.find(block_end_token, parse_pos) < next_block){
				next_block = temp.find(block_end_token, parse_pos);
				start_block = false; 
			}
			//Update the parse_pos. Can't set it to next_block because we would just find the block repeatedly.
			parse_pos = next_block + 1;
			//If we couldn't find either of them that means there is an unclosed block.
			std::string error_message = "Error Creating Page";
			char * error = new char[error_message.length()];
			length = error_message.length();
			if(next_block == std::string::npos) return error;		
			//If it is a start block, increase the level by one.
			if(start_block) level++;
			//If it is an end block, decrease the level by one.
			else level--;	
			//If the level is zero, that means the current position is that of the end block.
			if(level == 0) block_end = next_block;
		}
		if(block_type == "IF") temp = handle_conds(temp, args.cond_vars, block_start, block_end);
		else {
			temp = handle_loops(temp, args.loop_vars, block_start, block_end);
		}
	}
	char * output = new char[temp.length()];
	strcpy(output, temp.c_str());
	length = temp.length();
	return output;
}

//Returns a string containing the template after all replace_vars have been replaced.
//If there is nothing to replace it just returns input.
std::string HTMLTemplate::handle_replace(std::string input, std::map<std::string, std::string> replace_vars){
	for(auto const& [find, replace] : replace_vars){
		while(true){
			size_t pos = input.find(find);
			if(pos != std::string::npos){
				input.erase(pos, find.length());
				input.insert(pos, replace);
			} else break;
		}
	}	
	return input;
}

//Returns a string containing the template will all imports replaced.
//The syntax for an import is {{IMPORT var}}
//var holds the path to the file to be imported.
//To specify a path in the file, use: {{IMPORT path(/path/to/file)}}
//If the file cannot be found the import is simply deleted and not replaced.
std::string HTMLTemplate::handle_imports(std::string input, std::map<std::string, std::string> import_vars){
	std::string start_token = "{{IMPORT ";
	std::string end_token = "}}";
	std::string path_token = "path(";
	//While we can find both the start and end tokens.
	while(input.find(start_token) != std::string::npos && input.find(end_token) != std::string::npos){
		//Get the start and end of the import syntax
		size_t start = input.find(start_token);
		size_t end = input.find(end_token, start);

		//Load the file the import needs to be replaced with.
		size_t path_start = start + start_token.length();
		size_t path_length = end - path_start;
		std::string argument = input.substr(path_start, path_length);
		std::string filename;

		//If it starts with path(
		if(argument.find(path_token) == 0){
			filename = argument.substr(path_token.length(), argument.length() - path_token.length());
		} else {
			filename = import_vars[argument];
		}

		//Get the file
		std::string file = str_from_file(filename);

		//Replace it.
		input.erase(start, end + end_token.length() - start);
		input.insert(start, file);
	}
	return input;
}

//Returns a string containing the template with all if else statements
//evaluated and replaced. Currently no support for compound statements.
//If there is an if but no else and the if returns false, the block is
//just deleted and not replaced.
std::string HTMLTemplate::handle_conds(std::string input, std::map<std::string, bool> cond_vars, size_t start, size_t end){
	std::string start_token = "{{IF ";
	std::string start_end_token = "}}";
	std::string else_token = "{{ELSE}}";
	std::string end_token = "{{ENDIF}}";
	//Get the positions of tokens that we don't already know.
	size_t start_end = input.find(start_end_token, start);

	//Find if there is an else block on the right level.
	int else_level = 1;
	size_t else_pos;
	//So we don't just keep finding the same thing over and over, we need to move where we start searching from.
	size_t parse_pos = start_end;
	while(else_level != 0){
		//Then check if there is an else that occurs earlier
		size_t next_block = input.find(else_token, parse_pos);
		bool else_block = true;

		//Then, check if an if occurs earlier.
		if(input.find(start_token, parse_pos) < next_block){
			next_block = input.find(start_token, parse_pos);
			else_block = false;
		}

		//If the end of the whole block occurs before both of these,
		//there is no else.
		if(end < next_block){
			else_pos = end;
			break;
		}

		parse_pos = next_block + 1;
		//If we didn't find any of them something wasn't right.
		if(next_block == std::string::npos) return "Error Creating Page";
		//If it was an end or else block
		if(else_block) else_level--;
		else else_level++;	
		//We reached the end of this block, either by finding an else or getting to the end of the block.
		if(else_level == 0 && else_block) else_pos = next_block;
	}

	//Get the variable we are evaluating against
	size_t var_start = start + start_token.length();
	size_t var_length = start_end - var_start;
	std::string var = input.substr(var_start, var_length);
	//If there is an ! at the beginning of var, remove it and not the variable when we evauluate.
	bool not_var = false;
	if(var[0] == '!'){
		var = var.substr(1); 
		not_var = true;
	}
	bool val = cond_vars[var] ^ not_var;

	//Determine what content we are using to replace the block.	
	size_t content_start = 0;
	size_t content_length = 0;	
	//If val, replace with content after IF but before either ELSE if it exists or ENDIF if it doesn't exist.
	if(val){
		content_start = start_end + start_end_token.length();
		content_length = else_pos - content_start; //If no else, else_pos is set to the position of end.
	} else if (!val && (else_pos != end)) {
		content_start = else_pos + else_token.length();
		content_length = end - content_start;
	}
	std::string content = input.substr(content_start, content_length);
	//Replace the block. 
	input.erase(start, end + end_token.length() - start);
	input.insert(start, content); 
	return input;
}

std::string HTMLTemplate::handle_loops(std::string input, std::map<std::string, std::vector<std::string>> loop_vars, size_t start, size_t end){
	std::string start_token = "{{LOOP ";
	std::string start_end_token = "}}";
	std::string end_token = "{{ENDLOOP}}";
	std::string delimeter = ", ";
	
	//Get the positions of tokens we don't already know.
	size_t start_end = input.find(start_end_token, start);
	
	//Get the variables we are looping with.
	size_t pos = start + start_token.length();
	std::vector<std::string> vars;
	while(true){
		if(input.find(delimeter, pos) < start_end){
			vars.push_back(input.substr(pos, input.find(delimeter, pos) - pos));
			pos += input.find(delimeter, pos) - pos + delimeter.length();
		}
		else {
			vars.push_back(input.substr(pos, input.find(start_end_token, pos) - pos));
			break;
		} 
	}

	//Get the content we are looping with
	size_t content_start = start_end + start_end_token.length();
	size_t content_length = end - content_start;
	std::string content = input.substr(content_start, content_length);

	//Delete the block and start looping.
	input.erase(start, end + end_token.length() - start);
	pos = start;
	for(int i = 0; i < loop_vars[vars[0]].size(); i++){
		//Prepare the content
		std::string temp_content = content;
		for(std::string var: vars){
			std::cout << var << std::endl;
			size_t rep_loc = temp_content.find("{{" + var + "}}"); 
			if(rep_loc != std::string::npos){
				temp_content.erase(rep_loc, 4 + var.length()); 
				temp_content.insert(rep_loc, loop_vars[var][i]);
			}
		}
		//Place it in the input
		input.insert(pos, temp_content);
		//Advance our position
		pos += temp_content.length();
	}
	return input;
} 
