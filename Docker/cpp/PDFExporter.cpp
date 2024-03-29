// class PDFExporter
#include <fstream>
#include <iostream>
#include <cstring>
#include <vector>

#include "PDFExporter.hpp"
#include "log.hpp"

using namespace std;

PDFExporter::PDFExporter(PDFParser* parser):
	PP(parser),
	count(0),
	literalStringBorder(0.3)
{
	// PDF Ver>=1.5 -> Cross-reference streams
	// PDF Ver<1.5 -> Cross-reference tables
	if(PP->v_document.IsValid() &&
		 ((PP->v_document.major==1 && PP->v_document.minor>=5) ||
			PP->v_document.major>=2)){
		XRefStmUsed=true;
	}else{
		XRefStmUsed=false;
	} 
}

bool PDFExporter::exportToFile(char* fileName){
	return exportToFile(fileName, false);
}

bool PDFExporter::exportToFile(char* fileName, bool encryption){
	if(encryption){
		Log(LOG_INFO, "Check Document catalog dictionary");
		// move Document catalog dictionary out of the object stream
		// This procedure is necessary for Adobe Reader !!
		Indirect *dCatalogRef;
		Dictionary *dCatalog;
		if (PP->trailer.Read("Root", (void **)&dCatalogRef, Type::Indirect) &&
			PP->Read(&(PP->trailer), "Root", (void**)&dCatalog, Type::Dict)){
			int dCatalogNumber = dCatalogRef->objNumber;
			Indirect* dCatalogRefInRef=PP->Reference[dCatalogNumber];
			int dCatalogIndex=dCatalogRefInRef->objStreamIndex;
			int dCatalogStmNumber=dCatalogRefInRef->objStreamNumber;
			Log(LOG_DEBUG, "Number: %d, Index: %d, StmNumber: %d", dCatalogNumber, dCatalogIndex, dCatalogStmNumber);
			Indirect *dCatalogRef2 = PP->Reference[dCatalogNumber];
			if (dCatalogRef2->objStream){
				dCatalogRef2->objStream = false;
				Stream* Parent;
				if(PP->ReadRefObj(PP->Reference[dCatalogStmNumber], (void**)&Parent, Type::Stream)){
					// shift objStreamIndex
					int N=Parent->numObjects;
					int* incObjNums2=new int[N-1];
					int* incObjOffs2=new int[N-1];
					int index=0;
					for(int i=0; i<N; i++){
						if(Parent->includedObjNumbers[i]!=dCatalogNumber){
							incObjNums2[index]=Parent->includedObjNumbers[i];
							incObjOffs2[index]=Parent->includedObjOffsets[i];
							index++;
						}
						if(dCatalogIndex<i){
							PP->Reference[Parent->includedObjNumbers[i]]->objStreamIndex--;
						}
					}
					Parent->includedObjNumbers=incObjNums2;
					Parent->includedObjOffsets=incObjOffs2;
					Parent->numObjects--;
				}else{
					Log(LOG_ERROR, "Failed in reading the parent object stream");
					return false;
				}
			}
		}
		else{
			Log(LOG_ERROR, "Failed in reading Document catalog dictionary");
			return false;
		}
	}

	Log(LOG_INFO, "Export started");
	int i, j;
	file=new ofstream(fileName, ios::binary);

	if(!*file){
		Log(LOG_ERROR, "Failed in opening the output file"); return false;
	}

	char buffer[1024];
	PDFStr* binary=new PDFStr(1024);
	// Header
	if(PP->v_document.IsValid()){
		sprintf(buffer, "%s%s%c", HEADER, PP->v_document.v, LF);
	}else{
		Log(LOG_ERROR, "PDF version error"); return false;
	}
	writeData(buffer);
	// binary comment
	binary->decrData[0]='%';
	for(i=0; i<4; i++){
		binary->decrData[1+i]=0x80;
	}
	binary->decrDataLen=5;
	writeData(binary);
	sprintf(buffer, "%c", LF);
	writeData(buffer);

	if(PP->IsEncrypted()==false && encryption==true){
		Log(LOG_INFO, "Encrypt dictionary is newly created");
		PP->encryptObjNum=PP->AddNewReference(Type::Dict);
		PP->trailer.Append("Encrypt", PP->Reference[PP->encryptObjNum], Type::Indirect);
	}
	
	// body
	// list of ref numbers with data, excluding data in the object stream
	Log(LOG_INFO, "List of Ref numbers with data");
	for(i=0; i<PP->ReferenceSize; i++){
		if(!(PP->Reference[i]->objStream) && PP->Reference[i]->used){
			Log(LOG_INFO, "Ref #%4d: genNumber %2d", i, PP->Reference[i]->genNumber);
		}
	}
	
	// export
	for(i=0; i<PP->ReferenceSize; i++){
		if(PP->Reference[i]->objStream ||  !PP->Reference[i]->used){
			continue;
		}
		Log(LOG_INFO, "Export Ref #%4d", i);
		void* refObj;
		int objType;
		if(XRefStmUsed && i==PP->lastXRefStm){
			// XRef stream is exported at the last
			continue;
		}else{
			if(i==PP->encryptObjNum){
				refObj=PP->encryptObj_ex->ExportDict();
				objType=Type::Dict;
			}else if(PP->ReadRefObj(PP->Reference[i], &refObj, &objType)){
				Log(LOG_DEBUG, "Object type is %d", objType);
			}else{
				Log(LOG_ERROR, "ReadRefObj error, skip it");
				continue;
			}
		}		
		// if the object is a decoded object stream, re-fill the stream and encode data
		if(objType==Type::Stream){
			Stream* streamObject=(Stream*)refObj;
			if(streamObject->used==true && streamObject->objStream){
				Log(LOG_DEBUG, "This object stream is an used object stream");
				if(!constructObjStm(streamObject)){
					Log(LOG_ERROR, "Object stream construction failed");
					return false;
				}
			}
		}
		// (objNum) (genNum) obj
		PP->Reference[i]->position=count;
		sprintf(buffer, "%d %d obj%c", i, PP->Reference[i]->genNumber, LF);
		writeData(buffer);
		vector<unsigned char> data;
		data=exportObj(refObj, objType, encryption && (i!=PP->encryptObjNum), PP->Reference[i]->objNumber, PP->Reference[i]->genNumber);
		PDFStr* data_str=new PDFStr(data.size());
		for(j=0; j<data.size(); j++){
			data_str->decrData[j]=data[j];
		}
		writeData(data_str);
		sprintf(buffer, "%cendobj%c", LF, LF);
		writeData(buffer);
	}
	if(XRefStmUsed){
		// XRef stream
		if(PP->lastXRefStm<0){
			// no XRef stream exists in the original
			PP->lastXRefStm=PP->AddNewReference(Type::Stream);
		}
		PP->Reference[PP->lastXRefStm]->position=count;
	}
	int trailerPosition=count;

	// trailer preparation
	if(!encryption){
		// remove "Encrypt"
		int encryptIndex=PP->trailer.Search("Encrypt");
		if(encryptIndex>=0){
			PP->trailer.Delete(encryptIndex);
		}
	}
	// remove "Prev"
	int prevIndex=PP->trailer.Search("Prev");
	if(prevIndex>=0){
		PP->trailer.Delete(prevIndex);
	}

	if(XRefStmUsed){
		// XRef stream
		Log(LOG_INFO, "Export cross-reference stream");
		constructXRefStm();
	
		sprintf(buffer, "%d %d obj%c", PP->lastXRefStm, PP->Reference[PP->lastXRefStm]->genNumber, LF);
		writeData(buffer);
		vector<unsigned char> data;
		data=exportObj(&XRefStm, Type::Stream, false);
		PDFStr* data_str=new PDFStr(data.size());
		for(j=0; j<data.size(); j++){
			data_str->decrData[j]=data[j];
		}
		writeData(data_str);
		sprintf(buffer, "%cendobj%c", LF, LF);
		writeData(buffer);
	}else{
		// XRef table
		Log(LOG_INFO, "Export cross-reference table");
		sprintf(buffer, "xref%c%c", CR, LF);
		writeData(buffer);
		// 0 -> free
		// 1 -> used
		// 2 -> in the object stream
		int* typeList=new int[PP->ReferenceSize];
		for(i=0; i<PP->ReferenceSize; i++){
			if(PP->Reference[i]->objStream){
				typeList[i]=2;
			}else if(PP->Reference[i]->used){
				typeList[i]=1;
			}else{
				typeList[i]=0;
			}
		}
		int numSubsections=1;
		i=0;
		while(i<PP->ReferenceSize){
			if(typeList[i]==0 || typeList[i]==1){
				i++;
				continue;
			}else{
				while(typeList[i]==2 && i<PP->ReferenceSize){
					i++;
				}
				if(i<PP->ReferenceSize){
					numSubsections++;
				}
			}
		}
		int firstIndex[numSubsections];
		int numEntries[numSubsections];
		bool in_list=true;
		i=0;
		int subsectionIndex=0;
		firstIndex[0]=0;
		while(i<PP->ReferenceSize){
			if(typeList[i]==0 || typeList[i]==1){
				i++;
				continue;
			}else{
				in_list=false;
				numEntries[subsectionIndex]=i-firstIndex[subsectionIndex];
				while(typeList[i]==2 && i<PP->ReferenceSize){
					i++;
				}
				if(i<PP->ReferenceSize){
					subsectionIndex++;
					firstIndex[subsectionIndex]=i;
					in_list=true;
				}
			}
		}
		if(in_list){
			numEntries[subsectionIndex]=i-firstIndex[subsectionIndex];
		}
		int k;
		for(k=0; k<numSubsections; k++){
			sprintf(buffer, "%d %d%c", firstIndex[k], numEntries[k], LF);
			writeData(buffer);
			for(i=0; i<numEntries[k]; i++){
				int index=i+firstIndex[k];
				int nextFreeNumber;
				if(!(PP->Reference[index]->objStream) && PP->Reference[index]->used){
					// used
					// no offset
					sprintf(buffer, "%010d %05d %c%c%c", PP->Reference[index]->position, PP->Reference[index]->genNumber,'n', CR, LF);
					writeData(buffer);
				}else{
					// free
					nextFreeNumber=0;
					for(j=index+1; j<PP->ReferenceSize; j++){
						if(!PP->Reference[j]->objStream && !PP->Reference[j]->used){
							nextFreeNumber=j;
							break;
						}
					}
					sprintf(buffer, "%010d %05d %c%c%c", nextFreeNumber, PP->Reference[index]->genNumber, 'f', CR, LF);
					writeData(buffer);
				}
			}
		}
	
		sprintf(buffer, "trailer%c", LF);
		writeData(buffer);	
		vector<unsigned char> data2=exportObj((void*)&(PP->trailer), Type::Dict, false);
		// PP->trailer.Print();
		PDFStr* data_uc2=new PDFStr(data2.size());
		for(i=0; i<data2.size(); i++){
			data_uc2->decrData[i]=data2[i];
		}
		writeData(data_uc2);
	}
	
	// footer
	sprintf(buffer, "%cstartxref%c%d%c%%%%EOF", LF, LF, trailerPosition, LF);
	writeData(buffer);
	file->close();
	Log(LOG_INFO, "Export finished");
	return true;
}

void PDFExporter::writeData(char* buffer){
	int bufLen=strlen(buffer);
	file->write(buffer, bufLen);
	count+=bufLen;
	Log(LOG_DEBUG, "---- %5d bytes written, total %6d bytes", bufLen, count);
}

void PDFExporter::writeData(PDFStr* binary){
	file->write((char*)binary->decrData, binary->decrDataLen);
	count+=binary->decrDataLen;
	Log(LOG_DEBUG, "---- %5d bytes written, total %6d bytes", binary->decrDataLen, count);
}


// objNumber, genNumber are used when encryption of string
vector<unsigned char> PDFExporter::exportObj(void* obj, int objType, bool encryption){
	return exportObj(obj, objType, encryption, 0, 0);
}


vector<unsigned char> PDFExporter::exportObj(void* obj, int objType, bool encryption, int objNumber, int genNumber){
	vector<unsigned char> ret;
	int i, j;
	bool* obj_b;
	int* obj_i;
	double* obj_do;
	unsigned char* obj_n;
	PDFStr* obj_st;
	int numNormalLetters;
	int len;
	char buffer[1024];
	Array* obj_a;
	int aSize;

	void* value;
	int type;
	unsigned char* key;
	
	Dictionary* obj_d;
	int dSize;
	vector<unsigned char> ret2;
	vector<unsigned char> ret3;

	Stream* obj_s;
	int obj_s_length;
	unsigned char* obj_s_data;

	Indirect* obj_in;
	
	switch(objType){
	case Type::Bool:
		obj_b=(bool*)obj;
		if(*obj_b){
			sprintf(buffer, "true");
		}else{
			sprintf(buffer, "false");
		}
		for(i=0; i<strlen(buffer); i++){
			ret.push_back((unsigned char)buffer[i]);
		}
		break;
	case Type::Int:
		obj_i=(int*)obj;
		sprintf(buffer, "%d", *obj_i);
		for(i=0; i<strlen(buffer); i++){
			ret.push_back((unsigned char)buffer[i]);
		}
		break;
	case Type::Real:
		obj_do=(double*)obj;
		sprintf(buffer, "%f", *obj_do);
		for(i=0; i<strlen(buffer); i++){
			ret.push_back((unsigned char)buffer[i]);
		}
		break;
	case Type::String:
		obj_st=(PDFStr*)obj;
		// encryption
		if(encryption){
			PP->encryptObj_ex->EncryptString(obj_st, objNumber, genNumber);
			obj_s_length=obj_st->encrDataLen;
			obj_s_data=obj_st->encrData;
		}else{
			obj_s_length=obj_st->decrDataLen;
			obj_s_data=obj_st->decrData;
		}
		
		// count the number of normal letters (0x21~0x7E)
		numNormalLetters=0;
		for(i=0; i<obj_s_length; i++){
			if(0x21<=obj_s_data[i] && obj_s_data[i]<=0x7E){
				numNormalLetters++;
			}
		}
		if(obj_st->isHexStr==false && 1.0*numNormalLetters/obj_s_length>literalStringBorder){
			// literal
			ret.push_back((unsigned char)'(');
			for(i=0; i<obj_s_length; i++){
				switch(obj_s_data[i]){
				case 0x0A: // line feed
					ret.push_back((unsigned char)'\\');
					ret.push_back((unsigned char)'n');
					break;
				case 0x0D: // CR
					ret.push_back((unsigned char)'\\');
					ret.push_back((unsigned char)'r');
					break;
				case 0x09: // tab
					ret.push_back((unsigned char)'\\');
					ret.push_back((unsigned char)'t');
					break;
				case 0x08: // backspace
					ret.push_back((unsigned char)'\\');
					ret.push_back((unsigned char)'b');
					break;
				case 0x0C: // FF
					ret.push_back((unsigned char)'\\');
					ret.push_back((unsigned char)'f');
					break;
				case 0x28: // left parenthesis
					ret.push_back((unsigned char)'\\');
					ret.push_back((unsigned char)'(');
					break;
				case 0x29: // right parenthesis
					ret.push_back((unsigned char)'\\');
					ret.push_back((unsigned char)')');
					break;
				case 0x5C: // backslash
					ret.push_back((unsigned char)'\\');
					ret.push_back((unsigned char)'\\');
					break;
				default:
					if(0x21<=obj_s_data[i] && obj_s_data[i] <=0x7E){
						// normal letter
						ret.push_back(obj_s_data[i]);
					}else{
						// octal
						sprintf(buffer, "\\%03o", obj_s_data[i]);
						for(j=0; j<strlen(buffer); j++){
							ret.push_back((unsigned char)buffer[j]);
						}
					}
					break;
				}
			}
			
			ret.push_back((unsigned char)')');
		}else{
			// hexadecimal
			ret.push_back((unsigned char)'<');
			for(i=0; i<obj_s_length; i++){
				sprintf(buffer, "%02x", obj_s_data[i]);
				ret.push_back((unsigned char)buffer[0]);
				ret.push_back((unsigned char)buffer[1]);
			}
			ret.push_back((unsigned char)'>');
			
		}
		break;
	case Type::Name:
	  obj_n=(unsigned char*)obj;
		len=unsignedstrlen(obj_n);
		ret.push_back((unsigned char)'/');
		for(i=0; i<len; i++){
			// 0x21~0x7E except #(0x23): normal
			if(0x21<=obj_n[i] && obj_n[i]<=0x7E && obj_n[i]!=0x23){
				ret.push_back(obj_n[i]);
			}else{
				// exception: #xx
				ret.push_back((unsigned char)'#');
				sprintf(buffer, "%02x", obj_n[i]);
				ret.push_back((unsigned char)buffer[0]);
				ret.push_back((unsigned char)buffer[1]);
			}
		}
		break;
	case Type::Array:
	  obj_a=(Array*) obj;
	  aSize=obj_a->GetSize();
		ret.push_back((unsigned char)'[');
		for(i=0; i<aSize; i++){
			if(obj_a->Read(i, &value, &type)){
			  ret2=exportObj(value, type, encryption);
				copy(ret2.begin(), ret2.end(), back_inserter(ret));
				ret.push_back((unsigned char)' ');
			}
		}
		ret.push_back((unsigned char)']');
		break;
	case Type::Dict:
	  obj_d=(Dictionary*) obj;
		dSize=obj_d->GetSize();
		ret.push_back((unsigned char)'<');
		ret.push_back((unsigned char)'<');
		for(i=0; i<dSize; i++){
			if(obj_d->Read(i, &key, &value, &type)){
			  ret2=exportObj(key, Type::Name, encryption);
				copy(ret2.begin(), ret2.end(), back_inserter(ret));
				ret.push_back((unsigned char)' ');
			  ret3=exportObj(value, type, encryption, objNumber, genNumber);
				copy(ret3.begin(), ret3.end(), back_inserter(ret));
				ret.push_back((unsigned char)' ');
			}
		}
		ret.push_back((unsigned char)'>');
		ret.push_back((unsigned char)'>');
		break;
	case Type::Stream:
		obj_s=(Stream*)obj;
		if(encryption){
			PP->encryptObj_ex->EncryptStream(obj_s);
			obj_s->StmDict.Update((unsigned char*)"Length", &(obj_s->encrDataLen), Type::Int);
		}else{
			obj_s->StmDict.Update((unsigned char*)"Length", &(obj_s->encoDataLen), Type::Int);
		}
		// stream dictionary
		ret2=exportObj((void*)&(obj_s->StmDict), Type::Dict, encryption, objNumber, genNumber);
		copy(ret2.begin(), ret2.end(), back_inserter(ret));
		sprintf(buffer, "%cstream%c", LF, LF);
		for(i=0; i<strlen(buffer); i++){
			ret.push_back((unsigned char)buffer[i]);
		}
		// stream data
		if(encryption){
			for(i=0; i<obj_s->encrDataLen; i++){
				ret.push_back(obj_s->encrData[i]);
			}
		}else{
			for(i=0; i<obj_s->encoDataLen; i++){
				ret.push_back(obj_s->encoData[i]);
			}
		}
		// stream footer
		sprintf(buffer, "%cendstream", LF);
		for(i=0; i<strlen(buffer); i++){
			ret.push_back((unsigned char)buffer[i]);
		}
		break;
	case Type::Null:
		sprintf(buffer, "null");
		for(i=0; i<strlen(buffer); i++){
			ret.push_back((unsigned char)buffer[i]);
		}
		break;
	case Type::Indirect:
		obj_in=(Indirect*) obj;
		sprintf(buffer, "%d %d R", obj_in->objNumber, obj_in->genNumber);
		for(i=0; i<strlen(buffer); i++){
			ret.push_back((unsigned char)buffer[i]);
		}
		break;
	}
	return ret;
}

void PDFExporter::constructXRefStm(){
	Log(LOG_DEBUG, "Construct XRef stream");
	XRefStm.objNumber=PP->lastXRefStm;
	XRefStm.genNumber=0;
	unsigned char* typeObj=new unsigned char[5];
	unsigned char* filterObj=new unsigned char[12];
	Dictionary* decodeParmsObj=new Dictionary();
	unsignedstrcpy(typeObj, "XRef");
	unsignedstrcpy(filterObj, "FlateDecode");
	XRefStm.StmDict.Append("Type", (void*)typeObj, Type::Name);
	XRefStm.StmDict.Append("Filter", (void*)filterObj, Type::Name);
	XRefStm.StmDict.Append("Size", (void*)&(PP->ReferenceSize), Type::Int);
	XRefStm.StmDict.Append("DecodeParms", (void*)decodeParmsObj, Type::Dict);

	// 0 -> free
	// 1 -> used
	// 2 -> in the object stream
	// -1 -> used but not listable because objNumber>=lastXRefStm
	int* typeList=new int[PP->ReferenceSize];
	int ok_count=0;
	int i, j, k;
	for(i=0; i<PP->ReferenceSize; i++){
		if(PP->Reference[i]->objStream){
			typeList[i]=2;
			ok_count++;
		}else if(PP->Reference[i]->used){
			if(true || i<PP->lastXRefStm){
				typeList[i]=1;
				ok_count++;
			}else{
				typeList[i]=-1;
			}
		}else{
			typeList[i]=0;
			ok_count++;
		}
	}
	/*
	cout << "TypeList: ";
	for(i=0; i<PP->ReferenceSize; i++){
		printf("%2d ", typeList[i]);
	}
	cout << endl;*/
	// prepare Index array
	Array* indexArr=new Array();
	int* zero=new int(0);
	indexArr->Append((void*)zero, Type::Int);
	i=0;
	int start=0;
	bool in_list=true;
	while(i<PP->ReferenceSize){
		if(typeList[i]>=0){
			i++;
			continue;
		}else{
			in_list=false;
			int* length1=new int(i-start);
			indexArr->Append((void*)length1, Type::Int);
			while(typeList[i]<0 && i<PP->ReferenceSize){
				i++;
			}
			if(i<PP->ReferenceSize){
				start=i;
				int*start2=new int(i);
				indexArr->Append((void*)start2, Type::Int);
				in_list=true;
			}
		}
	}
	if(in_list){
		int* length2=new int(i-start);
		indexArr->Append((void*)length2, Type::Int);
	}
	XRefStm.StmDict.Append("Index", (void*)indexArr, Type::Array);
	// list
	int** dataList=new int*[ok_count];
	int index=0;
	for(i=0; i<PP->ReferenceSize; i++){
		if(typeList[i]>=0){
			dataList[index]=new int[3];
			dataList[index][0]=typeList[i];
			int nextFree=0;
			switch(typeList[i]){
			case 0:
				// free -> next free object
				for(j=i+1; j<PP->ReferenceSize; j++){
					if(typeList[j]==0){
						nextFree=j;
						break;
					}
				}
				dataList[index][1]=nextFree;
				dataList[index][2]=PP->Reference[i]->genNumber;
				break;
			case 1:
				// used
				dataList[index][1]=PP->Reference[i]->position;
				dataList[index][2]=PP->Reference[i]->genNumber;
				break;
			case 2:
				// used, in the object stream
				dataList[index][1]=PP->Reference[i]->objStreamNumber;
				dataList[index][2]=PP->Reference[i]->objStreamIndex;
				break;
			default:
				cout << "typeList error" << endl;
				break;
			}
			index++;
		}
	}
	/*
	for(i=0; i<ok_count; i++){
		printf("%d %d %d\n", dataList[i][0], dataList[i][1], dataList[i][2]);
		}*/
	// determine W
	int* W=new int[3];
	W[0]=1;
	W[1]=1;
	W[2]=1;
	for(i=0; i<ok_count; i++){
		for(j=0; j<3; j++){
			int b=byteSize(dataList[i][j]);
			if(W[j]<b){
				W[j]=b;
			}
		}
	}
	// printf("W: %d %d %d\n", W[0], W[1], W[2]);
	Array* WArr=new Array();
	for(i=0; i<3; i++){
		WArr->Append((void*)&(W[i]), Type::Int);
	}
	XRefStm.StmDict.Append("W", (void*)WArr, Type::Array);
	int sumW=W[0]+W[1]+W[2];
	XRefStm.decoDataLen=sumW*ok_count;
	XRefStm.decoData=new unsigned char[XRefStm.decoDataLen];
	int di=0;
	for(i=0; i<ok_count; i++){
		for(j=0; j<3; j++){
			int v=dataList[i][j];
			for(k=0; k<W[j]; k++){
				XRefStm.decoData[di+W[j]-1-k]=(unsigned char)(v%256);
				v=(v-v%256)/256;
			}
			di+=W[j];
		}
	}
	/*
	for(i=0; i<XRefStm.dlength; i++){
		printf("%02x ", XRefStm.decoded[i]);
	}
	cout << endl;*/
  XRefStm.Encode();
	XRefStm.StmDict.Append("Length", (void*)&(XRefStm.encoDataLen), Type::Int);
	XRefStm.StmDict.Merge(PP->trailer);
}

bool PDFExporter::constructObjStm(Stream* objStream){
	// Document catalog dictionary may be skipped (when encryption is on)
	Log(LOG_DEBUG, "Construct object stream");
	PDFStr** exported_objects=new PDFStr*[objStream->numObjects];
	int i, j;
	for(i=0; i<objStream->numObjects; i++){
		int objNumber=objStream->includedObjNumbers[i];
		Log(LOG_DEBUG, "#%d: object number %d", i, objNumber);
		void* object;
		int objType;
		if(PP->ReadRefObj(PP->Reference[objNumber], &object, &objType)){
			vector<unsigned char> data;
			data=exportObj(object, objType, false, objNumber, 0);
			exported_objects[i]=new PDFStr(data.size());
			for(j=0; j<data.size(); j++){
				exported_objects[i]->decrData[j]=data[j];
			}
		}else{
			Log(LOG_ERROR, "ReadRefObj error, skip it");
			return false;
		}
	}

	// body
	int body_length=0;
	for(i=0; i<objStream->numObjects; i++){
		Log(LOG_DEBUG, "#%d: Length %d", i, exported_objects[i]->decrDataLen);
		body_length+=exported_objects[i]->decrDataLen+2; // +2 is for CRLF
	}
	unsigned char* objStream_body=new unsigned char[body_length];
	int currentpos=0;
	for(i=0; i<objStream->numObjects; i++){
		objStream->includedObjOffsets[i]=currentpos;
		for(j=0; j<exported_objects[i]->decrDataLen; j++){
			objStream_body[currentpos]=exported_objects[i]->decrData[j];
			currentpos++;
		}
		objStream_body[currentpos]=CR;
		objStream_body[currentpos+1]=LF;
		currentpos+=2;
	}
	// Log(LOG_DEBUG, "Constructed object stream: %s", objStream->decoData);

	// header
	vector<unsigned char> header;
	char* headerBuffer=new char[64];
	for(i=0; i<objStream->numObjects; i++){
		sprintf(headerBuffer, "%d %d ", objStream->includedObjNumbers[i], objStream->includedObjOffsets[i]);
		for(j=0; j<strlen(headerBuffer); j++){
			header.push_back((unsigned char)headerBuffer[j]);
		}
	}
	header.push_back(CR);
	header.push_back(LF);
	//unsigned char* objStream_header=new unsigned char[header.size()+1];
	//for(i=0; i<header.size(); i++){
	//	objStream_header[i]=header[i];
	//}
	//objStream_header[header.size()]='\0';
	//Log(LOG_DEBUG, "Object stream header: %s", objStream_header);

	// delete[] objStream->decoData;
	objStream->decoDataLen=header.size()+body_length;
	objStream->decoData=new unsigned char[objStream->decoDataLen+1];
	for(i=0; i<header.size(); i++){
		objStream->decoData[i]=header[i];
	}
	for(i=0; i<body_length; i++){
		objStream->decoData[i+header.size()]=objStream_body[i];
	}
	objStream->decoData[objStream->decoDataLen]='\0';

	int* First=new int();
	*First=header.size();
	objStream->StmDict.Update("First", First, Type::Int);
	objStream->StmDict.Update("N", &(objStream->numObjects), Type::Int);
	objStream->Encode();

	delete[] headerBuffer;
	delete[] objStream_body;

	return true;
}