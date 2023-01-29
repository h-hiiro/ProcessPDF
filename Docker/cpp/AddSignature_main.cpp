/* AddSignature_main.cpp: an executable to add a signature in a PDF file
 */

#include <fstream>
#include <iostream>
#include <filesystem>
#include <cmath>
#include <cstring>
#include <openssl/cms.h>
#include <openssl/err.h>
#include <openssl/pem.h>
#include <getopt.h>

#ifndef LOG_INCLUDED
#include "log.hpp"
#endif

#include "PDFExporter.hpp"
#include "variables.hpp"
#include "AddSignature_main.hpp"

using namespace std;

int findString(ifstream* file, char* keyword);
char* ComputeSign(char* str_for_sign, int strSize, char* privateKeyPath, char* publicKeyPath, char** certKeyPaths, int numCerts);

int main(int argc, char** argv){
	// Read options
	// For reading a PDF
	char* inputFilePath=NULL;
	char* ownerPwd=NULL;
	char* userPwd=NULL;
	char* log_level_char=NULL;
	// For exporting a PDF
	char* destPath=NULL;
	char* signerName=NULL;
	char* publicKeyPath=NULL;
	char* privateKeyPath=NULL;
	char* pageNum_char=NULL;
	int pageNum=1;
	char* trPerm_char=NULL;
	int trPerm=1;
	int maxNumCerts=256;
	char* certKeyPaths[maxNumCerts];
	int numCerts=0;

	int opt; // getopt result
	int longIndex; // index of the specified option
	opterr=0; // disable error output

	int i,j;

	while((opt=getopt_long(argc, argv, "hi:o:u:v:D:S:R:U:N:C:P:", longOpts, &longIndex))!=-1){
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
		case 'S':
			if(signerName!=NULL){
				Log(LOG_ERROR, "Signer name is already specified"); return -1;
			}
			signerName=optarg; break;
		case 'R':
			if(privateKeyPath!=NULL){
				Log(LOG_ERROR, "Private key is already specified"); return -1;
			}
			privateKeyPath=optarg; break;
		case 'U':
			if(publicKeyPath!=NULL){
				Log(LOG_ERROR, "Public key is already specified"); return -1;
			}
			publicKeyPath=optarg; break;
		case 'N':
			if(pageNum_char!=NULL){
				Log(LOG_ERROR, "Page number is already specified"); return -1;
			}
			pageNum_char=optarg; break;
		case 'C':
			if(numCerts>=maxNumCerts){
				Log(LOG_ERROR, "Too many certificates"); return -1;
			}
			certKeyPaths[numCerts]=optarg; numCerts++; break;
		case 'P':
			if(trPerm_char!=NULL){
				Log(LOG_ERROR, "Perm is already specified"); return -1;
			}
			trPerm_char=optarg; break;
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

	// file content (for sign length estimation)
	ifstream file_all(inputFilePath);
	file_all.seekg(0, ios_base::end);
	int fileSize_all=file_all.tellg();
	Log(LOG_DEBUG, "File size: %d", fileSize_all);
	file_all.seekg(0, ios_base::beg);
	char data_all[fileSize_all];
	file_all.read(data_all, fileSize_all);
	file_all.close();
	
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
	// Pages
	if(!PP.ReadPages()){
		return -1;
	}
	int numPages=PP.GetPageSize();

	// export configurations
	if(destPath==NULL){
		Log(LOG_ERROR, "Destination path is not specified, see the help below");
		printf(help);
		return -1;
	}
	Log(LOG_INFO, "Destination path is %s", destPath);
	if(signerName==NULL){
		Log(LOG_ERROR, "Signer name is not specified, see the help below");
		printf(help);
		return -1;
	}
	Log(LOG_INFO, "Signer name is %s", signerName);
	if(privateKeyPath==NULL){
		Log(LOG_ERROR, "Private key path is not specified, see the help below");
		printf(help);
		return -1;
	}
	Log(LOG_INFO, "Private key path is %s", privateKeyPath);
	if(publicKeyPath==NULL){
		Log(LOG_ERROR, "Public key path is not specified, see the help below");
		printf(help);
		return -1;
	}
	Log(LOG_INFO, "Public key path is %s", publicKeyPath);
	if(numCerts>0){
		Log(LOG_INFO, "%d certificate(s) will be included", numCerts);
	}else{
		Log(LOG_INFO, "No certificate will be included");
	}
	if(pageNum_char==NULL){
		Log(LOG_INFO, "Page number is NOT specified, the sign is added in page 1");
	}else{
		pageNum=atoi(pageNum_char);
		if(1<=pageNum && pageNum<=numPages){
			Log(LOG_INFO, "The sign is added in page %d", pageNum);
		}else{
			Log(LOG_ERROR, "Page %d is invalid", pageNum);
			return -1;
		}
	}
	if(trPerm_char==NULL){
		Log(LOG_INFO, "Perm is NOT specified, default 1 is used");
	}else{
		trPerm=atoi(trPerm_char);
		if(1<=trPerm && trPerm<=3){
			Log(LOG_INFO, "Perm is set to %d", trPerm);
		}else{
			Log(LOG_ERROR, "Perm %d is invalid", trPerm);
			return -1;
		}
	}
  
	// document catalog dictionary
	Dictionary* dCatalog;
	if(PP.Read(&PP.trailer, "Root", (void**)&dCatalog, Type::Dict)){
		//ok
	}else{
		Log(LOG_ERROR, "Failed in reading document catalog dictionary");
		return -1;
	}

	// /Perms in document catalog dictionary
	Dictionary* Perms;
	if(dCatalog->Search("Perms")<0){
		Log(LOG_DEBUG, "Perms not found in the document catalog dictionary, add it");
		Perms=new Dictionary();
		dCatalog->Append("Perms", Perms, Type::Dict);
	}else{
		Log(LOG_DEBUG, "Perms found in the document catalog dictionary");
		if(dCatalog->Read("Perms", (void**)&Perms, Type::Dict)){
			//ok
		}else{
			Log(LOG_ERROR, "Failed in reading Perms");
			return -1;
		}
	}

	// DocMDP
	Dictionary* DocMDP;
	Indirect* DocMDPRef;
	if(Perms->Search("DocMDP")<0){
		Log(LOG_DEBUG, "DocMDP not found in permissions dictionary, make it");
		// new DocMDP (Indirect dictionary)
		DocMDP=new Dictionary();
		int DocMDPObjNum=PP.AddNewReference(Type::Dict);
	  DocMDPRef=PP.Reference[DocMDPObjNum];
		DocMDPRef->position=-1;
		DocMDPRef->value=DocMDP;
	}else{
		Log(LOG_DEBUG, "DocMDP found in the permissions dictionary");
		if(Perms->Read("DocMDP", (void**)&DocMDPRef, Type::Indirect) &&
			 PP.Read(Perms, "DocMDP", (void**)&DocMDP, Type::Dict)){
			// ok
			DocMDPRef=PP.Reference[DocMDPRef->objNumber];
		}else{
			Log(LOG_ERROR, "Failed in reading the DocMDP dictionary");
			return -1;
		}
	}

	// remove elements in DocMDP
  while(DocMDP->GetSize()>0){
		DocMDP->Delete(0);
	}

	// remove elements in Perms
	while(Perms->GetSize()>0){
		Perms->Delete(0);
	}
	
	// add DocMDP in Perms
	Perms->Append("DocMDP", DocMDPRef, Type::Indirect);

	// DocMDP data
	DocMDP->Append("Type", (unsigned char*)"Sig", Type::Name);
	DocMDP->Append("Filter", (unsigned char*)"Adobe.PPKLite", Type::Name);
	DocMDP->Append("SubFilter", (unsigned char*)"adbe.pkcs7.detached", Type::Name);

	// Contents, byte range (estimation)
	Log(LOG_DEBUG, "Compute the sign (test)");
	char* sign_est=ComputeSign(data_all, fileSize_all, privateKeyPath, publicKeyPath, certKeyPaths, numCerts);
	int signBufferSize=strlen(sign_est)+1024;
	
	PDFStr* signBuffer=new PDFStr(signBufferSize);
	for(i=0; i<signBufferSize; i++){
		signBuffer->decrData[i]='\0';
	}
	signBuffer->isHexStr=true;
	DocMDP->Append("Contents", signBuffer, Type::String);

	int approxOutSize=fileSize_all+signBufferSize;
	int tentRangeDigit=1;
	while(approxOutSize>pow(10, tentRangeDigit)){
		tentRangeDigit++;
	}
	int tentRangeValue=pow(10, tentRangeDigit);
	Array* tentByteRange=new Array();
	for(i=0; i<4; i++){
		tentByteRange->Append(&tentRangeValue,Type::Int);
	}
	DocMDP->Append("ByteRange", tentByteRange, Type::Array);

	// signature reference array
	Array* Reference=new Array();
	DocMDP->Append("Reference", Reference, Type::Array);
	Dictionary* SigRefDict=new Dictionary();
	Reference->Append(SigRefDict, Type::Dict);
	SigRefDict->Append("Type", (unsigned char*)"SigRef", Type::Name);
	SigRefDict->Append("TransformMethod", (unsigned char*)"DocMDP", Type::Name);
	Dictionary* trParams=new Dictionary();	
	SigRefDict->Append("TransformParams", trParams, Type::Dict);
	trParams->Append("Type", (unsigned char*)"TransformParams", Type::Name);
	trParams->Append("P", &trPerm, Type::Int);
	trParams->Append("V", (unsigned char*)"1.2", Type::Name);
	unsigned char* DigestMethod=new unsigned char[7];
	unsignedstrcpy(DigestMethod, "SHA256");
	SigRefDict->Append("DigestMethod", DigestMethod, Type::Name);

	PDFStr* signer_str=new PDFStr(strlen(signerName));
	unsignedstrcpy(signer_str->decrData, signerName);
	DocMDP->Append("Name", signer_str, Type::String);
	DocMDP->Append("M", dateString(), Type::String);

	// ToDo: Prop_Build?
	
	// /AcroForm (indirect dictionary) in document catalog dictionary	
	Indirect* AcroFormRef; // may be unused
	Dictionary* AcroForm;
	if(dCatalog->Search("AcroForm")>=0){
		if(PP.Read(dCatalog, "AcroForm", (void**)&AcroForm, Type::Dict)){
			//ok
		}else{
			Log(LOG_ERROR, "Failed in reading AcroForm");
			return -1;
		}
	}else{
		Log(LOG_DEBUG, "AcroForm does not exist, make it");
		int AcroFormIndex=PP.AddNewReference(Type::Dict);
		AcroForm=new Dictionary();
		AcroFormRef=PP.Reference[AcroFormIndex];
		AcroFormRef->position=-1;
		AcroFormRef->value=AcroForm;
		dCatalog->Append("AcroForm", AcroFormRef, Type::Indirect);
	}

	
	// Annots in page
	Array* annotsArray;
	if(PP.ReadPageDict(pageNum-1, "Annots", (void**)&annotsArray, Type::Array, false)){
		// annots array exists
	}else{
		// annots array does not
		Log(LOG_DEBUG, "Annots array does not exist in page %d, make it", pageNum);
		annotsArray=new Array();
		PP.AppendToPageDict(pageNum-1, "Annots", annotsArray, Type::Array);
	}
	
	Dictionary* SigField=new Dictionary();	
	SigField->Append("FT", (unsigned char*)"Sig", Type::Name);
	PDFStr* SigFieldName=new PDFStr(10);
	unsignedstrcpy(SigFieldName->decrData, "Signature");
	SigField->Append("T", SigFieldName, Type::String);
	SigField->Append("V", DocMDPRef, Type::Indirect);

	// entries of Widget annotations
	SigField->Append("Type", (unsigned char*)"Annot", Type::Name);
	SigField->Append("Subtype", (unsigned char*)"Widget", Type::Name);
	int F_value=0b0000000010;
	SigField->Append("F", &F_value, Type::Int);
	Array* Sig_rect=new Array();
	int zero=0;
	for(i=0; i<4; i++){
		Sig_rect->Append(&zero, Type::Int);
	}
	SigField->Append("Rect", Sig_rect, Type::Array);

	int SigFieldIndex=PP.AddNewReference(Type::Dict);
	Indirect* SigFieldRef=PP.Reference[SigFieldIndex];
	SigFieldRef->position=-1;
	SigFieldRef->value=SigField;
	annotsArray->Append(SigFieldRef, Type::Indirect);

	
	int fieldsIndex=AcroForm->Search((unsigned char*)"Fields");
	Array* fields;
	if(AcroForm->Search("Fields")>=0){
		if(AcroForm->Read("Fields", (void**)&fields, Type::Array)){
			// ok
		}else{
			Log(LOG_ERROR, "Failed in reading Fields");
			return -1;
		}
	}else{
		Log(LOG_DEBUG, "Fields does not exist in AcroForm, make it");
		fields=new Array();
		AcroForm->Append("Fields", fields, Type::Array);
	}
	fields->Append(SigFieldRef, Type::Indirect);

	int SigFlagsValue=2; //AppendOnly
	if(AcroForm->Search("SigFlags")>=0){
		if(AcroForm->Update("SigFlags", &SigFlagsValue, Type::Int)){
			//ok
		}else{
			Log(LOG_ERROR, "Failed in updating SigFlags");
			return -1;
		}
	}else{
		AcroForm->Append("SigFlags", &SigFlagsValue, Type::Int);
	}
	
	// export
	PDFExporter PE(&PP);
	if(PE.exportToFile(destPath,PP.IsEncrypted())){
		Log(LOG_INFO, "Export succeeded");
	}else{
		Log(LOG_ERROR, "Export failed");
		return -1;
	}
	
	
	int outputSize=(int)filesystem::file_size(destPath);
	Log(LOG_INFO, "Output file size: %d", outputSize);
	
	// modify ByteRange
	int sigDictPos=DocMDPRef->position;
	Log(LOG_DEBUG, "Signature dictionary (DocMDP) position in the output: %d",sigDictPos);

	ifstream* file_in=new ifstream(destPath, ios::binary);
	file_in->seekg(sigDictPos,ios_base::beg);

	int ByteRangePos=findString(file_in, (char*)"/ByteRange");
	int LSBPos=findString(file_in, (char*)"[");
	int RSBPos=findString(file_in, (char*)"]");

	Log(LOG_DEBUG, "/ByteRange @%d", ByteRangePos);
	Log(LOG_DEBUG, "[ @%d", LSBPos);
	Log(LOG_DEBUG, "] @%d", RSBPos);
	int ByteRangeLength=RSBPos-LSBPos-1;

	file_in->seekg(sigDictPos, ios_base::beg);
	int ContentsPos=findString(file_in, (char*)"/Contents");
	int LTPos=findString(file_in, (char*)"<");
	int RTPos=findString(file_in, (char*)">");
	Log(LOG_DEBUG, "/Contents @%d", ContentsPos);
	Log(LOG_DEBUG, "< @%d", LTPos);
	Log(LOG_DEBUG, "> @%d", RTPos);

	// ByteRange is 0 - before <, after > - end (< and > are excluded);
	int ByteRange[4];
	ByteRange[0]=0;
	ByteRange[1]=LTPos;
	ByteRange[2]=RTPos+1;
	ByteRange[3]=outputSize-RTPos-1;
	int rangeSize=ByteRange[1]+ByteRange[3];
	Log(LOG_DEBUG, "Data length for signature: %d", rangeSize);
	char ByteRange_c[ByteRangeLength+1];
	for(i=0; i<ByteRangeLength; i++){
		ByteRange_c[i]=' ';
	}
	ByteRange_c[ByteRangeLength]='\0';
	sprintf(ByteRange_c, "%d %d %d %d", ByteRange[0],ByteRange[1],ByteRange[2],ByteRange[3]);
	ByteRange_c[strlen(ByteRange_c)]=' ';
	Log(LOG_DEBUG, "ByteRange: \"%s\"", ByteRange_c);

	char file_in_all[outputSize];
	file_in->seekg(0, ios_base::beg);
	file_in->read(file_in_all, outputSize);
	file_in->close();

	for(i=0; i<ByteRangeLength; i++){
		file_in_all[i+LSBPos+1]=ByteRange_c[i];
	}
	Log(LOG_DEBUG, "Modification of ByteRange finished");
	for(i=LTPos+1; i<RTPos; i++){
		file_in_all[i]='0';
	}

	Log(LOG_INFO, "Compute the sign");
	char file_for_sign[rangeSize];
	for(i=0; i<ByteRange[1]; i++){
		file_for_sign[i]=file_in_all[i];
	}
	for(i=0; i<ByteRange[3]; i++){
		file_for_sign[i+ByteRange[1]]=file_in_all[ByteRange[2]+i];
	}
	
	char* sign_hex=ComputeSign(file_for_sign, rangeSize, privateKeyPath, publicKeyPath, certKeyPaths, numCerts);
	int signSize=strlen(sign_hex);
	if(signSize>signBufferSize){
		Log(LOG_ERROR, "SignBuffer was not enough");
		return -1;
	}
	for(i=0; i<signSize; i++){
		file_in_all[LTPos+i+1]=sign_hex[i];
	}
	
	ofstream* file_out=new ofstream(destPath, ios::binary);
	file_out->write(file_in_all, outputSize);
	file_out->close();
	
}

char* ComputeSign(char* str_for_sign, int strSize, char* privateKeyPath, char* publicKeyPath, char** certKeyPaths, int numCerts){
	int i;
	// private key
	Log(LOG_DEBUG, "Private key from %s", privateKeyPath);
	BIO* mem=BIO_new(BIO_s_mem());
	ifstream file_pri(privateKeyPath);
	file_pri.seekg(0, ios_base::end);
	int fileSize_pri=file_pri.tellg();
	Log(LOG_DEBUG, "File size: %d", fileSize_pri);
	file_pri.seekg(0, ios_base::beg);
	char data_pri[fileSize_pri];
	file_pri.read(data_pri, fileSize_pri);

	int bioResult=BIO_write(mem, data_pri, fileSize_pri);
	Log(LOG_DEBUG, "BIO size: %d", bioResult);

	EVP_PKEY* private_key=PEM_read_bio_PrivateKey(mem, NULL, NULL, NULL);
	ERR_print_errors_fp(stderr);

	// data
	Log(LOG_DEBUG, "Read data to BIO");
	BIO* data_for_sign=BIO_new(BIO_s_mem());
	int bioResult2=BIO_write(data_for_sign, str_for_sign, strSize);
	Log(LOG_DEBUG, "BIO size: %d", bioResult2);

	// public key (certificate)
	Log(LOG_DEBUG, "Public key (certificate) from %s", publicKeyPath);
	ifstream file_pub(publicKeyPath);
	file_pub.seekg(0, ios_base::end);
	int fileSize_pub=file_pub.tellg();
	Log(LOG_DEBUG, "File size: %d", fileSize_pub);
	file_pub.seekg(0, ios_base::beg);
	char data_pub[fileSize_pub];
	file_pub.read(data_pub, fileSize_pub);

	BIO* mem3=BIO_new(BIO_s_mem());
	int bioResult3=BIO_write(mem3, data_pub, fileSize_pub);
	Log(LOG_DEBUG, "BIO size: %d", bioResult3);

	X509* cert=PEM_read_bio_X509(mem3, NULL, 0, NULL);
	if(cert==NULL){
		Log(LOG_ERROR, "X509 failed");
		return NULL;
	}

	// certificate chains
	STACK_OF(X509)* certs=NULL;
	if(numCerts>0){
		certs=sk_X509_new_null();
		for(i=0; i<numCerts; i++){
			Log(LOG_DEBUG, "Certificate chain from %s", certKeyPaths[i]);
			ifstream file_chain(certKeyPaths[i]);
			file_chain.seekg(0, ios_base::end);
			int fileSize_chain=file_chain.tellg();
			Log(LOG_DEBUG, "File size: %d", fileSize_chain);
			file_chain.seekg(0, ios_base::beg);
			char data_chain[fileSize_chain];
			file_chain.read(data_chain, fileSize_chain);

			BIO* mem4=BIO_new(BIO_s_mem());
			int bioResult4=BIO_write(mem4, data_chain, fileSize_chain);
			Log(LOG_DEBUG, "BIO size: %d", bioResult4);

			X509* chain=PEM_read_bio_X509(mem4, NULL, 0, NULL);
			if(chain==NULL){
				Log(LOG_WARN, "X509 failed");
				break;
			}
			int pushResult=sk_X509_push(certs, chain);
			if(pushResult<=0){
				Log(LOG_WARN, "Push failed");
				break;
			}else{
				Log(LOG_DEBUG, "Number of chain elements: %d", pushResult);
			}
		}
	}
	
	CMS_ContentInfo* signed_data=CMS_sign(cert, private_key, certs, data_for_sign, CMS_DETACHED | CMS_BINARY | CMS_NOSMIMECAP);
	if(signed_data==NULL){
		Log(LOG_ERROR, "Sign failed");
		ERR_print_errors_fp(stderr);
		return NULL;
	}
	
	unsigned char* sign_binary=NULL;
	int sign_length=i2d_CMS_ContentInfo(signed_data, &sign_binary);
	Log(LOG_DEBUG, "Sign length: %d", sign_length);
	
	char* sign_hex=new char[sign_length*2+1];
	for(i=0; i<sign_length; i++){
		sprintf(&sign_hex[i*2], "%02x", sign_binary[i]);
	}
	return sign_hex;
}

int findString(ifstream* file, char* keyword){
	int currentPos=file->tellg();
	file->seekg(0, ios_base::end);
	int endPos=file->tellg();
	file->seekg(currentPos, ios_base::beg);

	int keylen=strlen(keyword);
	char a;
	int i;
	int keywordStartPos;
	while(currentPos<endPos){
		file->get(a);
		currentPos++;
		if(a==keyword[0]){
			bool flag=true;
			keywordStartPos=(int)file->tellg()-1;
			for(i=1; i<keylen; i++){
				file->get(a);
				currentPos++;
				if(a==keyword[i]){
					//ok
				}else{
					flag=false;
					break;
				}
			}
			if(flag==true){
				return keywordStartPos;
			}
		}
	}
	return -1;
}
