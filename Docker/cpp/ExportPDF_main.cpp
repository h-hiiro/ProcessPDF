/* ExportPDF_main.cpp: an exectable to export a PDF file
 */

#include <fstream>
#include <iostream>
#include <cstring>
#include <cstdlib>
#include <ctime>
#include <getopt.h>

#ifndef LOG_INCLUDED
#include "log.hpp"
#endif

#include "ExportPDF_main.hpp"
#include "PDFExporter.hpp"
#include "variables.hpp"

using namespace std;
int main(int argc, char** argv){
	// Read options
	// For reading a PDF
	char* inputFilePath=NULL;
	char* ownerPwd=NULL;
	char* userPwd=NULL;
	char* log_level_char=NULL;
	// For exporting a PDF
	bool outputIsEncrypted=false;
	char* newOwnerPwd=NULL;
	char* newUserPwd=NULL;
	char* pwdLength_char=NULL;
	char* perms_char=NULL;
	char* encrVersion_char=NULL;
	char* destPath=NULL;

	int opt; // getopt result
	int longIndex; // index of the specified option
	opterr=0; // disable error output

	int i,j;

	while((opt=getopt_long(argc, argv, "hi:o:u:v:D:EL:O:P:U:V:", longOpts, &longIndex))!=-1){
		switch(opt){
		case 'i':
			if(inputFilePath!=NULL){
				Log(LOG_ERROR, "Input file is already specified"); return -1;
			}
			inputFilePath=optarg;	break;
		case 'o':
			if(ownerPwd!=NULL){
				Log(LOG_ERROR, "Owner password is already specified"); return -1;
			}
			ownerPwd=optarg; break;
		case 'u':
			if(userPwd!=NULL){
				Log(LOG_ERROR, "User password is already specified");	return -1;
			}
			userPwd=optarg; break;
		case 'v':
			if(log_level_char!=NULL){
				Log(LOG_ERROR, "Verbosity is already specified");	return -1;
			}
			log_level_char=optarg; SetLogLevel(atoi(log_level_char));	break;
		case 'D':
			if(destPath!=NULL){
				Log(LOG_ERROR, "Destination path is already specified"); return -1;
			}
			destPath=optarg; break;
		case 'E':
			outputIsEncrypted=true;	break;
		case 'O':
			if(newOwnerPwd!=NULL){
				Log(LOG_ERROR, "New owner password is already specified"); return -1;
			}
			newOwnerPwd=optarg; break;
		case 'U':
			if(newUserPwd!=NULL){
				Log(LOG_ERROR, "New user password is already specified");	return -1;
			}
			newUserPwd=optarg; break;
		case 'L':
			if(pwdLength_char!=NULL){
				Log(LOG_ERROR, "Password length is already specified");	return -1;
			}
			pwdLength_char=optarg; break;
		case 'P':
			if(perms_char!=NULL){
				Log(LOG_ERROR, "Perms is already specified");	return -1;
			}
			perms_char=optarg; break;
		case 'V':
			if(encrVersion_char!=NULL){
				Log(LOG_ERROR, "Encryption version is already specified");	return -1;
			}
			encrVersion_char=optarg; break;
		case 'h':
			printf(help); return 0;
		case '?':
			Log(LOG_ERROR, "Option '%c' is invalid", optopt);	return -1;
		default:
			Log(LOG_ERROR, "Unexpected error");	return -1;
		}
	}

	// read configurations
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
	bool ownerAuthenticated=false;
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
				ownerAuthenticated=true;
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
				ownerAuthenticated=true;
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
	if(!outputIsEncrypted){
		Log(LOG_INFO, "The exported PDF is not encrypted");
	}else{
		Log(LOG_INFO, "The exported PDF is encrypted");
		if(newOwnerPwd==NULL){
			Log(LOG_ERROR, "New owner password is not specified, see the help below");
			printf(help);
			return -1;
		}else{
			Log(LOG_INFO, "New owner password is %d bytes", strlen(newOwnerPwd));
		}
		if(newUserPwd==NULL){
			Log(LOG_INFO, "New user password is not specifed, the blank is set");
			newUserPwd=new char[1];
			newUserPwd[0]='\0';
		}else{
			Log(LOG_INFO, "New user password is %d bytes", strlen(newUserPwd));
		}
		if(PP.IsEncrypted()==false){
			Log(LOG_INFO, "Original PDF is not encrypted");
			PP.encryptObj_ex=new Encryption();
		}
		
		// V (encryption version)
		int encrVersion=-1;
		if(encrVersion_char!=NULL){
			encrVersion=atoi(encrVersion_char);
			Log(LOG_INFO, "The specified encryption version is %d", encrVersion);
		}
		if(PP.IsEncrypted() && encrVersion<0){
			Log(LOG_INFO, "Encryption version is not specified, inherted from the original");
		}else{
			if(PP.JudgeEncrVersion(&encrVersion)){
				// ok
				PP.encryptObj_ex->SetV(encrVersion);
			}else{
				Log(LOG_ERROR, "Encryption version %d is incompatible", encrVersion);
				return -1;
			}
		}
		encrVersion=PP.encryptObj_ex->GetV();
		Log(LOG_INFO, "Encryption version is %d", encrVersion);

		// key length
		if(encrVersion==2 && pwdLength_char!=NULL){
			int pwdLength=atoi(pwdLength_char);
			if(PP.encryptObj_ex->SetKeyLength(pwdLength)){
				// ok
				Log(LOG_INFO, "Key length is set to %d", pwdLength);
			}else{
				Log(LOG_INFO, "Key length %d is invalid in V==2", pwdLength);
				return -1;
			}
		}

		// CFM
		if(encrVersion==4 || encrVersion==5){
			// V=4, PDF=1.5 -> V2
			// V=4, PDF>1.6 -> AESV2
			// V=5          -> AESV3
			if(encrVersion==4 && PP.v_document.major==1){
				if(PP.v_document.minor<=5){
					PP.encryptObj_ex->SetCFM("V2");
					Log(LOG_INFO, "CFM is set to V2");
				}else{
					PP.encryptObj_ex->SetCFM("AESV2");
					Log(LOG_INFO, "CFM is set to AESV2");
				}
			}else if(encrVersion==5 /*&& PP.v_document.major==2*/){
				// the major version verification is omitted
				// because Adobe Acrobat exports PDF 1.7 with V=5
				PP.encryptObj_ex->SetCFM("AESV3");
				Log(LOG_INFO, "CFM is set to AESV3");
			}
		}

		// P
		if(perms_char==NULL){
			if(PP.IsEncrypted()){
				// inherted
				Log(LOG_INFO, "Perms is not specified, inherited from the original");
			}else{
				// default is used
				Log(LOG_INFO, "Perms is not specified, default '1111111' is used");
				perms_char=new char[8];
				strcpy(perms_char, "1111111");
			}
		}
		if(perms_char!=NULL){
			bool P_bool[7];
			if(strlen(perms_char)<7){
				Log(LOG_ERROR, "Perms is shorter than 7");
				return -1;
			}
			for(i=0; i<7; i++){
				if(perms_char[i]=='1'){
					P_bool[i]=true;
				}else if(perms_char[i]=='0'){
					P_bool[i]=false;
				}else{
					Log(LOG_ERROR, "Invalid character in perms");
					return -1;
				}
			}
			PP.encryptObj_ex->SetP(P_bool);
		}
		if(LOG_LEVEL>=LOG_INFO){
			Log(LOG_INFO, "Perms:");
			PP.encryptObj_ex->PrintP();
		}
		
		// password
		PDFStr* newUserPwd_str=new PDFStr(strlen(newUserPwd));
		unsignedstrcpy(newUserPwd_str->decrData, newUserPwd);
		PDFStr* newOwnerPwd_str=new PDFStr(strlen(newUserPwd));
		unsignedstrcpy(newOwnerPwd_str->decrData, newOwnerPwd);

		// prepare IDs
		Array* ID;
		if(PP.trailer.Search("ID")>=0){
			if(PP.trailer.Read("ID", (void**)&ID, Type::Array)){
				//ok
			}else{
				Log(LOG_ERROR, "Failed in reading ID");
				return -1;
			}
		}else{
			Log(LOG_WARN, "ID does not exist in the trailer dictionary, make it");
			PDFStr* id1=new PDFStr(16);
			id1->isHexStr=true;
			srand((unsigned int)time(NULL));
			for(i=0; i<16; i++){
				id1->decrData[i]=(unsigned char)(rand()%256);
			}
			if(LOG_LEVEL>=LOG_DEBUG){
				Log(LOG_DEBUG, "Prepared ID:");
				DumpPDFStr(id1);
			}
			ID=new Array();
			ID->Append(id1, Type::String);
			PDFStr* id2=new PDFStr(16);
			id2->isHexStr=true;
			for(i=0; i<16; i++){
				id2->decrData[i]=(unsigned char)(rand()%256);
			}
			if(LOG_LEVEL>=LOG_DEBUG){
				Log(LOG_DEBUG, "Prepared ID:");
				DumpPDFStr(id2);
			}
			ID=new Array();
			ID->Append(id1, Type::String);
			ID->Append(id2, Type::String);
			PP.trailer.Append("ID", ID, Type::Array);
		}
		
		// set password
		PP.encryptObj_ex->SetPwd(ID, newUserPwd_str, newOwnerPwd_str);
	}
  
	PDFExporter PE(&PP);
	if(PE.exportToFile(destPath, outputIsEncrypted)){
		Log(LOG_INFO, "Export succeeded");
	}else{
		Log(LOG_ERROR, "Export failed");
		return -1;
	}
	return 0;
	
}
