/* ReadFormData_main.cpp: an exectable to read form data in a PDF file
 */

#include <fstream>
#include <iostream>
#include <cstring>
#include <getopt.h>
#include <filesystem>

#ifndef LOG_INCLUDED
#include "log.hpp"
#endif

#include "ReadFormData_main.hpp"
#include "variables.hpp"
#include "PDFParser.hpp"

using namespace std;
int main(int argc, char** argv){
	// Read options
	char* inputFilePath=NULL;
	char* ownerPwd=NULL;
	char* userPwd=NULL;
	char* log_level_char=NULL;
	// Export options
	char* exportFilePath=NULL;

	int opt; // getopt result
	int longIndex; // index of the specified option
	opterr=0; // disable error output

	// constants
	int numRadioGroupsMax=64;
	
	while((opt=getopt_long(argc, argv, "hi:o:u:v:D:", longOpts, &longIndex))!=-1){
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
		case 'D':
			if(exportFilePath!=NULL){
				Log(LOG_ERROR, "Export file is already specified");
				return -1;
			}
			exportFilePath=optarg;
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

	if(exportFilePath==NULL){
		Log(LOG_ERROR, "Export file is not specified, see the help below");
		printf(help);
		return -1;
	}
	Log(LOG_INFO, "Export file is %s", exportFilePath);

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

	bool appendMode=false;
	if(filesystem::exists(exportFilePath)){
		appendMode=true;
		Log(LOG_INFO, "Export file already exists, append mode");
	}else{
		Log(LOG_INFO, "Export file does not exist, newly create it");
	}

	filesystem::path inputPath(inputFilePath);
  const char* inputFileName=inputPath.filename().c_str();
	Log(LOG_INFO, "InputFileName: %s", inputFileName);
	
	// Open export destination
	ofstream file(exportFilePath, ios::app | ios::binary);
	if(!file){
		Log(LOG_ERROR, "Failed in opening the output file");
		return false;
	}

	if(!appendMode){
		// UTF-8 BOM
		unsigned char BOM[3]={0xef, 0xbb, 0xbf};
		file.write((char*)BOM, 3);

		char* header=new char[128];
		strcpy(header, "FileName,Page,Left,Bottom,Right,Top,FieldName,FieldType,FieldValue\r\n");
		file.write(header, strlen(header));
	}
	char* buffer=new char[1024];
	char* format=new char[128];
	strcpy(format, "%s,%d,%.2f,%.2f,%.2f,%.2f,%s,%s,");
	
	// Read each page
	int i, j;
	int* exportedRadioGroups=new int[numRadioGroupsMax];
	int numExportedRadioGroups=0;
	int numPages=PP.GetPageSize();
	for(i=0; i<numPages; i++){
		Log(LOG_INFO, "Page %d", i);
		Array* Annots;
		if(PP.ReadPageDict(i, "Annots", (void**)&Annots, Type::Array, false)){
			int numAnnots=Annots->GetSize();
			for(j=0; j<numAnnots; j++){
				Dictionary* Annotation;
				unsigned char* Subtype;
				unsigned char* FieldType;
				unsigned int FieldFlag=0;
				PDFStr* FieldName;
				PDFStr* FieldName_u8;
				PDFStr* FieldName_u8csv;
				int* FieldFlagPointer;
				Array* Rect;
				double* RectValue=new double[4];
				double* RectPointer;
				bool error=false;
				if(!PP.Read(Annots, j, (void**)&Annotation, Type::Dict)){
					Log(LOG_ERROR, "Failed in reading Annots %d", j);
					continue;
				}
				if(!PP.Read(Annotation, "Subtype", (void**)&Subtype, Type::Name)){
					Log(LOG_ERROR, "Failed in reading Subtype");
					continue;
				}
				if(!unsignedstrcmp(Subtype, "Widget")){
					continue;
				}
				if(!PP.Read(Annotation, "Rect", (void**)&Rect, Type::Array)){
					Log(LOG_ERROR, "Failed in reading Rect");
					continue;
				}
				error=false;
				for(int x=0; x<4; x++){
					if(!PP.Read(Rect, x, (void**)&RectPointer, Type::Real)){
						Log(LOG_ERROR, "Failed in reading Rect elements");
						error=true;
						break;
					}else{
						RectValue[x]=*RectPointer;
					}
				}
				if(error){
					continue;
				}
				if(!PP.Read(Annotation, "T", (void**)&FieldName, Type::String, true)){
					Log(LOG_ERROR, "Failed in reading T");
					continue;
				}
				FieldName_u8=FieldName->ConvertToUTF8();
				FieldName_u8csv=FieldName_u8->CSVEscape();
				Log(LOG_INFO, "Annotation widget %s (#%d): (%.1f, %.1f), (%.1f, %.1f)",
						FieldName_u8->decrData, j, RectValue[0], RectValue[1], RectValue[2], RectValue[3]);

				if(!PP.Read(Annotation, "FT", (void**)&FieldType, Type::Name, true)){
					Log(LOG_ERROR, "Failed in reading FT");
					continue;
				}
				Log(LOG_INFO, "Field Type: %s", FieldType);
					
				if(!PP.Read(Annotation, "Ff", (void**)&FieldFlagPointer, Type::Int, true)){
					Log(LOG_WARN, "Failed in reading Ff or Ff not found");
				}else{
					FieldFlag=*FieldFlagPointer;
				}
				// Log(LOG_INFO, "Field flag: %d", FieldFlag);

				// NoExport check
				int NoExport=FieldFlag >> 2 & 1;
				if(NoExport==1){
					Log(LOG_WARN, "This form field shall not be exported");
					continue;
				}

				if(unsignedstrcmp(FieldType, "Tx")){
					// text field
					PDFStr* TxValue;
					if(Annotation->Search("V")<0){
						Log(LOG_WARN, "V not found in an annotation dictionary");
						TxValue=new PDFStr(0);
						TxValue->decrData[0]='\n';
					}else{
						if(!PP.Read(Annotation, "V", (void**)&TxValue, Type::String, true)){
							Log(LOG_ERROR, "Failed in reading V for Tx");
							continue;
						}
					}
					//if(LOG_LEVEL>=LOG_DEBUG){
					//	DumpPDFStr(TxValue);
					//}
					PDFStr* TxValue_u8=TxValue->ConvertToUTF8();
					PDFStr* TxValue_u8csv=TxValue_u8->CSVEscape();
					Log(LOG_INFO, "Value: %s", TxValue_u8->decrData);
					sprintf(buffer, format, inputFileName, i+1, RectValue[0], RectValue[1], RectValue[2], RectValue[3],
									FieldName_u8csv->decrData, "Text");
					file.write(buffer,strlen(buffer));
					file.write((char*)TxValue_u8csv->decrData, TxValue_u8csv->decrDataLen);
					file.write("\r\n", 2);
				}else if(unsignedstrcmp(FieldType, "Ch")){
					// choice field
					void* ChValue;
					int ChValueType;
					if(Annotation->Search("V")<0){
						Log(LOG_WARN, "V not found in an annotation dictrionary");
						ChValue=new PDFStr(0);
						((PDFStr*)ChValue)->decrData[0]='\n';
						ChValueType=Type::String;
					}else{
						if(!PP.Read(Annotation, "V", (void**)&ChValue, &ChValueType)){
							Log(LOG_ERROR, "Failed in reading V for Ch");
							continue;
						}
					}
					if(ChValueType==Type::String){
						PDFStr* ChValue_single=(PDFStr*)ChValue;
						PDFStr* ChValue_single_u8=ChValue_single->ConvertToUTF8();
						PDFStr* ChValue_single_u8csv=ChValue_single_u8->CSVEscape();
						Log(LOG_INFO, "Value: %s", ChValue_single_u8->decrData);
						sprintf(buffer, format, inputFileName, i+1, RectValue[0], RectValue[1], RectValue[2], RectValue[3],
										FieldName_u8csv->decrData, "Choice");
						file.write(buffer,strlen(buffer));
						file.write((char*)ChValue_single_u8csv->decrData, ChValue_single_u8csv->decrDataLen);
						file.write("\r\n", 2);
					}else if(ChValueType==Type::Array){
						PDFStr* ChValue_one;
						Array* ChValue_array=(Array*)ChValue;
						int ChValueCount=ChValue_array->GetSize();
						error=false;
						for(int k=0; k<ChValueCount; k++){
							if(!PP.Read(ChValue_array, k, (void**)&ChValue_one, Type::String)){
								Log(LOG_ERROR, "Failed in reading an element of V for Ch");
								error=true;
								break;
							}else{
								PDFStr* ChValue_one_u8=ChValue_one->ConvertToUTF8();
								PDFStr* ChValue_one_u8csv=ChValue_one_u8->CSVEscape();
								Log(LOG_INFO, "Value[%d]: %s", k, ChValue_one_u8->decrData);
								sprintf(buffer, format, inputFileName, i+1, RectValue[0], RectValue[1], RectValue[2], RectValue[3],
												FieldName_u8csv->decrData, "Choice");
								file.write(buffer,strlen(buffer));
								file.write((char*)ChValue_one_u8csv->decrData, ChValue_one_u8csv->decrDataLen);
								file.write("\r\n", 2);
							}
						}
						if(error){
							continue;
						}
					}
				}else if(unsignedstrcmp(FieldType, "Btn")){
					if((FieldFlag>>15 & 1)==1){
						// Radio
						// The parent holds the value
						Dictionary* Parent;
						Indirect* ParentRef;
						if(!(Annotation->Read("Parent", (void**)&ParentRef, Type::Indirect) &&
								 PP.Read(Annotation, "Parent", (void**)&Parent, Type::Dict))){
							Log(LOG_ERROR, "Error in reading Parent for RadioBtn");
							continue;
						}
						int parentObjNum=ParentRef->objNumber;
						Log(LOG_DEBUG, "Object number of the parent: %d", parentObjNum);
						bool alreadyExported=false;
						for(int k=0; k<numExportedRadioGroups; k++){
							if(exportedRadioGroups[k]==parentObjNum){
								alreadyExported=true;
								break;
							}
						}
						if(alreadyExported){
							Log(LOG_INFO, "This form field is already exported");
							continue;
						}
						exportedRadioGroups[numExportedRadioGroups]=parentObjNum;
						numExportedRadioGroups++;
						unsigned char* RadioValue;
						if(Parent->Search("V")<0){
							Log(LOG_WARN, "V not found in the parent for RadioBtn");
							RadioValue=new unsigned char[1];
							RadioValue[0]='\0';
						}else{
							if(!PP.Read(Parent, "V", (void**)&RadioValue, Type::Name)){
								Log(LOG_ERROR, "Failed in reading V for RadioBtn parent");
								continue;
							}
						}
						Log(LOG_INFO, "Value: %s", RadioValue);
						sprintf(buffer, format, inputFileName, i+1, RectValue[0], RectValue[1], RectValue[2], RectValue[3],
										FieldName_u8csv->decrData, "Radio");
						file.write(buffer,strlen(buffer));
						file.write((char*)RadioValue, unsignedstrlen(RadioValue));
						file.write("\r\n", 2);
					}else if((FieldFlag>>16 & 1)==1){
						// Push
						continue;
					}else{
						// Check
						unsigned char* CheckValue;
						if(Annotation->Search("V")<0){
							Log(LOG_WARN, "V not found in an annotation dictrionary");
							CheckValue=new unsigned char[4];
							unsignedstrcpy(CheckValue, "Off");
						}else{
							if(!PP.Read(Annotation, "V", (void**)&CheckValue, Type::Name)){
								Log(LOG_ERROR, "Failed in reading V for CheckBtn");
								continue;
							}
						}
						Log(LOG_INFO, "Value: %s", CheckValue);
						sprintf(buffer, format, inputFileName, i+1, RectValue[0], RectValue[1], RectValue[2], RectValue[3],
										FieldName_u8csv->decrData, "Check");
						file.write(buffer,strlen(buffer));
						file.write((char*)CheckValue, unsignedstrlen(CheckValue));
						file.write("\r\n", 2);
					}
				}
							
			}
		}else{
			Log(LOG_WARN, "Failed in reading Annots or Annots not found");
		}
	
	}

	// AcroForm
	/*
		Dictionary* dCatalog;
		if(PP.Read(&PP.trailer, "Root", (void**)&dCatalog, Type::Dict)){
		Dictionary* AcroForm;
		if(PP.Read(dCatalog, "AcroForm", (void**)&AcroForm, Type::Dict)){
		Array* Fields;
		if(PP.Read(AcroForm, "Fields", (void**)&Fields, Type::Array)){
		int numFields=Fields->GetSize();
		Dictionary* field;
		for(i=0; i<numFields; i++){
		Log(LOG_INFO, "Fields %d", i);
		if(PP.Read(Fields, i, (void**)&field, Type::Dict)){
		if(LOG_LEVEL>=LOG_DEBUG){
		field->Print();
		// /V
		unsigned char* V;
		if(PP.Read(field, "V", (void**)&V, Type::Name)){
		int Vlen=unsignedstrlen(V);
		for(j=0; j<Vlen; j++){
		printf("%02x ", V[j]);
		}
		printf("\n");
		}
		}
		}
		}
		}else{
		Log(LOG_ERROR, "Failed in reading Field in AcroForm");
		return -1;
		}
		}else{
		Log(LOG_WARN, "Failed in reading AcroForm or AcroForm not found");
		}
		}else{
		Log(LOG_ERROR, "Failed in reading Root");
		return -1;
		}*/
	
	return 0;
}
