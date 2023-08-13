// classes of objects

#include <iostream>
#include <cstring>
#include <string>
#include <cmath>
#include "Objects.hpp"
#include "zlib.h" // /usr/include/zlib.h

#ifndef LOG_INCLUDED
#include "log.hpp"
#endif

#include "variables_ext.hpp"

#define ASCIIHEX  "ASCIIHexDecode"
#define ASCII85   "ASCII85Decode"
#define LZW       "LZWDecode"
#define FLATE     "FlateDecode"
#define RUNLENGTH "RunLengthDecode"
#define CCITTFAX  "CCITTFaxDecode"
#define JBIG2     "JBIG2Decode"
#define DCT       "DCTDecode"
#define JPXDecode "JPXDecode"
#define CRYPT     "Crypt"

#define ZBUFSIZE 1024
#define DICTSIZE 128
#define ARRAYSIZE 128

using namespace std;

Dictionary::Dictionary(){
	keys.reserve(DICTSIZE);
	values.reserve(DICTSIZE);
	types.reserve(DICTSIZE);
}

Dictionary::Dictionary(Dictionary* original){
	copy(original->keys.begin(), original->keys.end(), back_inserter(keys));
	copy(original->values.begin(), original->values.end(), back_inserter(values));
	copy(original->types.begin(), original->types.end(), back_inserter(types));
}

int Dictionary::Search(unsigned char* key){
	// return the index associated with key, -1 otherwise
	int len=unsignedstrlen(key);
	int i;
	for(i=0; i<keys.size(); i++){
		int len2=unsignedstrlen(keys[i]);
		if(len==len2){
			int j;
			bool flag=true;
			for(j=0; j<len; j++){
				if(key[j]!=keys[i][j]){
					flag=false;
					break;
				}
			}
			if(flag){
				return i;
			}
		}
	}
	return -1;
}
int Dictionary::Search(const char* key){
	return Search((unsigned char*)key);
}

void Dictionary::Merge(Dictionary dict2){
	int i;
	for(i=0; i<dict2.keys.size(); i++){
		Append(dict2.keys[i], dict2.values[i], dict2.types[i]);
	}
}

bool Dictionary::Read(unsigned char* key, void** value, int* type){
	int index=Search(key);
	if(index<0){
		return false;
	}
	*value=values[index];
	*type=types[index];
	return true;
}
bool Dictionary::Read(const char* key, void** value, int* type){
	return Read((unsigned char*)key, value, type);
}

bool Dictionary::Read(unsigned char* key, void** value, int type){
	int readType;
	if(Read(key, value, &readType)){
		if(readType==type){
			return true;
		}else{
			Log(LOG_ERROR, "Type mismatch in Dictionary::Read");
		}
	}
	return false;
}
bool Dictionary::Read(const char* key, void** value, int type){
	return Read((unsigned char*)key, value, type);
}
bool Dictionary::Read(int index, unsigned char** key, void** value, int* type){
	*key=keys[index];
	*value=values[index];
	*type=types[index];
	return true;
}

void Dictionary::Append(const char* key, void* value, int type){
	Append((unsigned char*)key, value, type);
}

void Dictionary::Append(unsigned char* key, void* value, int type){
	// check whether the same key already exists
	if(Search(key)>=0){
		Log(LOG_WARN, "Key %s already exists, skip it", key);
		return;
	}	
	keys.push_back(key);
	values.push_back(value);
	types.push_back(type);
}
bool Dictionary::Update(const char* key, void* value, int type){
	return Update((unsigned char*)key, value, type);
}

bool Dictionary::Update(unsigned char* key, void* value, int type){
	int index=Search(key);
	if(index<0){
		printf("Key %s does not exist\n", key);
		return false;
	}
	values[index]=value;
	types[index]=type;
	return true;
}

void Dictionary::Print(){
	Print(0);
}

void Dictionary::Print(int indent){
	int i;
	char indentStr[indent*2+1];
	for(i=0; i<indent; i++){
		indentStr[2*i]=' ';
		indentStr[2*i+1]=' ';
	}
	indentStr[indent*2]='\0';
	for(i=0; i<keys.size(); i++){
		if(types[i]==Type::Dict || types[i]==Type::Array){
			printf("%sName: %s, TypeID: %d\n", indentStr, keys[i], types[i]);
			if(types[i]==Type::Dict){
				Dictionary* dictValue=(Dictionary*)values[i];
				dictValue->Print(indent+1);
			}else{
				Array* arrayValue=(Array*)values[i];
				arrayValue->Print(indent+1);
			}
		}else{
			char* printValue=printObj(values[i], types[i]);
			printf("%sName: %s, TypeID: %d, Value: %s\n", indentStr, keys[i], types[i], printValue);
			delete printValue;
		}
	}
}

void Dictionary::Delete(int index){
	keys.erase(keys.begin()+index);
	values.erase(values.begin()+index);
	types.erase(types.begin()+index);
}

Array::Array(){
	values.reserve(ARRAYSIZE);
	types.reserve(ARRAYSIZE);
}

void Array::Append(void* value, int type){
	values.push_back(value);
	types.push_back(type);
}

void Array::Print(){
	Print(0);
}
void Array::Print(int indent){
	int i;
	char indentStr[indent*2+1];
	for(i=0; i<indent; i++){
		indentStr[2*i]=' ';
		indentStr[2*i+1]=' ';
	}
	indentStr[indent*2]='\0';
	for(i=0; i<values.size(); i++){
		if(types[i]==Type::Dict || types[i]==Type::Array){
			printf("%sElement #%d, TypeID: %d\n", indentStr, i, types[i]);
			if(types[i]==Type::Dict){
				Dictionary* dictValue=(Dictionary*)values[i];
				dictValue->Print(indent+1);
			}else{
				Array* arrayValue=(Array*)values[i];
				arrayValue->Print(indent+1);
			}
		}else{
			char* printValue=printObj(values[i], types[i]);
			printf("%sElement #%d, TypeID: %d, Value: %s\n", indentStr, i, types[i], printValue);
			delete printValue;
		}
	}
}

int Dictionary::GetSize(){
	return values.size();
}

int Array::GetSize(){
	return values.size();
}

bool Array::Read(int index, void** value, int* type){
	if(index>=0 && index<values.size()){
		*value=values[index];
		*type=types[index];
		return true;
	}
	return false;
}
bool Array::Read(int index, void** value, int type){
	int readType;
	if(Read(index, value, &readType)){
		if(type==readType){
			return true;
		}else{
			Log(LOG_ERROR, "Type mismatch in Array::Read");
			return false;
		}
	}else{
		return false;
	}
}

Indirect::Indirect():
	objNumber(-1),
	objStream(false),
	notEncrypted(false)
{
}

Stream::Stream():
	decrypted(false)
{	
}


char* printObj(void* value, int type){
	char* buffer=new char[256];
	strcpy(buffer, "");
	PDFStr* stringValue;
	unsigned char* nameValue;
	Indirect* indirectValue;
	int len;
	switch(type){
	case Type::Bool:
		if(*((bool*)value)){
		  strcpy(buffer, "true");
		}else{
			strcpy(buffer, "false");
		}
		break;
	case Type::Int:
		sprintf(buffer, "%d", *((int*)value));
		break;
	case Type::Real:
		sprintf(buffer, "%10.3f", *((double*)value));
		break;
	case Type::String:
	  stringValue=(PDFStr*)value;
		len=stringValue->decrDataLen;
		sprintf(buffer, "%-32s ... [L=%d]", stringValue->decrData, len);
		break;
	case Type::Name:
	  nameValue=(unsigned char*)value;
		len=unsignedstrlen(nameValue);
		sprintf(buffer, "%-16s ... [L=%d]", nameValue, len);
		break;
	case Type::Null:
		strcpy(buffer, "null");
		break;
	case Type::Indirect:
		indirectValue=(Indirect*)value;
		sprintf(buffer, "objNumber=%d, genNumber=%d", indirectValue->objNumber, indirectValue->genNumber);
		break;
	}

	return buffer;
}


int unsignedstrlen(unsigned char* a){
	int len=0;
	while(true){
		if(a[len]!='\0'){
			len++;
		}else{
			return len;
		}
	}
}

void unsignedstrcpy(unsigned char* dest, const char* data){
	unsignedstrcpy(dest, (unsigned char*)data);
}
void unsignedstrcpy(unsigned char* dest, unsigned char* data){
	int len=unsignedstrlen(data);
	int i;
	for(i=0; i<len; i++){
		dest[i]=data[i];
	}
	dest[len]='\0';
}
		

bool Stream::Decode(){
	Log(LOG_DEBUG, "Decode stream");
	int filtersType;
	void* filtersValue;
	int i;
	
	if(!StmDict.Read("Filter", (void**)&filtersValue, &filtersType)){
		// no filter
		Log(LOG_DEBUG, "Stream with no Filter");
		decoData=new unsigned char[encoDataLen];
		for(i=0; i<encoDataLen; i++){
			decoData[i]=encoData[i];
		}
		decoDataLen=encoDataLen;
		return true;
	}

	unsigned char* encoBuffer=new unsigned char[encoDataLen];
	unsigned char* decoBuffer;
	for(i=0; i<encoDataLen; i++){
		encoBuffer[i]=encoData[i];
	}
	int encoBufLen=encoDataLen;
	int decoBufLen;
	
	// get decode paramters
	int parmsType;
	void* parmsValue;
	bool parmsExist=StmDict.Read("DecodeParms", (void**)&parmsValue, &parmsType);

	Dictionary* parmDict;
	void* parmValue;
	int parmType;
	
	unsigned char* filterName;
	void* filterValue;
	int filterType;
	
	if(filtersType==Type::Array){
		Array* filterArray=(Array*)filtersValue;
		Array* parmsArray=(Array*)parmsValue;
		int filterSize=filterArray->GetSize();
		for(i=0; i<filterSize; i++){
			// prepare filter
			Log(LOG_DEBUG, "Filter #%d", i);
			if(!filterArray->Read(i, (void**)&filterName, Type::Name)){
				Log(LOG_ERROR, "Failed in loading the filter");
				return false;
			}

			// prepare parm
			if(parmsExist){
				if(!parmsArray->Read(i, (void**)&parmValue, &parmType)){
					Log(LOG_ERROR, "Failed in loading DecodeParm");
					return false;
				}
			}else{
				parmType=Type::Null;
			}
			
			if(parmType==Type::Dict){
				parmDict=(Dictionary*)parmValue;
				decoBufLen=decodeData(encoBuffer, filterName, parmDict, encoBufLen, &decoBuffer);
			}else if(parmType==Type::Null){
				decoBufLen=decodeData(encoBuffer, filterName, NULL, encoBufLen, &decoBuffer);
			}else{
				Log(LOG_ERROR, "Invalid DecodeParm type: %d", parmType);
				return false;
			}
			if(decoBufLen==0){
				return false;
			}
			// copy
			encoBuffer=decoBuffer;
			encoBufLen=decoBufLen;
		}
	}else if(filtersType==Type::Name){
		filterName=(unsigned char*)filtersValue;
		if(parmsExist){
			if(parmsType==Type::Dict){
				parmDict=(Dictionary*)parmsValue;
				decoBufLen=decodeData(encoBuffer, filterName, parmDict, encoBufLen, &decoBuffer);
			}else if(parmsType==Type::Null){
				decoBufLen=decodeData(encoBuffer, filterName, NULL, encoBufLen, &decoBuffer);
			}else{
				Log(LOG_ERROR, "Invalid DecodeParm type");
				return false;
			}
		}else{
			decoBufLen=decodeData(encoBuffer, filterName, NULL, encoBufLen, &decoBuffer);
		}
		if(decoBufLen==0){
			return false;
		}
	}else{
		Log(LOG_ERROR, "Invalid Filter type");
		return false;
	}
	decoDataLen=decoBufLen;
	decoData=decoBuffer;

	return true;
}

bool Stream::Encode(){
	Log(LOG_DEBUG, "Encode stream");
	int filtersType;
	void* filtersValue;
	int i;
	
	if(!StmDict.Read("Filter", (void**)&filtersValue, &filtersType)){
		// no filter
		Log(LOG_DEBUG, "Stream with no Filter");
		encoData=new unsigned char[decoDataLen];
		for(i=0; i<decoDataLen; i++){
			encoData[i]=decoData[i];
		}
		encoDataLen=decoDataLen;
		return true;
	}

	unsigned char* decoBuffer=new unsigned char[decoDataLen];
	unsigned char* encoBuffer;
	for(i=0; i<decoDataLen; i++){
		decoBuffer[i]=decoData[i];
	}
	int decoBufLen=decoDataLen;
	int encoBufLen;
	
	// get decode paramters
	int parmsType;
	void* parmsValue;
	bool parmsExist=StmDict.Read("DecodeParms", (void**)&parmsValue, &parmsType);

	Dictionary* parmDict;
	void* parmValue;
	int parmType;
	
	unsigned char* filterName;
	void* filterValue;
	int filterType;
	
	if(filtersType==Type::Array){
		Array* filterArray=(Array*)filtersValue;
		Array* parmsArray=(Array*)parmsValue;
		int filterSize=filterArray->GetSize();
		for(i=filterSize-1; i>=0; i--){
			// prepare filter
			Log(LOG_DEBUG, "Filter #%d", i);
			if(!filterArray->Read(i, (void**)&filterName, Type::Name)){
				Log(LOG_ERROR, "Failed in loading the filter");
				return false;
			}

			// prepare parm
			if(parmsExist){
				if(!parmsArray->Read(i, (void**)&parmValue, &parmType)){
					Log(LOG_ERROR, "Failed in loading DecodeParm");
					return false;
				}
			}else{
				parmType==Type::Null;
			}
			
			if(parmType==Type::Dict){
				parmDict=(Dictionary*)parmValue;
				encoBufLen=encodeData(decoBuffer, filterName, parmDict, decoBufLen, &encoBuffer);
			}else if(parmType==Type::Null){
				encoBufLen=encodeData(decoBuffer, filterName, NULL, decoBufLen, &encoBuffer);
			}else{
				Log(LOG_ERROR, "Invalid DecodeParm type");
				return false;
			}
			if(encoBufLen==0){
				return false;
			}
			// copy
			decoBuffer=encoBuffer;
			decoBufLen=encoBufLen;
		}
	}else if(filtersType==Type::Name){
		filterName=(unsigned char*)filtersValue;
		if(parmsExist){
			if(parmsType==Type::Dict){
				parmDict=(Dictionary*)parmsValue;
				encoBufLen=encodeData(decoBuffer, filterName, parmDict, decoBufLen, &encoBuffer);
			}else if(parmsType==Type::Null){
				encoBufLen=encodeData(decoBuffer, filterName, NULL, decoBufLen, &encoBuffer);
			}else{
				Log(LOG_ERROR, "Invalid DecodeParm type");
				return false;
			}
		}else{
			encoBufLen=encodeData(decoBuffer, filterName, NULL, decoBufLen, &encoBuffer);
		}
		if(decoBufLen==0){
			return false;
		}
	}else{
		Log(LOG_ERROR, "Invalid Filter type");
		return false;
	}
	encoDataLen=encoBufLen;
	encoData=encoBuffer;
	return true;
}

bool unsignedstrcmp(unsigned char* a, unsigned char* b){
	int alength=unsignedstrlen(a);
	int blength=unsignedstrlen(b);
	if(alength!=blength){
		return false;
	}
	int i;
	for(i=0; i<alength; i++){
		if(a[i]!=b[i]){
			return false;
		}
	}
	return true;
}
bool unsignedstrcmp(unsigned char* a, const char* b){
	return unsignedstrcmp(a, (unsigned char*)b);
}

int decodeData(unsigned char* encoded, unsigned char* filter, Dictionary* parm, int encodedLength, unsigned char** decoded){
	Log(LOG_DEBUG, "decodeData: filter %s, encoded size: %d", filter, encodedLength);

	unsigned char inBuf[ZBUFSIZE];
	unsigned char outBuf[ZBUFSIZE];
	z_stream z;
	z.zalloc=Z_NULL;
	z.zfree=Z_NULL;
	z.opaque=Z_NULL;

	string output("");
	
	int remainingData=encodedLength;
	int decodedData=0;
	int i;
	int zStatus;
	int decodedLength;

	if(unsignedstrcmp(filter, (unsigned char*)FLATE)){
		Log(LOG_DEBUG, "Flate decode");
		if(parm!=NULL && LOG_LEVEL>=LOG_DEBUG){
			Log(LOG_DEBUG, "Flate decode parameters:");
			parm->Print();
		}else{
			// cout << "No parameters given" << endl;
		}

		// zlib preparation
	  z.next_in=Z_NULL;
		z.avail_in=0;
		if(inflateInit(&z)!=Z_OK){
			Log(LOG_ERROR, "Failed in inflateInit: %s\n", z.msg);
			return 0;
		}

		// output buffer
		z.next_out=&outBuf[0];
		z.avail_out=ZBUFSIZE;

		while(true){
			// copy the data to inBuf
			if(z.avail_in==0){
				int copyLength=min(remainingData, ZBUFSIZE);
				for(i=0; i<copyLength; i++){
					inBuf[i]=encoded[decodedData+i];
				}
				z.next_in=&inBuf[0];
				z.avail_in=copyLength;
			
				decodedData+=copyLength;
				remainingData-=copyLength;
				//printf("Decompress %d bytes\n", copyLength);
			}
			
			// decompress
			zStatus=inflate(&z, Z_NO_FLUSH);
			//printf("Remaining output space: %d bytes\n", z.avail_out);
			//printf("Remaining input buffer: %d bytes\n", z.avail_in);
			if(zStatus==Z_STREAM_END){
				break;
			}else if(zStatus==Z_OK){
				// cout << "inflate ok" << endl;
				if(remainingData==0 && z.avail_in==0){
					break;
				}
			}else{
				Log(LOG_ERROR, "Failed in inflate: %s", z.msg);
				return 0;
			}
			if(z.avail_out==0){
				// output buffer is full
				for(i=0; i<ZBUFSIZE; i++){
					output+=outBuf[i];
				}
				z.avail_out=ZBUFSIZE;
				z.next_out=&outBuf[0];
			}
		}
		// finish decompression
		if(inflateEnd(&z)!=Z_OK){
			Log(LOG_ERROR, "Failed in inflateEnd: %s", z.msg);
			return 0;
		}
		for(i=0; i<(ZBUFSIZE-z.avail_out); i++){
			output+=outBuf[i];
		}
		// cout << output.length() << " LENGTH" << endl;
		*decoded=new unsigned char[output.length()];
		for(i=0; i<output.length(); i++){
			(*decoded)[i]=(unsigned char)output[i];
		}
		decodedLength=output.length();

		// predictor
		if(parm!=NULL && parm->Search("Predictor")>=0){
			int* predictor;
			if(parm->Read("Predictor", (void**)&predictor, Type::Int)){
				if(*predictor>=10){
					// PNG predictor
					Log(LOG_DEBUG, "PNG predictor");
					decodedLength=PNGPredictor(decoded, decodedLength, parm);
				}
			}	
		}
		return decodedLength;
	}
	return 0;
}

int encodeData(unsigned char* decoded, unsigned char* filter, Dictionary* parm, int decodedLength, unsigned char** encoded){
	Log(LOG_DEBUG, "Filter: %s, decoded length: %d", filter, decodedLength);

	unsigned char inBuf[ZBUFSIZE];
	unsigned char outBuf[ZBUFSIZE];
	z_stream z;
	z.zalloc=Z_NULL;
	z.zfree=Z_NULL;
	z.opaque=Z_NULL;

	string output("");
	
	int remainingData=decodedLength;
	int encodedData=0;
	int i;
	int zStatus;
	int encodedLength;

	/* for debug 
	for(i=0; i<remainingData; i++){
		printf("%02x ", (unsigned int)decoded[i]);
	}
	cout << endl;*/
	

	if(unsignedstrcmp(filter, FLATE)){
		// cout << "Flate" << endl;
		if(parm!=NULL){
			parm->Print();
		}else{
			// cout << "No parameters given" << endl;
		}

		// zlib preparation
	  z.next_in=Z_NULL;
		z.avail_in=0;
		if(deflateInit(&z, Z_DEFAULT_COMPRESSION)!=Z_OK){
			Log(LOG_ERROR, "deflateInit error: %s", z.msg);
			return 0;
		}

		// output buffer
		z.next_out=&outBuf[0];
		z.avail_out=ZBUFSIZE;

		while(true){
			// copy the data to inBuf
			if(z.avail_in==0){
				int copyLength=min(remainingData, ZBUFSIZE);
				for(i=0; i<copyLength; i++){
					inBuf[i]=decoded[encodedData+i];
				}
				z.next_in=&inBuf[0];
				z.avail_in=copyLength;
			
				encodedData+=copyLength;
				remainingData-=copyLength;
				Log(LOG_DEBUG, "Compress %d bytes", copyLength);
			}
			int keyword=(z.avail_in==0?Z_FINISH:Z_NO_FLUSH);
			
			// compress
			zStatus=deflate(&z, keyword);
			Log(LOG_DEBUG, "Remaining output space: %d bytes", z.avail_out);
			Log(LOG_DEBUG, "Remaining input buffer: %d bytes", z.avail_in);
			if(zStatus==Z_STREAM_END){
				Log(LOG_DEBUG, "End");
				break;
			}else if(zStatus==Z_OK){
				Log(LOG_DEBUG, "Deflate ok");
				if(keyword==Z_FINISH && zStatus==Z_STREAM_END){
					Log(LOG_DEBUG, "Finish");
					break;
				}
			}else{
				Log(LOG_ERROR, "Deflate error: %s, %d", z.msg, zStatus);
				return 0;
			}
			if(z.avail_out==0){
				Log(LOG_DEBUG, "Buffer full");
				// output buffer is full
				for(i=0; i<ZBUFSIZE; i++){
					output+=outBuf[i];
				}
				z.avail_out=ZBUFSIZE;
				z.next_out=&outBuf[0];
			}
		}
		// finish compression
		if(deflateEnd(&z)!=Z_OK){
			Log(LOG_ERROR, "DeflateEnd error: %s", z.msg);
			return 0;
		}
		Log(LOG_DEBUG, "Remaining output space: %d bytes", z.avail_out);
		Log(LOG_DEBUG, "Remaining input buffer: %d bytes", z.avail_in);
		for(i=0; i<(ZBUFSIZE-z.avail_out); i++){
			output+=outBuf[i];
		}
		// cout << output.length() << " LENGTH" << endl;
		*encoded=new unsigned char[output.length()];
		for(i=0; i<output.length(); i++){
			(*encoded)[i]=(unsigned char)output[i];
		}
		encodedLength=output.length();
		return encodedLength;
	}
	
	return 0;
}

char PaethPredictor(char a, char b, char c){
	// a=left
	// b=top
	// c=left-top
	char p=a+b-c;
	char pa=abs(p-a);
	char pb=abs(p-b);
	char pc=abs(p-c);
	if(pa<=pb && pa<=pc){
		return a;
	}else if(pb<=pc){
		return b;
	}else{
		return c;
	}
}
		

int PNGPredictor(unsigned char** pointer, int length, Dictionary* parm){
	// read parameters
	int columns=1;
	int* columnsValue;
	if(parm->Read("Columns", (void**)&columnsValue, Type::Int)){
		columns=*columnsValue;
	}
	int colors=1;
	int* colorsValue;
	if(parm->Read("Colors", (void**)&colorsValue, Type::Int)){
		colors=*colorsValue;
	}
	int bits=8;
	int* bitsValue;
	if(parm->Read("BitsPerComponent", (void**)&bitsValue, Type::Int)){
		bits=*bitsValue;
	}
	int bytesPerColumn=columns*colors*bits/8;
	int bpp=colors*bits/8;
	if((colors*bits)%8!=0){
		bpp++;
	}
		
	if(length%(bytesPerColumn+1)!=0){
		Log(LOG_ERROR, "Failed in PNGPredictor: columns and length mismatch, length=%d, columns=%d", length, columns);
		return 0;
	}else{
		Log(LOG_DEBUG, "length=%d, column bytes=%d", length, bytesPerColumn);
		Log(LOG_DEBUG, "bpp=%d", bpp);
	}
	int numRows=length/(bytesPerColumn+1);
	unsigned char* restored=new unsigned char[numRows*bytesPerColumn];
	int i, j;
	char ave_left, ave_top, ave;
	char pa_left, pa_top, pa_lt, pa;
	for(i=0; i<numRows; i++){
		int algorithm=(int)(*pointer)[i*(bytesPerColumn+1)];
		switch(algorithm){
		case 0:
			// none
			for(j=0; j<bytesPerColumn;j++){
				restored[i*bytesPerColumn+j]=(*pointer)[i*(bytesPerColumn+1)+j+1];
			}
			break;
		case 1:
			// Sub
			for(j=0; j<bytesPerColumn; j++){
				if(j<bpp){
					restored[i*bytesPerColumn+j]=(*pointer)[i*(bytesPerColumn+1)+j+1];
				}else{
					restored[i*bytesPerColumn+j]=(*pointer)[i*(bytesPerColumn+1)+j+1]+restored[i*bytesPerColumn+j-bpp];
				}
			}
			break;
		case 2:
			// Up
			if(i==0){
				for(j=0; j<bytesPerColumn; j++){
					restored[i*bytesPerColumn+j]=(*pointer)[i*(bytesPerColumn+1)+j+1];
				}
			}else{
				for(j=0; j<bytesPerColumn; j++){
					restored[i*bytesPerColumn+j]=(*pointer)[i*(bytesPerColumn+1)+j+1]+restored[(i-1)*bytesPerColumn+j];
				}
			}
			break;
		case 3:
			// Average
			if(i==0){
				ave_top=0;
			}else{
				ave_top=restored[(i-1)*bytesPerColumn+j];
			}
			if(j<bpp){
				ave_left=0;
			}else{
				ave_left=restored[i*bytesPerColumn+j-bpp];
			}
			ave=(*pointer)[i*(bytesPerColumn+1)+j+1];
			restored[i*bytesPerColumn+j]=ave+floor((ave_left+ave_top)/2);
			break;
		case 4:
			//Paeth
			if(i==0){
				pa_top=0;
			}else{
				pa_top=restored[(i-1)*bytesPerColumn+j];
			}
			if(j<bpp){
				pa_left=0;
			}else{
				pa_left=restored[i*bytesPerColumn+j-bpp];
			}
			if(i==0 || j<bpp){
				pa_lt=0;
			}else{
				pa_lt=restored[(i-1)*bytesPerColumn+j-bpp];
			}
			pa=(*pointer)[i*(bytesPerColumn+1)+j+1];
			restored[i*bytesPerColumn+j]=pa+PaethPredictor(pa_left, pa_top, pa_lt);
			break;
		default:
			Log(LOG_ERROR, "Unsupported predictor algorithm %d", algorithm);
			return 0;
		}
	}
	*pointer=&restored[0];
	return numRows*bytesPerColumn;
}

Page::Page(){
}

PDFStr::PDFStr():
	decrypted(false),
	isHexStr(false)
{
}

PDFStr::PDFStr(int dDL):
	decrypted(false),
	isHexStr(false)
{
	decrDataLen=dDL;
	decrData=new unsigned char[decrDataLen+1];
}

// for text string
PDFStr* PDFStr::ConvertToUTF8(){
	// Log(LOG_DEBUG, "ConvertToUTF8");
	PDFStr* ret;
	int i;
	// head bytes check
	if(decrDataLen>=2 && decrData[0]==0xfe && decrData[1]==0xff){
		//Log(LOG_DEBUG, "UTF-16BE");
		i=2;
		ret=new PDFStr((decrDataLen-2)*2);
		int count=0;
		while(i<decrDataLen){
			unsigned int char1=(int(decrData[i])<<8)+decrData[i+1];
			unsigned int char2=0;
			i+=2;
			if(char1>>10==0b110110){
				char2=(int(decrData[i])<<8)+decrData[i+1];
				char1=char1 & 0b1111111111;
				char2=char2 & 0b1111111111;
				char1=(char1<<10)+char2+0x10000;
				i+=2;
			}
			//Log(LOG_DEBUG, "%021b", char1);
			if(char1<0x0080){
				// 1 byte
				ret->decrData[count]=(unsigned char)char1;
				count++;
			}else if(char1<0x0800){
				// 2 bytes
				ret->decrData[count]=  (unsigned char)(0b11000000+(char1>>6));
				ret->decrData[count+1]=(unsigned char)(0b10000000+(char1 & 0b00111111));
				count+=2;
			}else if(char1<0xffff){
				// 3 bytes
				ret->decrData[count]=  (unsigned char)(0b11100000+(char1>>12));
				ret->decrData[count+1]=(unsigned char)(0b10000000+(char1>>6 & 0b00111111));
				ret->decrData[count+2]=(unsigned char)(0b10000000+(char1    & 0b00111111));
				count+=3;
			}else{
				// 4 bytes
				ret->decrData[count]=  (unsigned char)(0b11110000+(char1>>18));
				ret->decrData[count+1]=(unsigned char)(0b10000000+(char1>>12 & 0b00111111));
				ret->decrData[count+2]=(unsigned char)(0b10000000+(char1>>6  & 0b00111111));
				ret->decrData[count+3]=(unsigned char)(0b10000000+(char1     & 0b00111111));
				count+=4;
			}
			ret->decrData[count]='\0';
			ret->decrDataLen=count;
		}
	}else if(decrDataLen>=3 && decrData[0]==0xef && decrData[1]==0xbb && decrData[2]==0xbf){
		//Log(LOG_DEBUG, "UTF-8");
		ret=new PDFStr(decrDataLen-3);
		for(int i=0; i<decrDataLen-3; i++){
			ret->decrData[i]=decrData[i+3];
		}
	}else{
		ret=new PDFStr(decrDataLen);
		for(int i=0; i<decrDataLen; i++){
			ret->decrData[i]=decrData[i];
		}
		ret->decrData[decrDataLen]='\0';
	}
	return ret;
}

// need "" if , or \r or \n or " is included
// " is replaced by ""
PDFStr* PDFStr::CSVEscape(){
	bool dquoteRequired=false;
	int numDquotes=0;
	for(int i=0; i<decrDataLen; i++){
		if(decrData[i]=='\r' || decrData[i]=='\n' || decrData[i]==',' || decrData[i]=='"'){
			dquoteRequired=true;
		}
		if(decrData[i]=='"'){
			numDquotes++;
		}
	}
	PDFStr* ret=new PDFStr(decrDataLen+(dquoteRequired?2:0)+numDquotes);
	int index=0;
	if(dquoteRequired){
		ret->decrData[index]='"';
		index++;
	}
	for(int i=0; i<decrDataLen; i++){
		if(decrData[i]=='"'){
			ret->decrData[index]='"';
			ret->decrData[index+1]='"';
			index+=2;
		}else{
			ret->decrData[index]=decrData[i];
			index++;
		}
	}

	if(dquoteRequired){
		ret->decrData[index]='"';
		index++;
	}
	ret->decrData[index]='\0';
	return ret;
}

int byteSize(int n){
	int byte=1;
	while(n>=256){
		byte++;
		n/=256;
	}
	return byte;
}

PDFStr* dateString(){
	PDFStr* date=new PDFStr(23);

	time_t t=time(NULL);
	tm* now=localtime(&t);
	int Y=now->tm_year+1900;
	int M=now->tm_mon+1;
	int D=now->tm_mday;
	int H=now->tm_hour;
	int m=now->tm_min;
	int S=now->tm_sec;

	char* tz=new char[8];
	strcpy(tz, "+09'00'");
	
	sprintf((char*)date->decrData, "D:%04d%02d%02d%02d%02d%02d%7s",Y,M,D,H,m,S,tz);
	return date;
}


PDFVersion::PDFVersion():
	error(false),
	valid(false)
{
	
}

bool PDFVersion::Set(char* label){
	// label: "x.y"
	// (x, y) = (1, 0) ~ (1, 7), (2, 0)
	if(label[0]=='1'){
		major=1;
	}else if(label[0]=='2'){
		major=2;
	}else{
		cout << "Error 1 in version label" << endl;
		error=true;
		return false;
	}
	if(label[1]!='.'){
		cout << "Error 2 in version label" << endl;
		error=true;
		return false;
	}
	char minor_c[2];
	minor_c[0]=label[2];
	minor_c[1]='\0';
	minor=atoi(minor_c);
	if(major==1){
		if((label[2]=='0' || minor>0) && minor<=7){
			// ok
		}else{
			cout << "Error 3 in version label" << endl;
			error=true;
			return false;
		}
	}else if(major==2){
		if(label[2]=='0'){
			// ok
		}else{
			cout << "Error 4 in version label" << endl;
			error=true;
			return false;
		}
	}
	print();
	valid=true;
	return true;
}

void PDFVersion::print(){
	if(error){
		return;
	}
	sprintf(v, "%1d.%1d", major, minor);
	// cout << int(v[3]) << endl;
	// cout << strlen(v) << endl;
}

bool PDFVersion::IsValid(){
	return (!error) && valid;
}

void DumpPDFStr(PDFStr* str){
	int i;
	for(i=0; i<str->decrDataLen; i++){
		printf("%02x ", str->decrData[i]);
	}
	cout << endl;
}

// return true if a>=b, false if a<b
bool CompareVersions(PDFVersion a, PDFVersion b){
	if(a.major>b.major){
		return true;
	}else if(a.major<b.major){
		return false;
	}
	if(a.minor>=b.minor){
		return true;
	}else{
		return false;
	}
}
