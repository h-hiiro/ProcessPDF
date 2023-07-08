/* ModifyMetadata_main.cpp: an exectable to modify Metadata in a PDF file
 */

#include <fstream>
#include <iostream>

#include <getopt.h>

#ifndef LOG_INCLUDED
#include "log.hpp"
#endif

#include "ModifyMetadata_main.hpp"
#include "variables.hpp"
#include "PDFExporter.hpp"

using namespace std;
int main(int argc, char** argv){
	// Read options
	// For reading
	char* inputFilePath=NULL;
	char* ownerPwd=NULL;
	char* userPwd=NULL;
	char* log_level_char=NULL;
	// For exporting
	char* destPath=NULL;
	bool removeMetadata=false;

	int opt; // getopt result
	int longIndex; // index of the specified option
	opterr=0; // disable error output
	
	while((opt=getopt_long(argc, argv, "hi:o:u:v:D:R", longOpts, &longIndex))!=-1){
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
		case 'D':
			if(destPath!=NULL){
				Log(LOG_ERROR, "Destination path is already specified"); return -1;
			}
			destPath=optarg; break;
		case 'R':
			removeMetadata=true; break;
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
			PP.CopyEncryptObj();
		}
	}

	// Version in Document catalog dictionary
	if(!PP.ReadVersion()){
		return -1;
	}
	
	// export configurations
	if(destPath==NULL){
		Log(LOG_ERROR, "Destination path is not specified, see the help below");
		printf(help);
		return -1;
	}
	Log(LOG_INFO, "Destination path is %s", destPath);

	// Metadata in trailer/Info
	if(PP.trailer.Search("Info")>=0){
		Log(LOG_INFO, "Info exists in the trailer");
		Dictionary* Metadata_Info;
		Indirect* MetadataRef_Info;
		if(PP.Read(&PP.trailer, "Info", (void**)&Metadata_Info, Type::Dict) &&
			 PP.trailer.Read("Info", (void**)&MetadataRef_Info, Type::Indirect)){
			if(LOG_LEVEL>=LOG_DEBUG){
				Log(LOG_DEBUG, "Metadata in Info:");
				Metadata_Info->Print();
			}
			if(removeMetadata){
				while(Metadata_Info->GetSize()>0){
					Metadata_Info->Delete(0);
				}
				PP.trailer.Delete(PP.trailer.Search("Info"));
				PP.Reference[MetadataRef_Info->objNumber]->used=false;
			}
		}else{
			Log(LOG_ERROR, "Failed in reading Info");
			return -1;
		}
	}

	// Metadata in Root/Metadata
	Dictionary* Root;
	if(PP.Read(&PP.trailer, "Root", (void**)&Root, Type::Dict)){
		if(Root->Search("Metadata")>=0){
			Log(LOG_INFO, "Metadata exists in the Document catalog dictionary");
			Stream* Metadata_Root;
			Indirect* MetadataRef_Root;
			if(PP.Read(Root, "Metadata", (void**)&Metadata_Root, Type::Stream) &&
				 Root->Read("Metadata", (void**)&MetadataRef_Root, Type::Indirect)){
				if(LOG_LEVEL>=LOG_DEBUG){
					Log(LOG_DEBUG, "Metadata in Root:");
					Metadata_Root->StmDict.Print();
					for(int i=0; i<Metadata_Root->decoDataLen; i++){
						printf("%c", Metadata_Root->decoData[i]);
					}
					printf("\n");
				}
				if(removeMetadata){
					PP.Reference[MetadataRef_Root->objNumber]->used=false;
					Root->Delete(Root->Search("Metadata"));
				}
			}else{
				Log(LOG_ERROR, "Failed in reading Metadata");
				return -1;
			}
		}
	}else{
		Log(LOG_ERROR, "Failed in reading the Document catalog dictionary");
	}
				
	PDFExporter PE(&PP);
	if(PE.exportToFile(destPath, PP.IsEncrypted())){
		Log(LOG_INFO, "Export succeeded");
	}else{
		Log(LOG_ERROR, "Export failed");
		return -1;
	}

	
	return 0;
}
