/* ExportPDF_main.hpp: header file for ExportPDF_main.cpp
 */


// options
struct option longOpts[]={
	{"dest",    required_argument, NULL, 'd'},
	{"help",    no_argument,       NULL, 'h'},
	{"in",      required_argument, NULL, 'i'},
	{"owner",   required_argument, NULL, 'o'},
	{"user",    required_argument, NULL, 'u'},
	{"verbose", required_argument, NULL, 'v'},
	{"Encrypt", no_argument,        NULL, 'E'},
	{"Length",  required_argument, NULL, 'L'},
	{"Owner",   required_argument, NULL, 'O'},
	{"Perms",   required_argument, NULL, 'P'},
	{"User",    required_argument, NULL, 'U'},
	{"Version", required_argument, NULL, 'V'},
	{0,0,0,0}
};
const char* help="\
ExportPDF.o: Export a PDF file\n\
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
-D, --Dest    filePath      destination path\n\
-E, --Encrypt (no argument) encrypt the output\n\
-L, --Length  number        key length in bits (default 128), meaningful only when V=2\n\
-O, --Owner   password      new owner password\n\
-P, --Perms   perms label   permissions ('1' means enabled, '0' means disabled)\n\
  [0]: print the document (bit 3)\n\
  [1]: modify the contents (bit 4)\n\
  [2]: copy text and graphics (bit 5)\n\
  [3]: add or modify annotations (bit 6)\n\
  [4]: fill in form fields (bit 9)\n\
  [5]: assemble the document (bit 11)\n\
  [6]: faithfully print the document (bit 12)\n\
-U, --User    password      new user password\n\
-V, --Version number        encryption version (1, 2, 4, or 5)\n\
  1: RC4, key length=40 bits     (PDF 1.1-1.7)\n\
  2: RC4, key length=40-128 bits (PDF 1.4-1.7)\n\
  4: AES, key length=128 bits    (PDF 1.5-1.7) (PDF 1.5 uses RC4)\n\
  5: AES, key length=256 bits    (PDF 2.0)\n\
\n\
Rules for capitalized arguemnts for the output encryption\n\
\n\
1. When the original is encrypted:\n\
  You specify the passwords by the -O and -U arguments.\n\
  The owner password must be specified, while the user password can be blank.\n\
  You may specify new permissions by -P and encryption version by -V.\n\
  If you don't use these arguments, the original values are inherted.\n\
\n\
2. When the original is unencrypted:\n\
  You specify the passwords by the -O and -U arguments.\n\
  The owner password must be specified, while the user password can be blank.\n\
  You may specify the permissions by -P and the encryption version by -V.\n\
  The default value for the permission is '1111111', permitting all acts.\n\
  The default value for the version is determined from the PDF file version.\n";

	
