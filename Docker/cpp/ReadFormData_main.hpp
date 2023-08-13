/* ReadFormData_main.hpp: header file for ReadFormData_main.cpp
 */


// options
struct option longOpts[]={
	{"help",    no_argument,       NULL, 'h'},
	{"in",      required_argument, NULL, 'i'},
	{"owner",   required_argument, NULL, 'o'},
	{"user",    required_argument, NULL, 'u'},
	{"verbose", required_argument, NULL, 'v'},
	{"Dest",    required_argument, NULL, 'D'},
	{0,0,0,0}
};

const char* help="\
ReadFormData.o: Read form data in a PDF file\n\
The form data is exported to a CSV file\n\
\n\
Options:\n\
Labels        Argument      Description\n\
-h, --help    (no argument) print the readme and exit\n\
-i, --in      filePath      input PDF file path\n\
-o, --owner   password      owner password\n\
-u, --user    password      user password\n\
-v, --verbose verbosity     verbosity of the output\n\
  0: Only error(s)\n\
  1: Error(s) and warnings\n\
  2: Error(s), warnings, and information\n\
  3: Error(s), warnings, information, and debug logs\n\
-D, --Dest    filePath      destination path\n";

	
