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

	if(PP.HasError()){
		Log(LOG_ERROR, "Parse failed");
		return -1;
	}

	// Check the authentication
	// if the password is not given, try to authenticate with no password
	if(PP.IsEncrypted()){
		// user
		if(userPwd!=NULL){
			if(PP.AuthUser(userPwd)){
				Log(LOG_INFO, "User password is ok");
			}else{
				Log(LOG_WARN, "User password is NOT ok");
			}
		}
		// owner
		if(ownerPwd!=NULL){
			if(PP.AuthOwner(ownerPwd)){
				Log(LOG_INFO, "Owner password is ok");
			}else{
				Log(LOG_WARN, "Owner password is NOT ok");
			}
		}
		// when none is given
		if(userPwd==NULL && ownerPwd==NULL){
		  char* blankPwd=new char[1];
			blankPwd[0]='\0';
			if(PP.AuthUser(blankPwd)){
				Log(LOG_INFO, "User password (blank) is ok");
			}else{
				Log(LOG_WARN, "User password (blank) is NOT ok");
			}
			if(PP.AuthOwner(blankPwd)){
				Log(LOG_INFO, "Owner password (blank) is ok");
			}else{
				Log(LOG_WARN, "Owner password (blank) is NOT ok");
			}
		}
		if(PP.IsAuthenticated()==false){
			Log(LOG_ERROR, "Authentication failed");
			return -1;
		}else{
			Log(LOG_INFO, "Authentication ok");
		}
	}

	// Version in Document catalog dictionary
	if(!PP.ReadVersion()){
		return -1;
	}

	// Pages
	if(!PP.ReadPages()){
		return -1;
	}
	// Page information
	int i, j;
	if(LOG_LEVEL>=LOG_INFO){
		Log(LOG_INFO, "Pages information");
		Array* MediaBox;
		void* ContentsValue;
		Stream* Contents;
		int ContentsType;
		int numPages=PP.GetPageSize();
		for(i=0; i<numPages; i++){
			if(PP.ReadPageDict(i, "MediaBox", (void**)&MediaBox, Type::Array, true)){
				Log(LOG_INFO, "Page %d MediaBox:", i);
				MediaBox->Print();
			}
			if(PP.ReadPageDict(i, "Contents", (void**)&ContentsValue, &ContentsType, false)){
				if(ContentsType==Type::Array){
					int ContentsSize=((Array*)ContentsValue)->GetSize();
					for(j=0; j<ContentsSize; j++){
						if(PP.Read((Array*)ContentsValue, j, (void**)&Contents, Type::Array)){
							Log(LOG_INFO, "Page %d Contents %d:", i, j);
							printf("%s", Contents->decoData);
						}
					}
				}else if(ContentsType==Type::Stream){
					Log(LOG_INFO, "Page %d Contents:", i, j);
					printf("%s", ((Stream*)ContentsValue)->decoData);
				}else{
					Log(LOG_ERROR, "Invalid Contents, %d", ContentsType);
					return -1;
				}
			}
		}
	}

	
	return 0;
}
