/* ReadPDF_main.cpp: an exectable to read a PDF file
 */

#include <fstream>
#include <iostream>

#include <getopt.h>

#ifndef LOG_INCLUDED
#include "log.hpp"
#endif

#include "ReadPDF_main.hpp"
#include "variables.hpp"
#include "PDFParser.hpp"

using namespace std;
int main(int argc, char** argv){
	// Read options
	char* inputFilePath=NULL;
	char* ownerPwd=NULL;
	char* userPwd=NULL;
	char* log_level_char=NULL;

	int opt; // getopt result
	int longIndex; // index of the specified option
	opterr=0; // disable error output
	
	while((opt=getopt_long(argc, argv, "hi:o:u:v:", longOpts, &longIndex))!=-1){
		switch(opt){
		case 'i':
			if(inputFilePath!=NULL){
				Log(LOG_ERROR, "Input file is already specified");
				return -1;
			}
			inputFilePath=optarg;
			break;
		case 'o':
			if(ownerPwd!=NULL){
				Log(LOG_ERROR, "Owner password is already specified");
				return -1;
			}
			ownerPwd=optarg;
			break;
		case 'u':
			if(userPwd!=NULL){
				Log(LOG_ERROR, "User password is already specified");
				return -1;
			}
			userPwd=optarg;
			break;
		case 'v':
			if(log_level_char!=NULL){
				Log(LOG_ERROR, "Verbosity is already specified");
				return -1;
			}
			log_level_char=optarg;
			SetLogLevel(atoi(log_level_char));
			break;
		case 'h':
			printf(help);
			return 0;
		case '?':
			Log(LOG_ERROR, "Option '%c' is invalid", optopt);
			return -1;
		default:
			Log(LOG_ERROR, "Unexpected error");
			return -1;
		}
	}
	if(inputFilePath==NULL){
		Log(LOG_ERROR, "Input file is not specified, see the help below");
		printf(help);
		return -1;
	}
	Log(LOG_INFO, "Input file is %s", inputFilePath);
	if(userPwd!=NULL){
		Log(LOG_INFO, "User password is specified");
	}else{
		Log(LOG_INFO, "User password is NOT specified");
	}
	if(ownerPwd!=NULL){
		Log(LOG_INFO, "Owner password is specified");
	}else{
		Log(LOG_INFO, "Owner password is NOT specified");
	}

	// Initialize PDFParser
	PDFParser PP(inputFilePath);
		
	return 0;
}
