// class PDFParser

#include <fstream>
#include <iostream>
#include <cstring>
#include <string>
#include <sstream>
#include <cstdlib>
#include "PDFParser.hpp"
#include "log.hpp"
#include "variables_ext.hpp"

using namespace std;
PDFParser::PDFParser(char* fileName):
	// Member initializer list
	file(fileName),
	error(false),
	encrypted(false),
	lastXRefStm(-1)
{
	// file check
	if(!file){
		Log(LOG_ERROR, "Cannot open the input file");
		error=true;
		return;
	}

	// find "%PDF-x.x"
	if(findHeader()==false){
		error=true;
		return;
	}

	// go to the end
	file.seekg(0, ios_base::end);
	fileSize=file.tellg();
	Log(LOG_INFO, "File size is %d bytes", fileSize);

	// find "%%EOL"
	if(!findFooter()){
		error=true;
		return;
	}
	// one line before
	backtoEOL();
	readInt(&lastXRef);
	Log(LOG_INFO, "Last XRef is at %d", lastXRef);
	// one line before xref
	backtoEOL();
	backtoEOL();
	// this line should be "startxref"
  if(!findSXref()){
		error=true;
		return;
	}

	// read trailer(s)
	int prevXRef=lastXRef;
	while(true){
		prevXRef=readTrailer(prevXRef);
		if(prevXRef==0){
			// This is the first Trailer
			break;
		}else if(prevXRef<0){
			// error
			error=true;
			return;
		}
	}
	if(LOG_LEVEL>=LOG_DEBUG){
		Log(LOG_DEBUG, "Trailer dictionary:");
		trailer.Print();
	}

	// prepare xref database
	int* Size;
  if(trailer.Read("Size", (void**)&Size, Type::Int)){
	}else{
		Log(LOG_ERROR, "Size of References cannot be read");
		error=true;
		return;
	}
	ReferenceSize=*(Size);
	Log(LOG_INFO, "Make XRef database with size %d", ReferenceSize);
	Reference=new Indirect*[ReferenceSize];
	int i;
	for(i=0; i<ReferenceSize; i++){
		Reference[i]=new Indirect();
	}
	prevXRef=lastXRef;
	while(true){
		prevXRef=readXRef(prevXRef);
		if(prevXRef==0){
			// This is the first Trailer
			break;
		}else if(prevXRef<0){
			error=true;
			return;
		}
	}

	Log(LOG_INFO, "Reference information:");
	for(i=0; i<ReferenceSize; i++){
		if(i!=Reference[i]->objNumber){
			Log(LOG_ERROR, "missing ref #%d", i);
			error=true;
			return;
		}
		if(LOG_LEVEL>=LOG_INFO){
			printf("Ref #%-3d: genNumber %-5d, status %4s, ",
					 i, Reference[i]->genNumber,
					 Reference[i]->used ? "used" : "free");
			if(Reference[i]->used){
				if(Reference[i]->objStream){
					printf("index %-3d of objStream (#%-3d)", Reference[i]->objStreamIndex, Reference[i]->objStreamNumber);
				}else{
					printf("position %d", Reference[i]->position);
				}
				if(Reference[i]->notEncrypted){
					printf(", not encrypted\n");
				}else{
					printf("\n");
				}
			}else{
				printf("\n");
			}
		}
	}
	
	// find /Encrypt dictionary in Document catalog dictionary
	Log(LOG_INFO, "Try to find /Encrypt in Document catalog dictionary");
	int encryptType;
	void* encryptValue;
	if(trailer.Search("Encrypt")>=0){
		Log(LOG_INFO, "This document is encrypted");
		Array* ID;
		// Since Encrypt dictionary is already found, the ID array should not be be an indirect object
		if(!trailer.Read("ID", (void**)&ID, Type::Array)){
			Log(LOG_ERROR, "Failed in reading ID");
			error=true;
			return;
		}
		Dictionary* encryptDict;
		Indirect* encryptDictRef;
		if(trailer.Read("Encrypt", (void**)&encryptDictRef, Type::Indirect) &&
			 Read(&trailer, "Encrypt", (void**)&encryptDict, Type::Dict)){
			Log(LOG_DEBUG, "Encrypt dictionary:");
			if(LOG_LEVEL>=LOG_DEBUG){
				encryptDict->Print();
			}
		}else{
			Log(LOG_ERROR, "Failed in read Encrypt");
			error=true;
			return;
		}
		encryptObj=new Encryption(encryptDict, ID);
		encryptObj_ex=new Encryption(encryptObj);
		encryptObjNum=encryptDictRef->objNumber;
		encrypted=true;
	}else{
		Log(LOG_INFO, "This document is not encrypted");
	}
}

// When the value>0 is given, judge whether the version is consistent with the PDF version
// When the value<0 is given, set the appropriate (highest) version in the current PDF version
bool PDFParser::JudgeEncrVersion(int* V){
	if(v_document.IsValid()){
		if(*V<0){
			switch(v_document.major){
			case 2:
				*V=5;
				break;
			case 1:
				switch(v_document.minor){
				case 1:
				case 2:
				case 3:
					*V=1;
					break;
				case 4:
					*V=2;
					break;
				case 5:
				case 6:
				case 7:
					*V=4;
					break;
				default:
					return false;
				}
				break;
			default:
				return false;
			}
			Log(LOG_INFO, "Highest encryption version is %d in PDF %s", *V, v_document.v);
			return true;
		}else{
			switch(*V){
			case 1:
				return v_document.major==1 && v_document.minor>=1 && v_document.minor<=7;
			case 2:
				return v_document.major==1 && v_document.minor>=4 && v_document.minor<=7;
			case 4:
				return v_document.major==1 && v_document.minor>=5 && v_document.minor<=7;
			case 5:
				return v_document.major==2;
			default:
				return false;
			}
		}
	}else{
		Log(LOG_ERROR, "PDF version is not yet determined");
		return false;
	}
}

// Read Version in Document catalog dictionary
bool PDFParser::ReadVersion(){
	Log(LOG_INFO, "Try to find /Version in Document catalog dictionary");
	Dictionary* dCatalog;
	if(trailer.Search("Root")<0){
		Log(LOG_ERROR, "Root not found in the trailer"); error=true; return false;
	}
	if(Read(&trailer, "Root", (void**)&dCatalog, Type::Dict)){
	  char* version;
		if(dCatalog->Search("Version")>=0){
			if(dCatalog->Read("Version", (void**)&version, Type::Name)){
				v_catalog.Set(version);
				Log(LOG_INFO, "Version in document catalog dictionary is %s", v_catalog.v);
				if(CompareVersions(v_catalog, v_header)){
					// catalog>=header
					v_document.Set(v_catalog.v);
				}else{
					// catalog<header
					v_document.Set(v_header.v);
				}
				Log(LOG_INFO, "Finally determined version is %s", v_document.v);
			}else{
				Log(LOG_ERROR, "Failed in reading Version"); error=true; return false;
			}
		}else{
			Log(LOG_INFO, "No version information in document catalog dictionary");
			v_document.Set(v_header.v);
			Log(LOG_INFO, "Finally determined version is %s", v_document.v);
		}
	}else{
		Log(LOG_WARN, "Failed in reading Document catalog dictionary");
		error=true; return false;
	}
	return true;
}

// Read Page information
bool PDFParser::ReadPages(){
	Log(LOG_INFO, "Read page information");
	Dictionary* dCatalog;
	if(trailer.Search("Root")<0){
		Log(LOG_ERROR, "Root not found in the trailer"); error=true; return false;
	}
	if(Read(&trailer, "Root", (void**)&dCatalog, Type::Dict)){
		// ok
	}else{
		Log(LOG_ERROR, "Failed in reading Document catalog dictionary");
	}
	// construct Pages array
	Dictionary* rootPages;
	Indirect* rootPagesRef;
	if(dCatalog->Read("Pages", (void**)&rootPagesRef, Type::Indirect) &&
		 Read(dCatalog, "Pages", (void**)&rootPages, Type::Dict)){
		// ok
	}else{
		Log(LOG_ERROR, "Failed in reading Pages"); error=true; return false;
	}
	int* totalPages=new int();
	if(rootPages->Read("Count", (void**)&totalPages, Type::Int)){
		Log(LOG_INFO, "Total pages: %d", *totalPages);
	}else{
		Log(LOG_ERROR, "Failed in reading Count in rootPages"); error=true; return false;
	}
	Pages=new Page*[*totalPages];
	PagesSize=*totalPages;
	int pageCount=0;
	if(!investigatePages(rootPagesRef, rootPages, &pageCount)){
		error=true;
		return false;
	}
	return true;
}

bool PDFParser::investigatePages(Indirect* pagesRef, Dictionary* pages, int* pageCount){  
	Array* kids;
	if(pages->Read("Kids", (void**)&kids, Type::Array)){
		int kidsSize=kids->GetSize();
		Dictionary* kid;
		Indirect* kidRef;
		int i;
		for(i=0; i<kidsSize; i++){
			if(kids->Read(i, (void**)&kidRef, Type::Indirect) &&
				 Read(kids, i, (void**)&kid, Type::Dict)){
				unsigned char* kidType;
				if(kid->Read("Type", (void**)&kidType, Type::Name)){
					if(unsignedstrcmp(kidType, "Page")){
						Pages[*pageCount]=new Page();
						Pages[*pageCount]->Parent=pagesRef;
						Pages[*pageCount]->PageDict=kid;
						Pages[*pageCount]->objNumber=kidRef->objNumber;
						*pageCount=*pageCount+1;
					}else if(unsignedstrcmp(kidType, "Pages")){
						if(!investigatePages(kidRef, kid, pageCount)){
							return false;
						}
					}
				}
			}else{
				Log(LOG_ERROR, "Failed in reading a kid"); return false;
			}
		}
	}else{
		Log(LOG_ERROR, "Failed in reading Kids"); return false;
	}
	return true;
}
int PDFParser::GetPageSize(){
	return PagesSize;
}

int PDFParser::AddNewReference(int type){
	// add an element to Reference and return the index
	int addElementIndex=ReferenceSize;
	ReferenceSize++;
	Indirect** Reference_new=new Indirect*[ReferenceSize];
	int i;
	for(i=0; i<ReferenceSize-1; i++){
		Reference_new[i]=Reference[i];
	}
	Indirect* addElement=new Indirect();
	addElement->objNumber=addElementIndex;
	addElement->genNumber=0;
	addElement->type=type;
	addElement->used=true;
	Reference_new[addElementIndex]=addElement;
	Reference=Reference_new;
	return addElementIndex;
}

bool PDFParser::ReadPageDict(int index, const char* key, void** value, int type, bool inheritable){
	int readType;
	if(ReadPageDict(index, key, value, &readType, inheritable)){
		if(readType==type){
			return true;
		}else{
			Log(LOG_ERROR, "Type mismatch in ReadPageDict");
		}
	}
	return false;
}
bool PDFParser::ReadPageDict(int index, const char* key, void** value, int* type, bool inheritable){
	Page* page=Pages[index];
	if(page->PageDict->Search(key)<0){
		// key not found
		if(inheritable){
			Indirect* parentRef=page->Parent;
			Dictionary* parent;
			while(true){
				if(ReadRefObj(parentRef, (void**)&parent, Type::Dict)){
					if(parent->Search(key)>=0){
						return Read(parent, key, value, type);
						return true;
					}
					if(parent->Read("Parent", (void**)&parentRef, Type::Indirect)){
						// ok
					}else{
						// parent not found
						return false;
					}
				}
			}
		}else{
			return false;
		}
	}else{
		return Read(page->PageDict, key, value, type);
	}
}

bool PDFParser::ReadRefObj(Indirect* ref, void** object, int* objType){
	int i;
	int objNumber=ref->objNumber;
	int genNumber=ref->genNumber;
	Log(LOG_DEBUG, "Read indirect reference: objNumber %d, genNumber %d", objNumber, genNumber);
	Indirect* refInRef=Reference[objNumber];
	if(refInRef->objNumber!=objNumber || refInRef->genNumber!=genNumber){
		Log(LOG_ERROR, "obj and gen numbers in XRef (%d, %d) is different from arguments (%d, %d)", refInRef->objNumber, refInRef->genNumber, objNumber, genNumber);
		return false;
	}
	if(refInRef->position<0){
		Log(LOG_DEBUG, "Value in the Indirect object is used");
		*objType=refInRef->type;
		*object=refInRef->value;
		return true;
	}
	if(refInRef->objStream){
		Log(LOG_DEBUG, "The object is in an object stream");
		Stream* objStream;
		Indirect objStreamReference;
		objStreamReference.objNumber=refInRef->objStreamNumber;
		objStreamReference.genNumber=0;
		void* objStreamValue;
		int objStreamType;
		if(!ReadRefObj(&objStreamReference, (void**)&objStream, Type::Stream)){
			Log(LOG_ERROR, "Failed in reading an object stream");
			return false;
		}
		if(encrypted && refInRef->notEncrypted==false){
			Log(LOG_DEBUG, "Decrypt stream");
			if(!encryptObj->DecryptStream(objStream)){
				Log(LOG_ERROR, "Error in decrypting stream");
				return false;
			}
		}
		objStream->Decode();

		// get the offset
		int* First;
		int* N;
		
		int index=refInRef->objStreamIndex;
		if(!objStream->StmDict.Read("First", (void**)&First, Type::Int)){
			Log(LOG_ERROR, "Failed in reading First");
			return false;
		}
		if(!objStream->StmDict.Read("N", (void**)&N, Type::Int)){
			Log(LOG_ERROR, "Failed in reading N");
			return false;
		}
		string data((char*)objStream->decoData, objStream->decoDataLen);
		istringstream is(data);
		int positionInObjStream;
		for(i=0; i<*N; i++){
			int v1, v2;
			readInt(&v1, &is);
			readInt(&v2, &is);
			// printf("%d %d\n", v1, v2);
			if(i==index){
				if(v1==objNumber){
					positionInObjStream=v2+*First;
					Log(LOG_DEBUG, "object #%d is at %d", v1, positionInObjStream);
					break;
				}else{
					Log(LOG_ERROR, "objNumber mismatch %d %d", v1, objNumber);
					return false;
				}
			}
		}
		is.seekg(positionInObjStream, ios_base::beg);
		*objType=judgeType(&is);
		Log(LOG_DEBUG, "ObjType is %d", *objType);
		switch(*objType){
		case Type::Bool:
			*object=new bool();
			if(!readBool((bool*)*object, &is)){
				Log(LOG_ERROR, "Failed in read Bool"); return false;
			}
			break;
		case Type::Int:
			*object=new int();
			if(!readInt((int*)*object, &is)){
				Log(LOG_ERROR, "Failed in read Int"); return false;
			}
			break;
		case Type::Real:
			*object=new double();
			if(!readReal((double*)*object, &is)){
				Log(LOG_ERROR, "Failed in read Real"); return false;
			}
			break;
		case Type::String:
			*object=readString(&is);
			if(*object==NULL){
				Log(LOG_ERROR, "Failed in read String"); return false;
			}
			break;
		case Type::Name:
			*object=readName(&is);
			if(*object==NULL){
				Log(LOG_ERROR, "Failed in read Name"); return false;
			}
			break;
		case Type::Array:
			*object=new Array();
			if(!readArray((Array*)*object, &is)){
				Log(LOG_ERROR, "Failed in read Array"); return false;
			}
			break;
		case Type::Dict:
			*object=new Dictionary();
			if(!readDict((Dictionary*)*object, &is)){
				Log(LOG_ERROR, "Failed in read Dict"); return false;
			}
			break;
		default:
			Log(LOG_ERROR, "Invalid type"); return false;
		}
		Log(LOG_DEBUG, "ReadRefObj from object stream finished");
	}else{
		Log(LOG_DEBUG, "The object is at %d", refInRef->position);
		file.seekg(refInRef->position+offset, ios_base::beg);
		int objNumber2;
		int genNumber2;
		if(!readObjID(&objNumber2, &genNumber2)){
			Log(LOG_ERROR, "Failed in reading object ID"); return false;
		}
		if(objNumber2!=objNumber || genNumber2!=genNumber){
			Log(LOG_ERROR, "Error: object or generation numbers are different"); return false;
		}
	  *objType=judgeType();
		Log(LOG_DEBUG, "ObjType is %d", *objType);
		int currentPos;
		Stream* streamValue;
		Dictionary* dictValue;
		switch(*objType){
		case Type::Bool:
			*object=new bool();
			if(!readBool((bool*)*object)){
				Log(LOG_ERROR, "Failed in reading Bool"); return false;
			}
			break;
		case Type::Int:
			*object=new int();
			if(!readInt((int*)*object)){
				Log(LOG_ERROR, "Failed in reading Int"); return false;
			}
			break;
		case Type::Real:
			*object=new double();
			if(!readReal((double*)*object)){
				Log(LOG_ERROR, "Failed in reading Real"); return false;
			}
			break;
		case Type::String:
			*object=readString();
			if(*object==NULL){
				Log(LOG_ERROR, "Failed in reading String"); return false;
			}
			break;
		case Type::Name:
			*object=readName();
			if(*object==NULL){
				Log(LOG_ERROR, "Failed in reading Name"); return false;
			}
			break;
		case Type::Array:
		  *object=new Array();
			if(!readArray((Array*)*object)){
				Log(LOG_ERROR, "Failed in reading Array"); return false;
			}
			break;
		case Type::Null:
			*object=NULL;
			break;
		case Type::Dict:
			// it can be a Dictionary and a Stream
			currentPos=(int)file.tellg();
			file.seekg(refInRef->position+offset, ios_base::beg);
			streamValue=new Stream();
			if(readStream(streamValue, false)){
				*objType=Type::Stream;
				*object=(void*)streamValue;
			}else{
				file.seekg(currentPos, ios_base::beg);
				dictValue=new Dictionary();
				if(readDict(dictValue)){
					*objType=Type::Dict;
					*object=(void*)dictValue;
				}else{
					Log(LOG_ERROR, "Failed in reading dictionary or stream"); return false;
				}
			}
			break;
		default:
			Log(LOG_ERROR, "Invalid type"); return false;
		}
		
		// encryption check
		// Contents in the Signature Dictionary should not be decrypted
		if(encrypted && refInRef->notEncrypted==false){
			// decryption of strings
			void* elementValue;
			int elementType;
			unsigned char* elementKey;
			bool isSigDict=false;
			unsigned char* DictName;
			switch(*objType){
			case Type::String:
				if(encryptObj->DecryptString((PDFStr*)(*object), objNumber, genNumber)){
					Log(LOG_DEBUG, "Decryption of a String finished");
				}else{
					Log(LOG_ERROR, "Failed in decrypting a String"); return false;
				}
				break;
			case Type::Array:
				for(i=0; i<((Array*)(*object))->GetSize(); i++){
					if(((Array*)(*object))->Read(i, &elementValue, &elementType)){
						if(elementType==Type::String){
							if(encryptObj->DecryptString((PDFStr*)elementValue, objNumber, genNumber)){
								Log(LOG_DEBUG, "Decryption of a String in an Array finished");
							}else{
								Log(LOG_ERROR, "Failed in decrypting a String in an Array"); return false;
							}
						}
					}else{
						Log(LOG_ERROR, "Failed in reading an Array"); return false;
					}
				}
				break;
			case Type::Dict:
				if(((Dictionary*)(*object))->Search("Type")>=0 && ((Dictionary*)(*object))->Read("Type", (void**)&DictName, Type::Name)){
					if(unsignedstrcmp(DictName, "Sig")){
						Log(LOG_DEBUG, "The Dictionary is a Signature Dictionary");
						isSigDict=true;
					}
				}
				for(i=0; i<((Dictionary*)(*object))->GetSize(); i++){
					if(((Dictionary*)(*object))->Read(i, &elementKey, &elementValue, &elementType)){
						if(elementType==Type::String && !unsignedstrcmp(elementKey, "Contents")){
							if(encryptObj->DecryptString((PDFStr*)elementValue, objNumber, genNumber)){
								Log(LOG_DEBUG, "Decryption of a String in a Dictionary finished");
							}else{
								Log(LOG_ERROR, "Failed in decrypting a String in a Dictionary"); return false;
							}
						}
					}else{
						Log(LOG_ERROR, "Failed in reading a Dictionary"); return false;
					}
				}
				break;
			case Type::Stream:
				if(!encryptObj->DecryptStream((Stream*)*object)){
					Log(LOG_ERROR, "Failed in deryptiong a Stream"); return false;
				}
				Log(LOG_DEBUG, "Decrption of a Stream finished");
			}
		}
		if(*objType==Type::Stream){
			((Stream*)(*object))->Decode();
		}
		Log(LOG_DEBUG, "ReadRefObj finished");
	}
	refInRef->position=-1;
	refInRef->type=*objType;
	refInRef->value=*object;
	return true;
}
bool PDFParser::ReadRefObj(Indirect* ref, void** object, int objType){
	int readType;
	if(ReadRefObj(ref, object, &readType)){
		if(readType==objType){
			return true;
		}else{
			Log(LOG_ERROR, "Type mismatch in ReadRefObj");
		}
	}
	return false;
}


bool PDFParser::Read(Dictionary* dict, const char* key, void** value, int type){
	int readType;
	if(Read(dict, key, value, &readType)){
		if(readType==type){
			return true;
		}else{
			Log(LOG_ERROR, "Type mismatch in PDFParser::Read");
		}
	}
	return false;
}

bool PDFParser::Read(Dictionary* dict, const char* key, void** value, int* type){
	void* readValue;
	int readType;
	if(dict->Read(key, (void**)&readValue, &readType)){
		if(readType==Type::Indirect){
			Indirect* readRef=(Indirect*) readValue;
			if(ReadRefObj(readRef, (void**)&readValue, type)){
				*value=readValue;
				return true;		
			}else{
				return false;
			}
		}else{
			*type=readType;
			*value=readValue;
			return true;
		}
	}
	return false;
}


bool PDFParser::Read(Array* array, int index, void** value, int type){
	void* readValue;
	int readType;
	if(array->Read(index, (void**)&readValue, &readType)){
		if(readType==Type::Indirect){
			Indirect* readRef=(Indirect*) readValue;
			if(ReadRefObj(readRef, (void**)&readValue, type)){
				*value=readValue;
				return true;		
			}else{
				return false;
			}
		}else if(readType==type){
			*value=readValue;
			return true;
		}
	}else{
		return false;
	}
	return false;
}
 
bool PDFParser::AuthUser(char* pwd){
	int pwdLen=strlen(pwd);
	PDFStr* pwdStr=new PDFStr(pwdLen);
  unsignedstrcpy(pwdStr->decrData, (unsigned char*)pwd);
	return encryptObj->AuthUser(pwdStr);
}

bool PDFParser::AuthOwner(char* pwd){
	int pwdLen=strlen(pwd);
	PDFStr* pwdStr=new PDFStr(pwdLen);
  unsignedstrcpy(pwdStr->decrData, (unsigned char*)pwd);
	return encryptObj->AuthOwner(pwdStr);
}

bool PDFParser::HasError(){
	return error;
}

bool PDFParser::IsEncrypted(){
	return encrypted;
}

bool PDFParser::IsAuthenticated(){
	if(encrypted){
		return encryptObj->IsAuthenticated();
	}else{
		return true;
	}
}

bool PDFParser::isSpace(char a){
	return a==NUL || a==TAB || a==LF || a==FF || a==CR || a==SP;
}

bool PDFParser::isEOL(char a){
	return a==CR || a==LF;
}

bool PDFParser::isDelimiter(char a){
	return a=='(' || a==')' || a=='<' || a=='>' || a=='[' || a==']' || a=='/' || a=='%' || a=='{' || a=='}';
}

bool PDFParser::isOctal(char a){
	return a=='0' || a=='1' || a=='2' || a=='3' || a=='4' || a=='5' || a=='6' || a=='7';
}

// find "%PDF-x.y" header
bool PDFParser::findHeader(){
	// go to the top
	file.seekg(0, ios_base::beg);
	char a;
	int line=0;
	while(true){
		// investigate a line, whether the line includes the header or not
		int i;
		bool isNotHeader=false;
		for(i=0; i<strlen(HEADER); i++){
			file.get(a);
			if(a!=HEADER[i]){
				Log(LOG_INFO, "Find something before the header");
				if(a==CR || a==LF){
					file.seekg(-1, ios_base::cur);
				}
				gotoEOL();
				isNotHeader=true;
				break;
			}
		}
		if(isNotHeader){
			continue;
		}
		// after "%PDF-" is "x.y"
		char version[3];
		for(i=0; i<3; i++){
			file.get(version[i]);
			if(isEOL(version[i])){
				Log(LOG_ERROR, "Invalid version label");
				return false;
			}
		}
		// offset is the position of '%'
		offset=(int)file.tellg()-8;
		if(!v_header.Set(version)){
			return false;
		}
		if(!gotoEOL(1)){
			return false;
		}
		Log(LOG_INFO, "Version in the header is %s", v_header.v);
		Log(LOG_INFO, "Offset is %d bytes", offset);
		break;
	}	
	return true;
}

// EOL: CR, LF, CR+LF
bool PDFParser::gotoEOL(){
	return gotoEOL(0);
}

// Move the cursor forward until EOL character(s) are found
// flag
// 0: any character is ok before EOL
// 1: white space characters are ok
// 2: nothing is ok
bool PDFParser::gotoEOL(int flag){
	char a;
	// check whether the pointer is at EOF
	int currentPos=(int) file.tellg();
	file.seekg(0, ios_base::end);
	int endPos=(int)file.tellg();
	file.seekg(currentPos, ios_base::beg);
	if(currentPos==endPos){
		Log(LOG_WARN, "The current position is EOF");
		return true;
	}
	while(true){
		file.get(a);
		if(flag==1){
			if(!isSpace(a)){
				Log(LOG_ERROR, "Invalid character, other than white-space");
				return false;
			}
		}else if(flag==2){
			if(!isEOL(a)){
				Log(LOG_ERROR, "Invalid character, other than CR or LF");
				return false;
			}
		}
		if(a==CR){
			file.get(a);
			if(a!=LF){
				file.seekg(-1, ios_base::cur);
			}
			return true;
		}else if(a==LF){
			return true;
		}else{
			currentPos==(int)file.tellg();
			if(currentPos==endPos){
				Log(LOG_INFO, "The cursor reached to EOF");
				return true;
			}
		}
	}
}

// Find "%%EOF" footer
bool PDFParser::findFooter(){
	file.seekg(0, ios_base::end);
	backtoEOL();
	int i;
	char a;
	for(i=0; i<strlen(FOOTER); i++){
		file.get(a);
		if(a!=FOOTER[i]){
			Log(LOG_ERROR, "Footer not found");
			return false;
		}
	}
	if(!gotoEOL(1)){
		Log(LOG_ERROR, "Invalid character in footer");
		return false;
	}
	backtoEOL();
	return true;
}

// if the pointer is just after EOL, back to another EOL before
// if the pointer is not on EOL, back to the last EOL
void PDFParser::backtoEOL(){
	char a;
	file.seekg(-1, ios_base::cur);
	file.get(a);
	if(isEOL(a)){
		// the pointer is just after EOL
		if(a==LF){
			file.seekg(-2, ios_base::cur);
			file.get(a);
			if(a==CR){
				// EOL is CR+LF
				file.seekg(-1, ios_base::cur);
			}else{
				// EOL is LF
			}
		}else{
			// EOL is CR
		}
	}else{
		// the pointer is not on EOL
	}
	while(true){
		file.seekg(-1, ios_base::cur);
		file.get(a);
		if(isEOL(a)){
			return;
		}else{
			file.seekg(-1, ios_base::cur);
			// cout << a << endl;
		}
	}
}

// find "startxref" line
bool PDFParser::findSXref(){
	int i;
	char a;
	for(i=0; i<strlen(SXREF); i++){
		file.get(a);
		if(a!=SXREF[i]){
			Log(LOG_ERROR, "startxref not found");
			return false;
		}
	}
	if(!gotoEOL(1)){
		Log(LOG_ERROR, "Invalid character in the startxref line");
		return false;
	}
	backtoEOL();
	return true;
}

void PDFParser::readLine(char* buffer, int n){
	// n is buffer size, so (n-1) characters other than \0 can be read
	char a;
	int i;
	for(i=0; i<n-1; i++){
		file.get(a);
		if(!isEOL(a)){
			buffer[i]=a;
		}else{
			buffer[i]='\0';
			file.seekg(-1, ios_base::cur);
			break;
		}
	}
	buffer[n-1]='\0';
	gotoEOL();
}

// read trailer dictionary
int PDFParser::readTrailer(int position){
	Log(LOG_DEBUG, "Read trailer at %d", position);
	file.seekg(position+offset, ios_base::beg);

  // XRef table: begins with "xref" line
	bool XRefTable=true;
	int i;
	char a;
	for(i=0; i<strlen(XREF); i++){
		file.get(a);
		if(a!=XREF[i]){
			XRefTable=false;
			break;
		}
	}
	if(XRefTable && !gotoEOL(1)){
		XRefTable=false;
	}
	file.seekg(position+offset, ios_base::beg);
	
	// XRef stream: begins with "%d %d obj" line
	int objNumber;
	int genNumber;
	bool XRefStream=readObjID(&objNumber, &genNumber);
	file.seekg(position+offset, ios_base::beg);

	if(!XRefTable && !XRefStream){
		Log(LOG_ERROR, "None of XRef Table or XRef Stream");
		return -1;
	}

	char buffer[32];
	if(XRefTable){
		Log(LOG_DEBUG, "The trailer is XRef table & trailer dictionary type");
		// skip XRef table
	  int i;
		char a;
		while(true){
			bool isNotTrailer=false;
			for(i=0; i<strlen(TRAIL); i++){
				file.get(a);
				if(a!=TRAIL[i]){
					isNotTrailer=true;
					break;
				}
			}
			if(isNotTrailer){
				gotoEOL();
				continue;
			}
			if(!gotoEOL(1)){
				Log(LOG_ERROR, "Invalid character in the trailer line");
				return -1;
			}
			break;
		}
	}else{
		Log(LOG_DEBUG, "The trailer is XRef stream type");
		// skip 'objNumber, genNumber, obj' line
		readLine(buffer, 32);
	}

	// read Trailer dictionary
	Dictionary trailer2;
	if(!readDict(&trailer2)){
		return -1;
	}
	// merge trailer2 to trailer
	trailer.Merge(trailer2);
	
	// check Prev key exists
  int* PrevValue;
	if(trailer2.Read("Prev", (void**)&PrevValue, Type::Int)){
		return *PrevValue;
	}
	return 0;
}


int PDFParser::readXRef(int position){
	Log(LOG_DEBUG, "Read XRef at %d", position);
	file.seekg(position+offset, ios_base::beg);

  // XRef table: begins with "xref" line
	bool XRefTable=true;
	int i, j, k;
	char a;
	for(i=0; i<strlen(XREF); i++){
		file.get(a);
		if(a!=XREF[i]){
			XRefTable=false;
			break;
		}
	}
	if(XRefTable && !gotoEOL(1)){
		XRefTable=false;
	}
	file.seekg(position+offset, ios_base::beg);
	
	// XRef stream: begins with "%d %d obj" line
	int objNumber;
	int genNumber;
	bool XRefStream=readObjID(&objNumber, &genNumber);
	file.seekg(position+offset, ios_base::beg);

	if(!XRefTable && !XRefStream){
		Log(LOG_ERROR, "None of XRef Table or XRef Stream");
		return -1;
	}

	char buffer[32];
	Dictionary trailer2;
	if(XRefTable){
		Log(LOG_DEBUG, "The XRef is table type");
		// skip "xref" row
		readLine(buffer, 32);
		int objNumber;
		int numEntries;
		char position_c[11];
		char genNumber_c[6];
		char used_c[2];
		char* strtolPointer;
		position_c[10]='\0';
		genNumber_c[6]='\0';
		used_c[1]='\0';
		while(true){
			// check whether the line is "trailer" (end of xref table)
			int currentPosition=(int) file.tellg();
			bool isNotTrailer=false;
			for(i=0; i<strlen(TRAIL); i++){
				file.get(a);
				if(a!=TRAIL[i]){
					isNotTrailer=true;
					break;
				}
			}
			if(isNotTrailer){
				file.seekg(currentPosition, ios_base::beg);
			}else{
				if(!gotoEOL(1)){
					Log(LOG_ERROR, "Invalid character in the trailer line");
					return -1;
				}else{
					break;
				}
			}
			bool a1=readInt(&objNumber);
			bool a2=readInt(&numEntries);
			bool a3=gotoEOL(1);
			// cout << a1 << a2 << a3 << endl;
			if(!(a1 && a2 && a3)){
				Log(LOG_ERROR, "Failed in reading xref table header");
				return -1;
			}
			for(i=0; i<numEntries; i++){
				// xref table content: (offset 10 bytes) (genNumber 5 bytes) (n/f)(EOL 2 bytes)
				int currentObjNumber=objNumber+i;
				if(currentObjNumber>=ReferenceSize){
					break;
				}
				file.read(position_c, 10);
				file.get(a);
				file.read(genNumber_c, 5);
				file.get(a);
				file.read(used_c, 1);
				file.get(a);
				file.get(a);
				// cout << offset_c << " | " << genNumber_c << endl;
				// check a is CR of LF
				if(!isEOL(a)){
					return -1;
				}
				if(Reference[currentObjNumber]->objNumber>=0){
					// reference is already read
					Log(LOG_WARN, "XRef #%d is already read\n", currentObjNumber);
					continue;
				}
				Reference[currentObjNumber]->objNumber=currentObjNumber;
				// convert string to integer
				Reference[currentObjNumber]->position=strtol(position_c, &strtolPointer, 10);
				if(strtolPointer!=&position_c[10]){
					Log(LOG_ERROR, "Failed in parsing position");
					return -1;
				}
				Reference[currentObjNumber]->genNumber=strtol(genNumber_c, &strtolPointer, 10);
				if(strtolPointer!=&genNumber_c[5]){
					Log(LOG_ERROR, "Error in parsing genNumber");
					return -1;
				}
				// convert used (n/f)
				if(strcmp(used_c, "n")==0){
					Reference[currentObjNumber]->used=true;
				}else if(strcmp(used_c, "f")==0){
					Reference[currentObjNumber]->used=false;
				}else{
					Log(LOG_ERROR, "Invalid n/f value");
					return -1;
				}					
			}
		}
		// read Trailer dictionary
		if(!readDict(&trailer2)){
			return -1;
		}
		// if XRefStm entry exists in the Trailer dictionary, read it
		int* XRefStm;
		if(trailer2.Read((unsigned char*)"XRefStm", (void**)&XRefStm, Type::Int)){
			Log(LOG_DEBUG, "Read hybrid XRef stream");
			if(readXRef(*XRefStm)<0){
				return -1;
			}
		}
		// check Prev key exists
		int* Prev;
		if(trailer2.Read((unsigned char*)"Prev", (void**)&Prev, Type::Int)){
			return *Prev;
		}
	}else{
		// read xref stream
		Log(LOG_DEBUG, "The XRef is stream type");
		Stream XRefStm;
		if(!readStream(&XRefStm)){
			return -1;
		}
		// Set notEncrypted flag in Reference
		int XRefStm_objNumber=XRefStm.objNumber;
		Reference[XRefStm_objNumber]->notEncrypted=true;
		
		if(LOG_LEVEL>=LOG_DEBUG){
			Log(LOG_DEBUG, "Stream dictionary:");
			XRefStm.StmDict.Print();
		}
		if(lastXRefStm<0){
			// this is the last XRef stream
			lastXRefStm=XRefStm.objNumber;
			Log(LOG_DEBUG, "This is the last XRef stream");
		}
		
		// decode xref stream
		if(!XRefStm.Decode()){
			return -1;
		}

		// read W
		int field[3];
		int sumField=0;
		Array* WArray;
		int* fieldValue;
		if(XRefStm.StmDict.Read("W", (void**)&WArray, Type::Array)){
			for(i=0; i<3; i++){
				if(WArray->Read(i, (void**)&fieldValue, Type::Int)){
					field[i]=*fieldValue;
					sumField+=field[i];
				}else{
					Log(LOG_ERROR, "Failed in reading W element");
					return -1;
				}
			}
		}else{
			Log(LOG_ERROR, "Failed in reading W");
			return -1;
		}

		// read Index
		int numSubsections=1;
		bool indexExists=false;
		Array* IndexArray;
		if(XRefStm.StmDict.Search("Index")>=0){
			if(XRefStm.StmDict.Read("Index", (void**)&IndexArray, Type::Array)){
				int IndexArraySize=IndexArray->GetSize();
				if(IndexArraySize%2!=0){
					Log(LOG_ERROR, "The Index array contains odd number of elements");
					return -1;
				}
				numSubsections=IndexArraySize/2;
				indexExists=true;
			}else{
				Log(LOG_WARN, "Failed in reading the Index array");
			}
		}
				
		int firstIndex[numSubsections];
		int numEntries[numSubsections];
		int* indexElement;
		if(indexExists){
			for(i=0; i<numSubsections; i++){
				if(IndexArray->Read(2*i, (void**)&indexElement, Type::Int)){
					firstIndex[i]=*(indexElement);
				}else{
					Log(LOG_ERROR, "Failed in reading Index element");
					return -1;
				}
				if(IndexArray->Read(2*i+1, (void**)&indexElement, Type::Int)){
					numEntries[i]=*(indexElement);
				}else{
					Log(LOG_ERROR, "Failed in reading Index element");
					return -1;
				}
			}
		}else{
			// if Index does not exist, the default value is [0 Size]
			int* Size;
			firstIndex[0]=0;
			if(XRefStm.StmDict.Read((unsigned char*)"Size", (void**)&Size, Type::Int)){
				numEntries[0]=*(Size);
			}else{
				Log(LOG_ERROR, "Failed in reading Size");
				return -1;
			}
		}
		
		int totalEntries=0;
		for(i=0; i<numSubsections; i++){
			totalEntries+=numEntries[i];
		}
		Log(LOG_DEBUG, "Decoded XRef has %d bytes, %d entries", XRefStm.decoDataLen, totalEntries);
		if(LOG_LEVEL>=LOG_DEBUG){
			for(i=0; i<XRefStm.decoDataLen; i++){
				printf("%02x ", (unsigned int)XRefStm.decoData[i]);
				if((i+1)%sumField==0){
					cout << endl;
				}
			}
		}

		// record the information to Reference
		if(XRefStm.decoDataLen/sumField!=totalEntries){
			Log(LOG_ERROR, "Size mismatch in decoded data");
			return -1;
		}
		int XRefStmIndex=0;
		int XRefStmValue[3];
		int i2;
		for(i2=0; i2<numSubsections; i2++){
			for(i=0; i<numEntries[i2]; i++){
				for(j=0; j<3; j++){
					XRefStmValue[j]=0;
					for(k=0; k<field[j]; k++){
						XRefStmValue[j]=256*XRefStmValue[j]+(unsigned int)XRefStm.decoData[XRefStmIndex];
						XRefStmIndex++;
					}
				}
				// printf("%d %d %d\n", XRefStmValue[0], XRefStmValue[1], XRefStmValue[2]);
				int currentObjNumber=i+firstIndex[i2];
				if(Reference[currentObjNumber]->objNumber>=0){
					Log(LOG_WARN, "XRef #%d is already read", currentObjNumber);
					continue;
				}
				switch(XRefStmValue[0]){
				case 0:
					// free object
					Reference[currentObjNumber]->used=false;
					Reference[currentObjNumber]->objNumber=currentObjNumber;
					Reference[currentObjNumber]->position=XRefStmValue[1];
					Reference[currentObjNumber]->genNumber=XRefStmValue[2];
					break;
				case 1:
					// used object
					Reference[currentObjNumber]->used=true;
					Reference[currentObjNumber]->objNumber=currentObjNumber;
					Reference[currentObjNumber]->position=XRefStmValue[1];
					Reference[currentObjNumber]->genNumber=XRefStmValue[2];
					break;
				case 2:
					// object in compressed object
					Reference[currentObjNumber]->used=true;
					Reference[currentObjNumber]->objStream=true;
					Reference[currentObjNumber]->objNumber=currentObjNumber;
					Reference[currentObjNumber]->objStreamNumber=XRefStmValue[1];
					Reference[currentObjNumber]->objStreamIndex=XRefStmValue[2];
				}
			}
		}

		// check Prev key exists
		int* Prev;
		if(XRefStm.StmDict.Read("Prev", (void**)&Prev, Type::Int)){
			return *(Prev);
		}
	}
	return 0;
}


bool PDFParser::readObjID(int* objNumber, int* genNumber){
	// Indirect object identifier row: %d %d obj
	readInt(objNumber);
	readInt(genNumber);
	char buffer[32];
	if(!readToken(buffer, 32)){
		return false;
	}
	if(strcmp(buffer, "obj")!=0){
		return false;
	}
	if(!gotoEOL(1)){
		return false;
	}
	if(*objNumber>0 && *genNumber>=0){
		return true;
	}else{
		return false;
	}
}

bool PDFParser::readRefID(int* objNumber, int* genNumber){
	return readRefID(objNumber, genNumber, &file);
}
bool PDFParser::readRefID(int* objNumber, int* genNumber, istream* is){
	// Indirect object reference row: %d %d R
	readInt(objNumber, is);
	readInt(genNumber, is);
	char buffer[32];
	if(!readToken(buffer, 32, is)){
		return false;
	}
	if(strcmp(buffer, "R")!=0){
		return false;
	}
	if(*objNumber>0 && *genNumber>=0){
		return true;
	}else{
		return false;
	}
}

bool PDFParser::readInt(int* value){
	return readInt(value, &file);
}
bool PDFParser::readInt(int* value, istream* is){
	char buffer[32];
	if(!readToken(buffer, 32, is)){
		return false;
	}
	char* pointer;
	*value=strtol(buffer, &pointer, 10);
	if(pointer==&buffer[strlen(buffer)]){
		return true;
	}else{
		return false;
	}
}

bool PDFParser::readBool(bool* value){
	return readBool(value, &file);
}
bool PDFParser::readBool(bool* value, istream* is){
	char buffer[32];
	if(!readToken(buffer, 32, is)){
		return false;
	}
	if(strcmp(buffer, "true")==0){
		*value=true;
	}else if(strcmp(buffer, "false")==0){
		*value=false;
	}else{
		return false;
	}
	return true;
}

bool PDFParser::readReal(double* value){
	return readReal(value, &file);
}
bool PDFParser::readReal(double* value, istream* is){
	char buffer[32];
	if(!readToken(buffer, 32, is)){
		return false;
	}
	char* pointer;
	*value=strtod(buffer, &pointer);
	if(pointer==&buffer[strlen(buffer)]){
		return true;
	}else{
		return false;
	}
}

bool PDFParser::readToken(char* buffer, int n){
	return readToken(buffer, n, &file);
}
bool PDFParser::readToken(char* buffer, int n, istream* is){
	if(!skipSpaces(is)){
		return false;
	}
	char a;
	int i=0;
	while(true){
		is->get(a);
		if(isSpace(a) || isDelimiter(a)){
			buffer[i]='\0';
			is->seekg(-1, ios_base::cur);
			return true;
		}else{
			buffer[i]=a;
			i++;
			if(i>=n-1){
				// token is too long
				buffer[n-1]='\0';
				return false;
			}
		}
	}
}


bool PDFParser::skipSpaces(){
	return skipSpaces(&file);
}		
bool PDFParser::skipSpaces(istream* is){
	char a;
	int pos=(int) is->tellg();
	// cout << pos << endl;
	while(true){
		is->get(a);
		// cout << a << endl;
		if(isSpace(a)){
			continue;
		}else if(a==EOF){
			return false;
		}else{
			break;
		}
	}
	is->seekg(-1, ios_base::cur);
	return true;
}

bool PDFParser::readDict(Dictionary* dict){
	return readDict(dict, &file);
}
bool PDFParser::readDict(Dictionary* dict, istream* is){
	// begins with "<<"
	if(!skipSpaces(is)){
		return false;
	}
	if(judgeDelimiter(true, is)!=Delimiter::LLT){
		return false;
	}
	
	// key (name), value (anything)
	while(true){
		int delim=judgeDelimiter(false, is);
		if(delim==Delimiter::SOL){
			// /name
		  unsigned char* name=readName(is);
			if(name==NULL){
				return false;
			}
			// value
			int type=judgeType(is);
			// printf("Name: %s, Type: %d\n", name, type);
			char buffer[128];
			PDFStr* stringValue;
			int* intValue;
			bool* boolValue;
			double* realValue;
			unsigned char* nameValue;
			int* objNumValue;
			int* genNumValue;
			Indirect* indirectValue;
			Dictionary* dictValue;
			Array* arrayValue;
			switch(type){
			case Type::Bool:
				boolValue=new bool();
				if(!readBool(boolValue, is)){
					cout << "Error in read bool" << endl;
					return false;
				}
				dict->Append(name, boolValue, type);
				break;
			case Type::Int:
				intValue=new int();
				if(!readInt(intValue, is)){
					cout << "Error in read integer" << endl;
					return false;
				}
				dict->Append(name, intValue, type);
				break;
			case Type::Real:
				realValue=new double();
				if(!readReal(realValue, is)){
					cout << "Error in read real number" << endl;
					return false;
				}
				dict->Append(name, realValue, type);
				break;
			case Type::Null:
				readToken(buffer, 128, is);
				break;
			case Type::String:
			  stringValue=readString(is);
				if(stringValue==NULL){
					cout << "Error in read string" << endl;
					return false;
				}
				dict->Append(name, stringValue, type);
				break;
			case Type::Name:
			  nameValue=readName(is);
				if(nameValue==NULL){
					cout << "Error in read name" << endl;
					return false;
				}
				dict->Append(name, nameValue, type);
				break;
			case Type::Indirect:
				indirectValue=new Indirect();
				if(!readRefID(&(indirectValue->objNumber), &(indirectValue->genNumber), is)){
					cout << "Error in read indirect object" << endl;
					return false;
				}
				dict->Append(name, indirectValue, type);
				break;
			case Type::Dict:
				dictValue=new Dictionary();
				if(!readDict(dictValue, is)){
					cout << "Error in read dictionary" << endl;
					return false;
				}
				dict->Append(name, dictValue, type);
				break;
			case Type::Array:
				arrayValue=new Array();
				if(!readArray(arrayValue, is)){
					cout << "Error in read array" << endl;
					return false;
				}
				dict->Append(name, arrayValue, type);
				break;
			}
			//dict->Print();
			//break;
		}else if(delim==Delimiter::GGT){
			// >> (end of dictionary)
			judgeDelimiter(true, is);
			break;
		}else{
			cout << "SOMETHING STRANGE" << endl;
			return NULL;
		}
	}
	return true;
}

bool PDFParser::readArray(Array* array){
	return readArray(array, &file);
}
bool PDFParser::readArray(Array* array, istream* is){
	// begins with "["
	if(!skipSpaces(is)){
		return false;
	}
	if(judgeDelimiter(true, is)!=Delimiter::LSB){
		return false;
	}
	
	// value (anything)
	while(true){
		int delim=judgeDelimiter(false, is);
		if(delim!=Delimiter::RSB){
			int type=judgeType(is);
			// printf("Type: %d\n", type);
			char buffer[128];
			PDFStr* stringValue;
			int* intValue;
			bool* boolValue;
			double* realValue;
			unsigned char* nameValue;
			int* objNumValue;
			int* genNumValue;
			Indirect* indirectValue;
			Dictionary* dictValue;
			Array* arrayValue;
			switch(type){
			case Type::Bool:
				boolValue=new bool();
				if(!readBool(boolValue, is)){
					cout << "Error in read bool" << endl;
					return false;
				}
				array->Append(boolValue, type);
				break;
			case Type::Int:
				intValue=new int();
				if(!readInt(intValue, is)){
					cout << "Error in read integer" << endl;
					return false;
				}
				array->Append(intValue, type);
				break;
			case Type::Real:
				realValue=new double();
				if(!readReal(realValue, is)){
					cout << "Error in read real number" << endl;
					return false;
				}
				array->Append(realValue, type);
				break;
			case Type::Null:
				readToken(buffer, 128, is);
				array->Append(NULL, type);
				break;
			case Type::String:
			  stringValue=readString(is);
				if(stringValue==NULL){
					cout << "Error in read string" << endl;
					return false;
				}
				array->Append(stringValue, type);
				break;
			case Type::Name:
			  nameValue=readName(is);
				if(nameValue==NULL){
					cout << "Error in read name" << endl;
					return false;
				}
				array->Append(nameValue, type);
				break;
			case Type::Indirect:
				indirectValue=new Indirect();
				if(!readRefID(&(indirectValue->objNumber), &(indirectValue->genNumber), is)){
					cout << "Error in read indirect object" << endl;
					return false;
				}
				array->Append(indirectValue, type);
				break;
			case Type::Dict:
				dictValue=new Dictionary();
				if(!readDict(dictValue, is)){
					cout << "Error in read dictionary" << endl;
					return false;
				}
				array->Append(dictValue, type);
				break;
			case Type::Array:
				arrayValue=new Array();
				if(!readArray(arrayValue, is)){
					cout << "Error in read array" << endl;
					return false;
				}
				array->Append(arrayValue, type);
				break;
			}
			
		}else{
			// ] (end of array)
			judgeDelimiter(true, is);
			break;
		}
	}
	return true;
}

int PDFParser::judgeDelimiter(bool skip){
	return judgeDelimiter(skip, &file);
}
int PDFParser::judgeDelimiter(bool skip, istream* is){
	// cout << "Delim" << endl;
	int pos=(int) is->tellg();
	// cout << pos << endl;
	int delimID=-1;
	if(!skipSpaces(is)){
		cout << "SS" << endl;
		return -1;
	}
	char a1, a2;
	is->get(a1);
	// cout << a1 << endl;
	if(a1=='('){
		delimID=Delimiter::LP;
	}else if(a1==')'){
		delimID=Delimiter::RP;
	}else if(a1=='<'){
		is->get(a2);
		if(a2=='<'){
			delimID=Delimiter::LLT;
		}else{
			is->seekg(-1, ios_base::cur);
			delimID=Delimiter::LT;
		}
	}else if(a1=='>'){
		is->get(a2);
		if(a2=='>'){
			delimID=Delimiter::GGT;
		}else{
			is->seekg(-1, ios_base::cur);
			delimID=Delimiter::GT;
		}
	}else if(a1=='['){
		delimID=Delimiter::LSB;
	}else if(a1==']'){
		delimID=Delimiter::RSB;
	}else if(a1=='{'){
		delimID=Delimiter::LCB;
	}else if(a1=='}'){
		delimID=Delimiter::RCB;
	}else if(a1=='/'){
		delimID=Delimiter::SOL;
	}else if(a1=='%'){
		delimID=Delimiter::PER;
	}	
	
	if(!skip){
		is->seekg(pos, ios_base::beg);
	}
	return delimID;
}

unsigned char* PDFParser::readName(){
	return readName(&file);
}
unsigned char* PDFParser::readName(istream* is){
	if(!skipSpaces(is)){
		return NULL;
	}
	char a;
	is->get(a);
	if(a!='/'){
		return NULL;
	}
	string value1=string("");
	while(true){
		is->get(a);
		if(isSpace(a) || isDelimiter(a)){
			is->seekg(-1, ios_base::cur);
			break;
		}else{
			value1+=a;
		}
	}
	// conversion: #xx -> an unsigned char
	unsigned char* ret=new unsigned char[value1.length()+1];
	int i;
	int j=0;
	for(i=0; i<value1.length(); i++){
		if(value1[i]!='#'){
			ret[j]=(unsigned char)value1[i];
			j++;
		}else{
			ret[j]=(unsigned char)stoi(value1.substr(i+1, 2), nullptr, 16);
			j++;
			i+=2;
		}
	}
	ret[i]='\0';
	return ret;
}

PDFStr* PDFParser::readString(){
	return readString(&file);
}
PDFStr* PDFParser::readString(istream* is){
	if(!skipSpaces(is)){
		return NULL;
	}
	char a;
	is->get(a);
	if(a!='<' && a!='('){
		return NULL;
	}
	char startDelim=a;
	bool isLiteral=(startDelim=='(');
	char endDelim= isLiteral ? ')' : '>';
	string value1=string("");
	int depth=1;
	while(true){
		is->get(a);
		if(a==endDelim){
			depth--;
			if(depth==0){
				break;
			}
		}else{
			if(a==startDelim){
				depth++;
			}else if(a=='\\'){
				value1+=a;
				is->get(a);
			}
			value1+=a;
		}
	}
	if(isLiteral){
		PDFStr* literal=new PDFStr(value1.length());
		int i=0;
		int j=0;
		for(i=0; i<value1.length(); i++){
			if(value1[i]=='\\'){
				// escape
				if(value1[i+1]=='n'){
					literal->decrData[j]='\n';
					j++;
					i++;
				}else if(value1[i+1]=='r'){
					literal->decrData[j]='\r';
					j++;
					i++;
				}else if(value1[i+1]=='t'){
					literal->decrData[j]='\t';
					j++;
					i++;
				}else if(value1[i+1]=='b'){
					literal->decrData[j]='\b';
					j++;
					i++;
				}else if(value1[i+1]=='f'){
					literal->decrData[j]='\f';
					j++;
					i++;
				}else if(value1[i+1]=='('){
					literal->decrData[j]='(';
					j++;
					i++;
				}else if(value1[i+1]==')'){
					literal->decrData[j]=')';
					j++;
					i++;
				}else if(value1[i+1]=='\\'){
					literal->decrData[j]='\\';
					j++;
					i++;
				}else if(value1[i+1]=='\r' || value1[i+1]=='\n'){
					if(value1[i+1]=='\r' && value1[i+2]=='\n'){
						i+=2;
					}else{
						i++;
					}
				}else if(isOctal(value1[i+1])){
					int length;
					if(isOctal(value1[i+2])){
						if(isOctal(value1[i+3])){
							length=3;
						}else{
							length=2;
						}
					}else{
						length=1;
					}
					char code[length+1];
					int l;
					for(l=0; l<length; l++){
						code[l]=value1[i+1+l];
					}
					code[length]='\0';
					literal->decrData[j]=(unsigned char)strtol(code, NULL, 8);
					i+=length;
					j++;
				}
			}else{
				literal->decrData[j]=(unsigned char)value1[i];
				j++;
			}
		}
		literal->decrData[j]='\0';
		literal->decrDataLen=j;
		return literal;
	}else{
		if(value1.length()%2==1){
			value1+='0';
		}
		PDFStr* hexadecimal=new PDFStr(value1.length()/2);
		int i;
		char hex[3];
		hex[2]='\0';
		for(i=0; i<value1.length()/2; i++){
			hex[0]=value1[i*2];
			hex[1]=value1[i*2+1];
			hexadecimal->decrData[i]=(unsigned char)strtol(hex, NULL, 16);
		}
		hexadecimal->decrData[value1.length()/2]=='\0';
		return hexadecimal;
	}
}

// outputError=false happend at ReadRefObj, where the object is Dictionary or Stream
bool PDFParser::readStream(Stream* stm, bool outputError){
	// read obj id
	if(!readObjID(&(stm->objNumber), &(stm->genNumber))){
		return false;
	}
	Log(LOG_DEBUG, "Read stream (objNumber %d, genNumber %d)", stm->objNumber, stm->genNumber);
	// read Stream dictionary
	if(!readDict(&(stm->StmDict))){
		return false;
	}
	// 'stream' row
	skipSpaces();
	int i;
	bool isNotStream=false;
	char a;
	for(i=0; i<strlen(STM); i++){
		file.get(a);
		if(a!=STM[i]){
			isNotStream=true;
			break;
		}
	}
	if(isNotStream){
		Log(outputError?LOG_ERROR:LOG_DEBUG, "Invalid character in the stream line");
		return false;
	}
	// here, EOL should be CRLF or LF, not including CR
	file.get(a);
	if(!isEOL(a)){
		Log(outputError?LOG_ERROR:LOG_DEBUG, "Invalid character in the stream line");
		return false;
	}
	if(a==CR){
		file.get(a);
		if(a!=LF){
			Log(outputError?LOG_ERROR:LOG_DEBUG, "Invalid EOL in the stream line");
			return false;
		}
	}
	int* length;
	if(stm->StmDict.Read("Length", (void**)&length, Type::Int)){
		Log(LOG_DEBUG, "Stream length is %d", *length);
		stm->encoData=new unsigned char[*length];
		stm->encoDataLen=*length;
		unsigned char a2;
		for(i=0; i<*length; i++){
			a2=(unsigned char)file.get();
			stm->encoData[i]=a2;
		}
		gotoEOL();
		// the last row should be 'endstream'
		bool isNotEndstream=false;
		for(i=0; i<strlen(ESTM); i++){
			file.get(a);
			if(a!=ESTM[i]){
				isNotEndstream=true;
				break;
			}
		}
		if(isNotEndstream){
			Log(LOG_WARN, "The endstream line not found");
		}
	}else{
		return false;
	}			
	return true;
}

int PDFParser::judgeType(){
	return judgeType(&file);
}
int PDFParser::judgeType(istream* is){
	// cout << "Type" << endl;
	if(!skipSpaces(is)){
		return -1;
	}
	// cout << "Skip" << endl;
	int delimID=judgeDelimiter(false, is);
	// cout << delimID << endl;
	// begins with delimiter: string, name, dict, array
	if(delimID==Delimiter::LP || delimID==Delimiter::LT){
		return Type::String;
	}else if(delimID==Delimiter::SOL){
		return Type::Name;
	}else if(delimID==Delimiter::LLT){
		return Type::Dict;
	}else if(delimID==Delimiter::LSB){
		return Type::Array;
	}
	// token
	int position=(int) is->tellg();
	int typeID=-1;
	char buffer[128];
	if(!readToken(buffer, 128, is)){
	}
	if(strcmp(buffer, "true")==0 || strcmp(buffer, "false")==0){
		typeID=Type::Bool;
	}
	if(strcmp(buffer, "null")==0){
		typeID=Type::Null;
	}

	//
	int v1, v2;
	double v3;
	is->seekg(position, ios_base::beg);
	if(readRefID(&v1, &v2, is)){
		typeID=Type::Indirect;
	}else{
		is->seekg(position, ios_base::beg);
		if(readInt(&v1, is)){
			typeID=Type::Int;
		}else{
			is->seekg(position, ios_base::beg);
			if(readReal(&v3, is)){
				typeID=Type::Real;
			}
		}
	}
		

	is->seekg(position, ios_base::beg);
	return typeID;
}
