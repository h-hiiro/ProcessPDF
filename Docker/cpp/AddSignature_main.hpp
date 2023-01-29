/* AddSignature_main.hpp: header file for AddSignature_main.cpp
 */


// options
struct option longOpts[]={
	{"help",    no_argument,       NULL, 'h'},
	{"in",      required_argument, NULL, 'i'},
	{"owner",   required_argument, NULL, 'o'},
	{"user",    required_argument, NULL, 'u'},
	{"verbose", required_argument, NULL, 'v'},
	{"Dest",    required_argument, NULL, 'D'},
	{"Signer",  required_argument, NULL, 'S'},
	{"Private", required_argument, NULL, 'R'},
	{"Public",  required_argument, NULL, 'U'},
	{"Number",  required_argument, NULL, 'N'},
	{"Cert",    required_argument, NULL, 'C'},
	{"Perm",    required_argument, NULL, 'P'},
	{0,0,0,0}
};
const char* help="\
AddSignature.o: Add a signature in a PDF file\n\
\n\
Options:\n\
Labels        Argument      Description\n\
-h, --help    (no argument) print the readme and exit\n\
-i, --in      filePath      input PDF file path\n\
-o, --owner   password      original owner password\n\
-u, --user    password      original user password\n\
-v, --verbose verbosity     verbosity of the output\n\
  0: Only error(s)\n\
  1: Error(s) and warnings\n\
  2: Error(s), warnings, and information\n\
  3: Error(s), warnings, information, and debug logs\n\
-C, --Cert    filePath      certificate PEM file(s)\n\
-D, --Dest    filePath      destination path\n\
-N, --Number  pageNum       page number where the sign is added (default 1)\n\
-P, --Perm    permission    permission label\n\
  1: No change is allowed (default)\n\
  2: Filling forms is allowed\n\
  3: Filling forms and mofidying annotations are allowed\n\
-R, --Private filePath      private key PEM file\n\
-S, --Signer  name          signer name\n\
-U, --Public  filePath      public key PEM file\n\
\n\
The -C option can appear more than once like '-C file1 -C file2'.\n";
	
