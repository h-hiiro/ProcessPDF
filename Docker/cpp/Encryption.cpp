// class Encryption

#include <iostream>
#include <termios.h>
#include <stdio.h>
#include <unistd.h>
#include <cstring>
#include <gsasl.h>
#include <cstdlib>
#include <ctime>

#include "Encryption.hpp"

#ifndef LOG_INCLUDED
#include "log.hpp"
#endif

#include "variables_ext.hpp"

/*
OpenSSL
EVP_MD_CTX *EVP_MD_CTX_new(void);
int EVP_DigestInit_ex(EVP_MD_CTX *ctx, const EVP_MD *type, ENGINE *impl);
int EVP_DigestUpdate(EVP_MD_CTX *ctx, const void *d, size_t cnt);
int EVP_DigestFinal_ex(EVP_MD_CTX *ctx, unsigned char *md, unsigned int *s);

int EVP_Digest(const void *data, size_t count, unsigned char *md,
                unsigned int *size, const EVP_MD *type, ENGINE *impl);


EVP_CIPHER_CTX *EVP_CIPHER_CTX_new(void);
int EVP_CIPHER_CTX_set_key_length(EVP_CIPHER_CTX *x, int keylen);
int EVP_EncryptInit_ex(EVP_CIPHER_CTX *ctx, const EVP_CIPHER *type,
                        ENGINE *impl, const unsigned char *key, const unsigned char *iv);
int EVP_EncryptInit_ex2(EVP_CIPHER_CTX *ctx, const EVP_CIPHER *type,
                         const unsigned char *key, const unsigned char *iv,
                         const OSSL_PARAM params[]);
int EVP_EncryptUpdate(EVP_CIPHER_CTX *ctx, unsigned char *out,
                       int *outl, const unsigned char *in, int inl);
int EVP_EncryptFinal_ex(EVP_CIPHER_CTX *ctx, unsigned char *out, int *outl);

int EVP_DecryptUpdate(EVP_CIPHER_CTX *ctx, unsigned char *out,
                       int *outl, const unsigned char *in, int inl);
 */

using namespace std;
Encryption::Encryption():
	error(false),
	Idn((unsigned char*)"Identity"),
	CFexist(false),
	encryptMeta(true),
	FEKObtained(false)
{
	OSSL_PROVIDER *prov_leg=OSSL_PROVIDER_load(NULL, "legacy");
	OSSL_PROVIDER *prov_def=OSSL_PROVIDER_load(NULL, "default");
	CF=new Dictionary();
}
Encryption::Encryption(Encryption* original):
	encryptDict(original->encryptDict),
	V(original->V),
	error(original->error),
	Length(original->Length),
	Length_bytes(original->Length_bytes),
	CFexist(original->CFexist),
	CF(original->CF),
	StmF(original->StmF),
	StrF(original->StrF),
	Idn(original->Idn),
	R(original->R),
	O(original->O),
	U(original->U),
	OE(original->OE),
	UE(original->UE),
	Perms(original->Perms),
	FEK(original->FEK),
	FEKObtained(original->FEKObtained),
	encryptMeta(original->encryptMeta)
{
	int i=0;
	for(i=0; i<32; i++){
		P[i]=original->P[i];
	}
			
}
Encryption::Encryption(Dictionary* encrypt, Array* ID):
	// member initializer
	encryptDict(encrypt),
	error(false),
	Idn((unsigned char*)"Identity"),
	CFexist(false),
	encryptMeta(true),
	FEKObtained(false)
{
	OSSL_PROVIDER *prov_leg=OSSL_PROVIDER_load(NULL, "legacy");
	OSSL_PROVIDER *prov_def=OSSL_PROVIDER_load(NULL, "default");
	// initialize gsasl
	Gsasl *ctx=NULL;
	int rc;
	rc=gsasl_init(&ctx);
	if(rc!=GSASL_OK){
		Log(LOG_ERROR, "GSASL initialization error(%d): %s", rc, gsasl_strerror(rc));
		error=true;
		return;
	}
	
	int i, j;
	Log(LOG_INFO, "Read Encrypt information");
	// Filter: only /Standard can be processed in this program
	unsigned char* filter;
	if(encryptDict->Read("Filter", (void**)&filter, Type::Name)){
		if(unsignedstrcmp(filter, "Standard")){
			Log(LOG_INFO, "Filter name is Standard");
		}else{
			Log(LOG_ERROR, "Filter %s is not supported", filter);
			error=true;
			return;
		}
	}else{
		Log(LOG_ERROR, "Failed in reading Filter");
		error=true;
		return;
	}
	// V: 1-5
	int* VValue;
	if(encryptDict->Read("V", (void**)&VValue, Type::Int)){
		V=*(VValue);
		if(V==1 || V==2 || V==4 || V==5){
			Log(LOG_INFO, "V is %d", V);
		}else{
			Log(LOG_ERROR, "%d is invalid as V", V);
			error=true;
			return;
		}
	}else{
		Log(LOG_ERROR, "Failed in reading V");
		error=true;
		return;
	}
	// Length: valid if V==2 (default 40)
	// if V==1, Length=40 bits
	// if V==4, Length=128 bits
	// if V==5, Length=256 bits
	if(V==1){
		Length=40;
	}else if(V==2){
		Length=40;
		if(encryptDict->Search("Length")>=0){
			int* lengthValue;
			if(encryptDict->Read("Length", (void**)&lengthValue, Type::Int)){
				Length=*lengthValue;
				if(Length%8!=0){
					Log(LOG_ERROR, "%d is invalid as Length", Length);
					error=true;
					return;
				}
			}else{
				Log(LOG_ERROR, "Failed in reading Length");
				error=true;
				return;
			}	
		}
	}else if(V==4){
		Length=128;
	}else if(V==5){
		Length=256;
	}
	Length_bytes=Length/8;
	Log(LOG_INFO, "File encryption key length: %d bits = %d bytes", Length, Length_bytes);
	
	// CF, StmF, StrF
	if(V==4 || V==5){
		// CF
		if(encryptDict->Search("CF")>=0){
			CFexist=true;
			if(encryptDict->Read("CF", (void**)&CF, Type::Dict)){
					// OK
			}else{
				Log(LOG_ERROR, "Failed in reading CF");
				error=true;
				return;
			}
			Log(LOG_INFO, "CF has %d entries", CF->GetSize());
			if(LOG_LEVEL>=LOG_DEBUG){
				Log(LOG_DEBUG, "CF dictionary:");
				CF->Print();
			}
		}else{
			Log(LOG_WARN, "CF not found");
		}
		// StmF
		if(encryptDict->Search("StmF")>=0){
			if(encryptDict->Read("StmF", (void**)&StmF, Type::Name)){
				// OK
			}else{
				Log(LOG_ERROR, "Failed in reading StmF");
				error=true;
				return;
			}
		}else{
			StmF=new unsigned char[unsignedstrlen(Idn)];
			unsignedstrcpy(StmF, Idn);
		}
		if(unsignedstrcmp(StmF, Idn) || (CFexist && CF->Search(StmF)>=0)){
			Log(LOG_INFO, "StmF is %s", StmF);
		}else{
			Log(LOG_ERROR, "%s cannot be used as StmF", StmF);
			error=true;
			return;
		}
		// StrF
		if(encryptDict->Search("StrF")>=0){
			if(encryptDict->Read("StrF", (void**)&StrF, Type::Name)){
				// OK
			}else{
				Log(LOG_ERROR, "Failed in reading  StrF");
				error=true;
				return;
			}
		}else{
			StrF=new unsigned char[unsignedstrlen(Idn)];
			unsignedstrcpy(StrF, Idn);
		}
		if(unsignedstrcmp(StrF, Idn) || (CFexist && CF->Search(StrF)>=0)){
			Log(LOG_INFO, "StrF is %s", StrF);
		}else{
			Log(LOG_ERROR, "%s cannot be used as StrF", StrF);			error=true;
			return;
		}
	}

	// R
	int* RValue;
	if(encryptDict->Read((unsigned char*)"R", (void**)&RValue, Type::Int)){
		R=*(RValue);
		if(2<=R && R<=6){
			Log(LOG_INFO, "R is %d", R);
		}else{
			Log(LOG_ERROR, "%d is invalid as R", R);
			error=true;
			return;
		}
	}
	// O and U
	if(encryptDict->Read((unsigned char*)"O", (void**)&O, Type::String) &&\
		 encryptDict->Read((unsigned char*)"U", (void**)&U, Type::String)){
		// OK
	}else{
		Log(LOG_ERROR, "Failed in reading O or U");
		error=true;
		return;
	}
	if(LOG_LEVEL>=LOG_DEBUG){
		Log(LOG_DEBUG, "O and U:");
		cout << "O: ";
		DumpPDFStr(O);
		cout << "U: ";
		DumpPDFStr(U);
	}
	// OE and UE
	if(R==6){
		if(encryptDict->Read("OE", (void**)&OE, Type::String) &&\
			 encryptDict->Read("UE", (void**)&UE, Type::String)){
			//OK
		}else{
			Log(LOG_ERROR, "Failed in reading OE or UE");
			error=true;
			return;
		}
		if(LOG_LEVEL>=LOG_DEBUG){
			Log(LOG_DEBUG, "OE and UE:");
			cout << "OE: ";
			DumpPDFStr(OE);
			cout << "UE: ";
			DumpPDFStr(UE);
		}
	}
	// P, bit position is from low-order to high-order
	unsigned int* PValue;
	unsigned int P_i;
	if(encryptDict->Read("P", (void**)&PValue, Type::Int)){
		P_i=*PValue;
		for(i=0; i<32; i++){
			P[i]=((P_i>>i & 1)==1);
		}
	}else{
		Log(LOG_ERROR, "Failed in reading P");
		error=true;
		return;
	}
	if(LOG_LEVEL>=LOG_INFO){
		Log(LOG_INFO, "P (1->32):");
		for(i=0; i<32; i++){
			printf("%1d", P[i] ? 1 : 0);
		}
		cout << endl;
	}
	// Perms
	if(R==6){
		if(encryptDict->Read("Perms", (void**)&Perms, Type::String)){
			// OK
		}else{
			Log(LOG_ERROR, "Failed in reading Perms");
			error=true;
			return;
		}
		if(LOG_LEVEL>=LOG_DEBUG){
			Log(LOG_DEBUG, "Perms:");
			DumpPDFStr(Perms);
		}
	}
	// EncryptMetadata
	if(V==4 || V==5){
		if(encryptDict->Search("EncryptMetadata")>=0){
			bool* encryptMetaValue;
			if(encryptDict->Read("EncryptMetadata", (void**)&encryptMetaValue, Type::Bool)){
				encryptMeta=*(encryptMetaValue);
			}else{
				Log(LOG_ERROR, "Failed in reading EncryptMetadata");
				error=true;
				return;
			}
		}
		Log(LOG_INFO, "EncryptMetadata is %s", encryptMeta ? "true": "false");
	}
	// ID
	for(i=0; i<2; i++){
		if(ID->Read(i, (void**)&(IDs[i]), Type::String)){
			// OK
		}else{
			Log(LOG_ERROR, "Failed in reading ID elements");
			error=true;
			return;
		}
	}
	
	if(LOG_LEVEL>=LOG_DEBUG){
		Log(LOG_DEBUG, "IDs:");
		for(i=0; i<2; i++){
			printf("ID[%d]: ", i);
			DumpPDFStr(IDs[i]);
		}
	}	
}
bool Encryption::IsAuthenticated(){
	return FEKObtained;
}

bool Encryption::getPerms6(){	
	if(R==6 && FEKObtained){
		// decrypt Perms
		Log(LOG_INFO, "Decrypt Perms in R=6");
		int aescount;
		int result;
		EVP_CIPHER_CTX *aesctx=EVP_CIPHER_CTX_new();
		unsigned char iv[16];
		PDFStr* Perms_decoded=new PDFStr(16);
		int i, j;
		for(i=0; i<16; i++){
			iv[i]='\0';
		}
		result=EVP_DecryptInit_ex2(aesctx, EVP_aes_256_cbc(), &(FEK->decrData[0]), &iv[0], NULL);
		if(result!=1){
			Log(LOG_ERROR, "EVP_DecryptInit failed"); return false;
		}
		result=EVP_CIPHER_CTX_set_padding(aesctx, 0);
		if(result!=1){
			Log(LOG_ERROR, "EVP_CIPHER_CTX_set_padding failed"); return false;
		}
		result=EVP_DecryptUpdate(aesctx, &(Perms_decoded->decrData[0]), &aescount, &(Perms->decrData[0]), 16);
		if(result!=1){
			Log(LOG_ERROR, "EVP_DecryptUpdate failed"); return false;
		}
		int aesfinalCount;
		result=EVP_DecryptFinal_ex(aesctx, &(Perms_decoded->decrData[aescount]), &aesfinalCount);
		if(result!=1){
			Log(LOG_ERROR, "EVP_DecryptFinal failed"); return false;
		}
		aescount+=aesfinalCount;

		bool P2[32];
		for(i=0; i<4; i++){
			for(j=0; j<8; j++){
				P2[i*8+j]=((Perms_decoded->decrData[i])>>(j))&1==1 ? true: false;
			}
		}
		// Permission check
		if(LOG_LEVEL>=LOG_DEBUG){
			Log(LOG_DEBUG, "Decoded Perms:");
			for(i=0; i<32; i++){
				printf("%01d", P2[i]?1:0);
			}
			cout << endl;
		}
		for(i=0; i<32; i++){
			if(P2[i]!=P[i]){
				Log(LOG_ERROR, "Mismatch found in Ps"); return false;
			}
		}
		Log(LOG_INFO, "P ok");
		// EncryptMetadata check
		if(Perms_decoded->decrData[8]=='T'){
			if(encryptMeta){
				// ok
			}else{
				// mismatch
				Log(LOG_ERROR, "Mismatch found in encryptMeta"); return false;
			}
		}else if(Perms_decoded->decrData[8]=='F'){
			if(!encryptMeta){
				// ok
			}else{
				// mismatch
				Log(LOG_ERROR, "Mismatch found in encryptMeta"); return false;
			}
		}else{
			Log(LOG_ERROR, "EncryptMetadata not ok"); return false;
		}
		Log(LOG_INFO, "EncryptMetadata ok");
		// 'adb'
		if(Perms_decoded->decrData[9]=='a' && Perms_decoded->decrData[10]=='d' && Perms_decoded->decrData[11]=='b'){
			Log(LOG_INFO, "'adb' ok");
		}else{
			Log(LOG_ERROR, "'adb' not ok "); return false;
		}
	}
	return true;
}

bool Encryption::DecryptString(PDFStr* str, int objNumber, int genNumber){
	Log(LOG_DEBUG, "DecryptString");
	if(!FEKObtained){
		Log(LOG_ERROR, "Not yet authenticated"); return false;
	}
	if(str->decrypted){
		// already decrypted
		return true;
	}
	// copy the original data to encrypted
	str->encrData=new unsigned char[str->decrDataLen+1];
	int i;
	for(i=0; i<str->decrDataLen; i++){
		str->encrData[i]=str->decrData[i];
	}
	str->encrData[str->decrDataLen]='\0';
	str->encrDataLen=str->decrDataLen;

	unsigned char* CFM;
	if(V<4){
		// StmF does not exist, CFM is "V2"
		CFM=new unsigned char[3];
		unsignedstrcpy(CFM, "V2");
	}else{
		if(unsignedstrcmp(StrF, Idn)){
			// Identity filter: do nothing
			str->decrypted=true;
			return true;
		}
		// use StrF
		Dictionary* filter;
		if(CF->Read(StrF, (void**)&filter, Type::Dict)){
			// OK
		}else{
			Log(LOG_ERROR, "Failed in reading StrF value"); return false;
		}

		if(filter->Read("CFM", (void**)&CFM, Type::Name)){
			// OK
		}else{
			Log(LOG_ERROR, "Failed in reading CFM"); return false;
		}
	}

	if(execDecryption(&(str->encrData), &(str->encrDataLen), &(str->decrData), &(str->decrDataLen), CFM, objNumber, genNumber)){
		str->decrypted=true;
		return true;
	}else{
		Log(LOG_ERROR, "Failed in execDecryption"); return false;
	}
}

bool Encryption::DecryptStream(Stream* stm){
	Log(LOG_DEBUG, "DecryptStream");
	if(!FEKObtained){
		Log(LOG_ERROR, "Not yet authenticated"); return false;
	}
	if(stm->decrypted){
		// already decrypted
		return true;
	}
	unsigned char* type;
	// check whether the type is XRef
	if(stm->StmDict.Read("Type", (void**)&type, Type::Name)){
		if(unsignedstrcmp(type, "XRef")){
			// XRef is not encrypted
			stm->decrypted=true;
			return true;
		}
	}
	// copy the original data (in encoData) to encrypted
	stm->encrData=new unsigned char[stm->encoDataLen+1];
	int i;
	for(i=0; i<stm->encoDataLen; i++){
		stm->encrData[i]=stm->encoData[i];
	}
	stm->encrData[stm->encoDataLen]='\0';
	stm->encrDataLen=stm->encoDataLen;

	unsigned char* CFM;
	if(V<4){
		// StmF does not exist, CFM is "V2"
		CFM=new unsigned char[3];
		unsignedstrcpy(CFM, "V2");
	}else{
		if(unsignedstrcmp(StmF, Idn)){
			// Identity filter: do nothing
			stm->decrypted=true;
			return true;
		}
		// use StmF
		Dictionary* filter;
		if(CF->Read(StmF, (void**)&filter, Type::Dict)){
			// OK
		}else{
			Log(LOG_ERROR, "Failed in reading StmF"); return false;
		}

		if(filter->Read("CFM", (void**)&CFM, Type::Name)){
			// OK
		}else{
			Log(LOG_ERROR, "Failed in reading CFM");return false;
		}
	}

	if(execDecryption(&(stm->encrData), &(stm->encrDataLen), &(stm->encoData), &(stm->encoDataLen), CFM, stm->objNumber, stm->genNumber)){
		stm->decrypted=true;
		return true;
	}else{
		Log(LOG_ERROR, "Failed in execDecryption");	return false;
	}
}

bool Encryption::EncryptStream(Stream* stm){
	if(!FEKObtained){
		Log(LOG_ERROR, "Not yet authenticated!");
		return false;
	}
	unsigned char* type;
	int typeType;
	// prepare buffer for encrypted data
	// slightly longer than original due to padding and iv (16)
	int padLength=16-(stm->encoDataLen%16);
	stm->encrData=new unsigned char[stm->encoDataLen+padLength+17];
	int i;
	
	unsigned char* CFM;
	if(V<4){
		CFM=new unsigned char[3];
		unsignedstrcpy(CFM, "V2");
	}else{
		if(unsignedstrcmp(StmF, Idn)){
			// Identity filter: do nothing
			for(i=0; i<stm->encoDataLen; i++){
				stm->encrData[i]=stm->encoData[i];
			}
			stm->encrData[stm->encoDataLen]='\0';
			stm->encrDataLen=stm->encoDataLen;
			return true;
		}
		// use StmF
		Dictionary* filter;
		if(CF->Read(StmF, (void**)&filter, Type::Dict)){
			//ok
		}else{
			Log(LOG_ERROR, "Failed in reading StmF value"); return false;
		}
		if(filter->Read("CFM", (void**)&CFM, Type::Name)){
			//ok
		}else{
			Log(LOG_ERROR, "Failed in reading CFM"); return false;
		}
	}	
	if(execEncryption(&(stm->encrData), &(stm->encrDataLen), &(stm->encoData), &(stm->encoDataLen), CFM, stm->objNumber, stm->genNumber)){
		return true;
	}else{
		Log(LOG_ERROR, "Failed in execEncryption");
		return false;
	}
	return true;
}


bool Encryption::EncryptString(PDFStr* str, int objNumber, int genNumber){
	if(!FEKObtained){
		Log(LOG_ERROR, "Not yet authenticated!123456"); return false;
	}
	// prepare buffer for encrypted data
	// slightly longer than original due to padding and iv (16)
	int padLength=16-(str->decrDataLen%16);
	str->encrData=new unsigned char[str->decrDataLen+padLength+17];
	int i;

	unsigned char* CFM;
	if(V<4){
		CFM=new unsigned char[3];
		unsignedstrcpy(CFM, "V2");
	}else{
		if(unsignedstrcmp(StrF, Idn)){
			// Identity filter: do nothing
			for(i=0; i<str->decrDataLen; i++){
				str->encrData[i]=str->decrData[i];
			}
			str->encrData[str->decrDataLen]='\0';
			str->encrDataLen=str->decrDataLen;
			return true;
		}
		// use StrF
		Dictionary* filter;
		if(CF->Read(StrF, (void**)&filter, Type::Dict)){
			// ok
		}else{
			Log(LOG_ERROR, "Failed in reading StrF value"); return false;
		}

		if(filter->Read("CFM", (void**)&CFM, Type::Name)){
			// ok
		}else{
			Log(LOG_ERROR, "Failed in reading CFM"); return false;
		}
	}

	if(execEncryption(&(str->encrData), &(str->encrDataLen), &(str->decrData), &(str->decrDataLen), CFM, objNumber, genNumber)){
		return true;
	}else{
		Log(LOG_ERROR, "Failed in execEncryption"); return false;
	}
} // end of EncryptString

bool Encryption::execDecryption(unsigned char** encrypted, int* elength, unsigned char** decrypted, int* length, unsigned char* CFM, int objNumber, int genNumber){
	int i;
	if(!FEKObtained){
		Log(LOG_ERROR, "Not yet authenticated"); return false;
	}

	if(unsignedstrcmp(CFM, "V2")){
		unsigned char hashed_md5[16];
		int result;
		EVP_MD_CTX* md5ctx=EVP_MD_CTX_new();
		result=EVP_DigestInit_ex(md5ctx, EVP_md5(), NULL);
		if(result!=1){
			Log(LOG_ERROR, "EVP_DigestInit failed"); return false;
		}
		// FEK
		result=EVP_DigestUpdate(md5ctx, &(FEK->decrData[0]), Length_bytes);
		if(result!=1){
			Log(LOG_ERROR, "EVP_DigestUpdate failed in FEK"); return false;
		}
		// object number (low-order byte first)
		int objNumber2=objNumber;
		unsigned char objNumber_c[3];
		for(i=0; i<3; i++){
			objNumber_c[i]=objNumber2%256;
			objNumber2=(objNumber2-objNumber2%256)/256;
		}
		result=EVP_DigestUpdate(md5ctx, &objNumber_c[0], 3);
		if(result!=1){
			Log(LOG_ERROR, "EVP_DigestUpdate failed in objNumber"); return false;
		}
		// gen number
		int genNumber2=genNumber;
		unsigned char genNumber_c[2];
		for(i=0; i<2; i++){
			genNumber_c[i]=genNumber2%256;
			genNumber2=(genNumber2-genNumber2%256)/256;
		}
		result=EVP_DigestUpdate(md5ctx, &genNumber_c[0], 2);
		if(result!=1){
			Log(LOG_ERROR, "EVP_DigestUpdate failed in genNumber");	return false;
		}
		// close the hash
		unsigned int count;
		result=EVP_DigestFinal_ex(md5ctx, &hashed_md5[0], &count);
		
		// key
		int key_length=min(Length_bytes+5, 16);
		unsigned char key[key_length];
		for(i=0; i<key_length; i++){
			key[i]=hashed_md5[i];
		}

		//decryption
		int rc4count;
		int rc4finalCount;
		EVP_CIPHER_CTX *rc4ctx=EVP_CIPHER_CTX_new();
		result=EVP_DecryptInit_ex2(rc4ctx, EVP_rc4(), NULL, NULL, NULL);
		if(result!=1){
			Log(LOG_ERROR, "EVP_DecryptInit failed"); return false;
		}
		result=EVP_CIPHER_CTX_set_key_length(rc4ctx, key_length);
		if(result!=1){
			Log(LOG_ERROR, "EVP_CIPHER_CTX_set_key_length failed");	return false;
		}
		Log(LOG_DEBUG, "Key length: %d", EVP_CIPHER_CTX_get_key_length(rc4ctx));
		result=EVP_DecryptInit_ex2(rc4ctx, NULL, key, NULL, NULL);
		if(result!=1){
			Log(LOG_ERROR, "EVP_DecryptInit failed"); return false;
		}
		result=EVP_DecryptUpdate(rc4ctx, &((*decrypted)[0]), &rc4count, &((*encrypted)[0]), *elength);
		if(result!=1){
			Log(LOG_ERROR, "EVP_DecryptUpdate failed");	return false;
		}
		result=EVP_DecryptFinal_ex(rc4ctx, &((*decrypted)[rc4count]), &rc4finalCount);
		if(result!=1){
			Log(LOG_ERROR, "EVP_DecryptFinal failed"); return NULL;
		}
		rc4count+=rc4finalCount;			
		*length=rc4count;
	}else if(unsignedstrcmp(CFM, "AESV2")){
		unsigned char hashed_md5[16];
		int result;
		EVP_MD_CTX* md5ctx=EVP_MD_CTX_new();
		result=EVP_DigestInit_ex(md5ctx, EVP_md5(), NULL);
		if(result!=1){
			Log(LOG_ERROR, "EVP_DigestInit failed"); return false;
		}
		// FEK
		result=EVP_DigestUpdate(md5ctx, &(FEK->decrData[0]), Length_bytes);
		if(result!=1){
			Log(LOG_ERROR, "EVP_DigestUpdate failed in FEK"); return false;
		}
		// object number (low-order byte first)
		int objNumber2=objNumber;
		unsigned char objNumber_c[3];
		for(i=0; i<3; i++){
			objNumber_c[i]=objNumber2%256;
			objNumber2=(objNumber2-objNumber2%256)/256;
		}
		result=EVP_DigestUpdate(md5ctx, &objNumber_c[0], 3);
		if(result!=1){
			Log(LOG_ERROR, "EVP_DigestUpdate failed in objNumber");	return false;
		}
		// gen number
		int genNumber2=genNumber;
		unsigned char genNumber_c[2];
		for(i=0; i<2; i++){
			genNumber_c[i]=genNumber2%256;
			genNumber2=(genNumber2-genNumber2%256)/256;
		}
		result=EVP_DigestUpdate(md5ctx, &genNumber_c[0], 2);
		if(result!=1){
			Log(LOG_ERROR, "EVP_DigestUpdate failed in genNumber");	return false;
		}
		// 'sAIT'
		unsigned char sAIT[4]={0x73, 0x41, 0x6c, 0x54};
		result=EVP_DigestUpdate(md5ctx, &sAIT[0], 4);
		if(result!=1){
			Log(LOG_ERROR, "EVP_DigestUpdate failed in sAIT"); return false;
		}
		// close the hash
		unsigned int count;
		result=EVP_DigestFinal_ex(md5ctx, &hashed_md5[0], &count);

		// key and init vector
		unsigned char key[16];
		for(i=0; i<16; i++){
			key[i]=hashed_md5[i];
		}
		unsigned char iv[16];
		for(i=0; i<16; i++){
			iv[i]=(*encrypted)[i];
		}

		// AES, CBC mode decrypt
		int aescount;
		EVP_CIPHER_CTX *aesctx=EVP_CIPHER_CTX_new();
		result=EVP_DecryptInit_ex2(aesctx, EVP_aes_128_cbc(), key, iv, NULL);
		if(result!=1){
			Log(LOG_ERROR, "EVP_DecryptInit failed"); return false;
		}
		result=EVP_DecryptUpdate(aesctx, &((*decrypted)[0]), &aescount, &((*encrypted)[16]), (*elength-16));
		if(result!=1){
			Log(LOG_ERROR, "EVP_DecryptUpdate failed");	return false;
		}
		int aesfinalCount;
		result=EVP_DecryptFinal_ex(aesctx, &((*decrypted)[aescount]), &aesfinalCount);
		if(result!=1){
			Log(LOG_ERROR, "EVP_DecryptFinal failed");
			ERR_print_errors_fp(stderr);
			return false;
		}
		aescount+=aesfinalCount;
		*length=aescount;
	}else if(unsignedstrcmp(CFM, "AESV3")){
		unsigned char iv[16];
		for(i=0; i<16; i++){
			iv[i]=(*encrypted)[i];
		}

		// AES, CBC mode decrypt
		int aescount;
		int result;
		EVP_CIPHER_CTX *aesctx=EVP_CIPHER_CTX_new();
		result=EVP_DecryptInit_ex2(aesctx, EVP_aes_256_cbc(), &(FEK->decrData[0]), iv, NULL);
		if(result!=1){
			Log(LOG_ERROR, "EVP_DecryptInit failed "); return false;
		}
		result=EVP_DecryptUpdate(aesctx, &((*decrypted)[0]), &aescount, &((*encrypted)[16]), *elength-16);
		if(result!=1){
			Log(LOG_ERROR, "EVP_DecryptUpdate failed"); return false;
		}
		int aesfinalCount;
		result=EVP_DecryptFinal_ex(aesctx, &((*decrypted)[aescount]), &aesfinalCount);
		if(result!=1){
			Log(LOG_ERROR, "EVP_DecryptFinal failed");
			ERR_print_errors_fp(stderr);
			return false;
		}
		aescount+=aesfinalCount;
		*length=aescount;
	}
	return true;
}

bool Encryption::execEncryption(unsigned char** encrypted, int* elength, unsigned char** decrypted, int* length, unsigned char* CFM, int objNumber, int genNumber){
	// in case of AESV2 and AESV3, encrypted[0-15] includes the iv
	int i;
	if(!FEKObtained){
		Log(LOG_ERROR, "Not yet authenticated"); return false;
	}
	// note: padding is automatially added for AESV2 and AESV3
	if(unsignedstrcmp(CFM, "V2")){
		unsigned char hashed_md5[16];
		int result;
		EVP_MD_CTX* md5ctx=EVP_MD_CTX_new();
		result=EVP_DigestInit_ex(md5ctx, EVP_md5(), NULL);
		if(result!=1){
			Log(LOG_ERROR, "EVP_DigestInit failed");
			return false;
		}
		// FEK
		result=EVP_DigestUpdate(md5ctx, &(FEK->decrData[0]), Length_bytes);
		if(result!=1){
			Log(LOG_ERROR, "EVP_DigestUpdate failed in FEK");	return false;
		}
		// object number (low-order byte first)
		int objNumber2=objNumber;
		unsigned char objNumber_c[3];
		for(i=0; i<3; i++){
			objNumber_c[i]=objNumber2%256;
			objNumber2=(objNumber2-objNumber2%256)/256;
		}
		result=EVP_DigestUpdate(md5ctx, &objNumber_c[0], 3);
		if(result!=1){
			Log(LOG_ERROR, "EVP_DigestUpdate failed in objNumber"); return false;
		}
		// gen number
		int genNumber2=genNumber;
		unsigned char genNumber_c[2];
		for(i=0; i<2; i++){
			genNumber_c[i]=genNumber2%256;
			genNumber2=(genNumber2-genNumber2%256)/256;
		}
		result=EVP_DigestUpdate(md5ctx, &genNumber_c[0], 2);
		if(result!=1){
			Log(LOG_ERROR, "EVP_DigestUpdate failed in genNumber");	return false;
		}
		// close the hash
		unsigned int count;
		result=EVP_DigestFinal_ex(md5ctx, &hashed_md5[0], &count);
		
		// key
		int key_length=min(Length_bytes+5, 16);
		unsigned char key[key_length];
		for(i=0; i<key_length; i++){
			key[i]=hashed_md5[i];
		}

		//encryption
		int rc4count;
		int rc4finalCount;
		EVP_CIPHER_CTX *rc4ctx=EVP_CIPHER_CTX_new();
		result=EVP_EncryptInit_ex2(rc4ctx, EVP_rc4(), NULL, NULL, NULL);
		if(result!=1){
			Log(LOG_ERROR, "EVP_EncryptInit failed"); return NULL;
		}
		result=EVP_CIPHER_CTX_set_key_length(rc4ctx, key_length);
		if(result!=1){
			Log(LOG_ERROR, "EVP_CIPHER_CTX_set_key_length failed"); return NULL;
		}
		result=EVP_EncryptInit_ex2(rc4ctx, NULL, key, NULL, NULL);
		if(result!=1){
			Log(LOG_ERROR, "EVP_EncryptInit failed"); return NULL;
		}
		result=EVP_EncryptUpdate(rc4ctx, &((*encrypted)[0]), &rc4count, &((*decrypted)[0]), *length);
		if(result!=1){
			Log(LOG_ERROR, "EVP_EncryptUpdate failed");	return NULL;
		}
		result=EVP_EncryptFinal_ex(rc4ctx, &((*encrypted)[rc4count]), &rc4finalCount);
		if(result!=1){
			Log(LOG_ERROR, "EVP_EncryptFinal failed"); return NULL;
		}
		rc4count+=rc4finalCount;
		*elength=rc4count;
	}else if(unsignedstrcmp(CFM, "AESV2")){
		unsigned char hashed_md5[16];
		int result;
		EVP_MD_CTX* md5ctx=EVP_MD_CTX_new();
		result=EVP_DigestInit_ex(md5ctx, EVP_md5(), NULL);
		if(result!=1){
			Log(LOG_ERROR, "EVP_DigestInit failed");
			return false;
		}
		// FEK
		result=EVP_DigestUpdate(md5ctx, &(FEK->decrData[0]), Length_bytes);
		if(result!=1){
			Log(LOG_ERROR, "EVP_DigestUpdate failed in FEK"); return false;
		}
		// object number (low-order byte first)
		int objNumber2=objNumber;
		unsigned char objNumber_c[3];
		for(i=0; i<3; i++){
			objNumber_c[i]=objNumber2%256;
			objNumber2=(objNumber2-objNumber2%256)/256;
		}
		result=EVP_DigestUpdate(md5ctx, &objNumber_c[0], 3);
		if(result!=1){
			Log(LOG_ERROR, "EVP_DigestUpdate failed in objNumber"); return false;
		}
		// gen number
		int genNumber2=genNumber;
		unsigned char genNumber_c[2];
		for(i=0; i<2; i++){
			genNumber_c[i]=genNumber2%256;
			genNumber2=(genNumber2-genNumber2%256)/256;
		}
		result=EVP_DigestUpdate(md5ctx, &genNumber_c[0], 2);
		if(result!=1){
			Log(LOG_ERROR, "EVP_DigestUpdate failed in genNumber"); return false;
		}
		// 'sAIT'
		unsigned char sAIT[4]={0x73, 0x41, 0x6c, 0x54};
		result=EVP_DigestUpdate(md5ctx, &sAIT[0], 4);
		if(result!=1){
			Log(LOG_ERROR, "EVP_DigestUpdate failed in sAIT"); return false;
		}
		// close the hash
		unsigned int count;
		result=EVP_DigestFinal_ex(md5ctx, &hashed_md5[0], &count);

		// key and init vector
		unsigned char key[16];
		for(i=0; i<16; i++){
			key[i]=hashed_md5[i];
		}
		unsigned char iv[16];
		prepareIV(iv);
		for(i=0; i<16; i++){
			(*encrypted)[i]=iv[i];
		}

		// AES, CBC mode encrypt
		int aescount;
		EVP_CIPHER_CTX *aesctx=EVP_CIPHER_CTX_new();
		result=EVP_EncryptInit_ex2(aesctx, EVP_aes_128_cbc(), key, iv, NULL);
		if(result!=1){
			Log(LOG_ERROR, "EVP_EncryptInit failed "); return false;
		}
		
		result=EVP_EncryptUpdate(aesctx, &((*encrypted)[16]), &aescount, &((*decrypted)[0]), *length);
		if(result!=1){
			Log(LOG_ERROR, "EVP_EncryptUpdate failed"); return false;
		}
		int aesfinalCount;
		result=EVP_EncryptFinal_ex(aesctx, &((*encrypted)[aescount+16]), &aesfinalCount);
		if(result!=1){
			Log(LOG_ERROR, "EVP_EncryptFinal failed"); return false;
		}
		aescount+=aesfinalCount;
		*elength=aescount+16;
	}else if(unsignedstrcmp(CFM, "AESV3")){
		unsigned char iv[16];
		prepareIV(iv);
		for(i=0; i<16; i++){
			(*encrypted)[i]=iv[i];
		}

		// AES, CBC mode decrypt
		int aescount;
		int result;
		EVP_CIPHER_CTX *aesctx=EVP_CIPHER_CTX_new();
		result=EVP_EncryptInit_ex2(aesctx, EVP_aes_256_cbc(), &(FEK->decrData[0]), iv, NULL);
		if(result!=1){
			Log(LOG_ERROR, "EVP_EncryptInit failed "); return false;
		}
		result=EVP_EncryptUpdate(aesctx, &((*encrypted)[16]), &aescount, &((*decrypted)[0]), *length);
		if(result!=1){
			Log(LOG_ERROR, "EVP_EncryptUpdate failed");
			return false;
		}
		// cout << aescount << endl;
		int aesfinalCount;
		result=EVP_EncryptFinal_ex(aesctx, &((*encrypted)[aescount+16]), &aesfinalCount);
		if(result!=1){
			Log(LOG_ERROR, "EVP_EncryptFinal failed"); return false;
		}
		aescount+=aesfinalCount;
		*elength=aescount+16;
	}
	return true;
}

bool Encryption::AuthOwner(PDFStr* pwd){
	if(error){
		Log(LOG_ERROR, "The object has error(s)"); return false;
	}
	int i;
	Log(LOG_INFO, "Owner authentication trial");
	Log(LOG_INFO, "Password length is %d", pwd->decrDataLen);
	if(LOG_LEVEL>=LOG_DEBUG){
		Log(LOG_DEBUG, "Trial password:");
		DumpPDFStr(pwd);
	}

	if(R<=4){
		PDFStr* RC4fek=RC4EncryptionKey(pwd);
		if(RC4fek==NULL){return false;}
		if(LOG_LEVEL>=LOG_DEBUG){
			Log(LOG_DEBUG, "RC4 file encryption key:");
			DumpPDFStr(RC4fek);
		}
		PDFStr* trialUserPassword=DecryptO(RC4fek);
		if(trialUserPassword==NULL){return false;}
		if(LOG_LEVEL>=LOG_DEBUG){
			Log(LOG_DEBUG, "Trial user password (padded):");
			DumpPDFStr(trialUserPassword);
		}
		return AuthUser(trialUserPassword);
	}else if(R==6){
		PDFStr* tO=trialO6(pwd);
		if(tO==NULL){return false;}
		if(LOG_LEVEL>=LOG_DEBUG){
			Log(LOG_DEBUG, "Trial O:");
			DumpPDFStr(tO);
		}
		for(i=0; i<tO->decrDataLen; i++){
			if(tO->decrData[i]!=O->decrData[i]){
				Log(LOG_WARN, "Mismatch found in O and trial O"); return false;
			}
		}
		Log(LOG_INFO, "O and trial O match, authenticated");
		FEK=fileEncryptionKey6(pwd, true);
		if(FEK==NULL){
			Log(LOG_ERROR, "Failed in obtaining FEK");
			return false;
		}
		if(LOG_LEVEL>=LOG_DEBUG){
			Log(LOG_DEBUG, "FEK:");
			DumpPDFStr(FEK);
		}
		bool isFirstAuth=!FEKObtained;
		FEKObtained=true;
		if(isFirstAuth){
			if(getPerms6()==false){
				error=true;
			}
		}
		return true;
	}
	Log(LOG_ERROR, "%d is invalid as R", R);
	return false;
}

bool Encryption::AuthUser(PDFStr* pwd){
	if(error){
		Log(LOG_ERROR, "The object has error(s)"); return false;
	}
	int i;
	Log(LOG_INFO, "User authentication trial");
	Log(LOG_INFO, "Password length is %d", pwd->decrDataLen);
	if(LOG_LEVEL>=LOG_DEBUG){
		Log(LOG_DEBUG, "Trial password:");
		DumpPDFStr(pwd);
	}
	if(R<=4){
		PDFStr* fek=fileEncryptionKey(pwd);
		if(fek==NULL){return false;}
		if(LOG_LEVEL>=LOG_DEBUG){
			Log(LOG_DEBUG, "Trial file encryption key:");
			DumpPDFStr(fek);
		}
  
		PDFStr* tU=trialU(fek);
		if(tU==NULL){return false;}
		if(LOG_LEVEL>=LOG_DEBUG){
			Log(LOG_DEBUG, "Trial U:");
			DumpPDFStr(tU);
		}

		for(i=0; i<tU->decrDataLen; i++){
			if(tU->decrData[i]!=U->decrData[i]){
				Log(LOG_WARN, "Mismatch found in U and trial U");	return false;
			}
		}
		Log(LOG_INFO, "U and trial U match");
		// save fileEncryptionKey
		FEK=fek;
		FEKObtained=true;
		return true;
	}else if(R==6){
		PDFStr* tU=trialU6(pwd);
		if(tU==NULL){return false;}
		if(LOG_LEVEL>=LOG_DEBUG){
			Log(LOG_DEBUG, "Trial U:");
			DumpPDFStr(tU);
		}
		for(i=0; i<tU->decrDataLen; i++){
			if(tU->decrData[i]!=U->decrData[i]){
				Log(LOG_WARN, "Mismatch found in U and trial U");	return false;
			}
		}
		Log(LOG_INFO, "U and trial U match, authenticated");
		FEK=fileEncryptionKey6(pwd, false);
		if(FEK==NULL){
			Log(LOG_ERROR, "Failed in obtaining FEK");
			return false;
		}
		if(LOG_LEVEL>=LOG_DEBUG){
			Log(LOG_DEBUG, "FEK:");
			DumpPDFStr(FEK);
		}
		bool isFirstAuth=!FEKObtained;
		FEKObtained=true;
		if(isFirstAuth){
			if(getPerms6()==false){
				error=true;
			}
		}
		return true;
	}
	Log(LOG_ERROR, "%d is invalid as R", R); return false;
}

PDFStr* Encryption::trialU(PDFStr* fek){
	if(error){
		return NULL;
	}
	int i,j;
	if(R==2){
		// encrypt the PADDING with fek
		int rc4count;
		unsigned char encrypted_rc4[32];
		EVP_CIPHER_CTX *rc4ctx=EVP_CIPHER_CTX_new();
		int result;
		result=EVP_EncryptInit_ex2(rc4ctx, EVP_rc4(), NULL, NULL, NULL);
		if(result!=1){
			Log(LOG_ERROR, "EVP_EncryptInit failed"); return NULL;
		}
		result=EVP_CIPHER_CTX_set_key_length(rc4ctx, Length_bytes);
		if(result!=1){
			Log(LOG_ERROR, "EVP_CIPHER_CTX_set_key_length failed"); return NULL;
		}
		Log(LOG_DEBUG, "Key length: %d", EVP_CIPHER_CTX_get_key_length(rc4ctx));
		result=EVP_EncryptInit_ex2(rc4ctx, NULL, fek->decrData, NULL, NULL);
		if(result!=1){
			Log(LOG_ERROR, "EVP_EncryptInit failed"); return NULL;
		}
		result=EVP_EncryptUpdate(rc4ctx, &encrypted_rc4[0], &rc4count, (unsigned char*)&PADDING[0], 32);
		if(result!=1){
			Log(LOG_ERROR, "EVP_EncryptUpdate failed"); return NULL;
		}
		int rc4finalCount;
		result=EVP_EncryptFinal_ex(rc4ctx, &(encrypted_rc4[rc4count]), &rc4finalCount);
		if(result!=1){
			Log(LOG_ERROR, "EVP_EncryptFinal failed"); return NULL;
		}
		rc4count+=rc4finalCount;
		PDFStr* tU=new PDFStr(32);
		for(i=0; i<rc4count; i++){
			tU->decrData[i]=encrypted_rc4[i];
		}
		tU->decrData[rc4count]='\0';
		return tU;		
	}else if(R==3 || R==4){
		unsigned char hashed_md5[16];
		int result;
		EVP_MD_CTX* md5ctx=EVP_MD_CTX_new();
		result=EVP_DigestInit_ex(md5ctx, EVP_md5(), NULL);
		if(result!=1){
			Log(LOG_ERROR, "EVP_DigestInit failed"); return NULL;
		}
		// PADDING
		result=EVP_DigestUpdate(md5ctx, &PADDING[0], 32);
		if(result!=1){
			Log(LOG_ERROR, "EVP_DigestUpdate failed in PADDING"); return NULL;
		}
		// ID[0]
		result=EVP_DigestUpdate(md5ctx, &(IDs[0]->decrData[0]), IDs[0]->decrDataLen);
		if(result!=1){
			Log(LOG_ERROR, "EVP_DigestUpdate failed in ID[0]"); return NULL;
			return NULL;
		}
		// close
		unsigned int count;
		result=EVP_DigestFinal_ex(md5ctx, &hashed_md5[0], &count);
		
		// encrypt the hash with fek
		int rc4count;
		unsigned char encrypted_rc4[16];
		EVP_CIPHER_CTX *rc4ctx=EVP_CIPHER_CTX_new();
		result=EVP_EncryptInit_ex2(rc4ctx, EVP_rc4(), NULL, NULL, NULL);
		if(result!=1){
			Log(LOG_ERROR, "EVP_EncryptInit failed1"); return NULL;
		}
		result=EVP_CIPHER_CTX_set_key_length(rc4ctx, Length_bytes);
		if(result!=1){
			Log(LOG_ERROR, "EVP_CIPHER_CTX_set_key_length failed"); return NULL;
		}
		Log(LOG_DEBUG, "Key length: %d", EVP_CIPHER_CTX_get_key_length(rc4ctx));
		result=EVP_EncryptInit_ex2(rc4ctx, NULL, fek->decrData, NULL, NULL);
		if(result!=1){
			Log(LOG_ERROR, "EVP_EncryptInit failed2"); return NULL;
		}
		result=EVP_EncryptUpdate(rc4ctx, &encrypted_rc4[0], &rc4count, &hashed_md5[0], 16);
		if(result!=1){
			Log(LOG_ERROR, "EVP_EncryptUpdate failed"); return NULL;
		}
		int rc4finalCount;
		result=EVP_EncryptFinal_ex(rc4ctx, &(encrypted_rc4[rc4count]), &rc4finalCount);
		if(result!=1){
			Log(LOG_ERROR, "EVP_EncryptFinal failed"); return NULL;
		}
		rc4count+=rc4finalCount;
		if(LOG_LEVEL>=LOG_DEBUG){
			Log(LOG_DEBUG, "RC4 encrypted %d bytes: ", rc4count);
			for(i=0; i<rc4count; i++){
				printf("%02x ", encrypted_rc4[i]);
			}
			cout << endl;
		}
		// Loop process
		for(i=1; i<=19; i++){
			unsigned char unencrypted[16];
			unsigned char fek_i[Length_bytes];
			for(j=0; j<16; j++){
				unencrypted[j]=encrypted_rc4[j];
			}
			for(j=0; j<Length_bytes; j++){
				fek_i[j]=(fek->decrData[j])^((unsigned char)i);
			}
			result=EVP_CIPHER_CTX_reset(rc4ctx);
			if(result!=1){
				Log(LOG_ERROR, "EVP_CIPHER_CTX_reset failed"); return NULL;
			}
			result=EVP_EncryptInit_ex2(rc4ctx, EVP_rc4(), &fek_i[0], NULL, NULL);
			if(result!=1){
				Log(LOG_ERROR, "EVP_EncryptInit failed3"); return NULL;
			}
			result=EVP_CIPHER_CTX_set_key_length(rc4ctx, Length_bytes);
			if(result!=1){
				Log(LOG_ERROR, "EVP_CIPHER_CTX_set_key_length failed"); return NULL;
				return NULL;
			}
			result=EVP_EncryptInit_ex2(rc4ctx, NULL, &fek_i[0], NULL, NULL);
			if(result!=1){
				Log(LOG_ERROR, "EVP_EncryptInit failed4"); return NULL;
			}
			// Log(LOG_DEBUG, "Key length: %d", EVP_CIPHER_CTX_get_key_length(rc4ctx));
			result=EVP_EncryptUpdate(rc4ctx, &encrypted_rc4[0], &rc4count, &unencrypted[0], 16);
			if(result!=1){
				Log(LOG_ERROR, "EVP_EncryptUpdate failed"); return NULL;
			}
			rc4finalCount;
			result=EVP_EncryptFinal_ex(rc4ctx, &(encrypted_rc4[rc4count]), &rc4finalCount);
			if(result!=1){
				Log(LOG_ERROR, "EVP_EncryptFinal failed"); return NULL;
			}
			rc4count+=rc4finalCount;
		}
		PDFStr* tU=new PDFStr(rc4count);
		for(i=0; i<rc4count; i++){
			tU->decrData[i]=encrypted_rc4[i];
		}
		tU->decrData[rc4count]='\0';
		return tU;
	}
	Log(LOG_ERROR, "%d is invalid as R", R); return NULL;
}

PDFStr* Encryption::trialU6(PDFStr* pwd){
	if(error){
		return NULL;
	}
	// saslprep
	int result;
	char* in=new char[pwd->decrDataLen+1];
	int i;
	for(i=0; i<pwd->decrDataLen; i++){
		in[i]=pwd->decrData[i];
	}
	in[pwd->decrDataLen]='\0';
	char* out;
	int stringpreprc;
	result=gsasl_saslprep(&in[0], GSASL_ALLOW_UNASSIGNED, &out, &stringpreprc);
	if(result!=GSASL_OK){
		Log(LOG_ERROR, "GSASL SASLprep error(%d): %s", stringpreprc, gsasl_strerror(stringpreprc)); return NULL;
	}

	// concatenate pwd with User Validation Salt
	int outLen=strlen(out);
	PDFStr* input=new PDFStr(outLen+8);
	for(i=0; i<outLen; i++){
		input->decrData[i]=out[i];
	}
	for(i=0; i<8; i++){
		input->decrData[outLen+i]=U->decrData[32+i];
	}
	if(LOG_LEVEL>=LOG_DEBUG){
		Log(LOG_DEBUG, "Input + User Validation Salt:");
		DumpPDFStr(input);
	}	
	return hash6(input, false, 8);
}

PDFStr* Encryption::hash6(PDFStr* input, bool owner, int saltLength){
	if(error){
		return NULL;
	}
	// K can be the hash of 32, 48, and 64 bytes
	PDFStr* K=new PDFStr(64);
	
	int result;
	unsigned int count;
	// K preparation (SHA-256)
	EVP_MD_CTX* ctx=EVP_MD_CTX_new();
	result=EVP_DigestInit_ex(ctx, EVP_sha256(), NULL);
	if(result!=1){
		Log(LOG_ERROR, "EVP_DigestInit failed"); return NULL;
	}
	result=EVP_DigestUpdate(ctx, &(input->decrData[0]), input->decrDataLen);
	if(result!=1){
		Log(LOG_ERROR, "EVP_DigestUpdate failed"); return NULL;
	}
	result=EVP_DigestFinal_ex(ctx, &(K->decrData[0]), &count);
	if(result!=1){
		Log(LOG_ERROR, "EVP_DigestFinal failed"); return NULL;
	}
	K->decrDataLen=32;

	int maxK1Length=input->decrDataLen-saltLength+64;
	if(owner){
		maxK1Length+=48;
	}
	PDFStr* K1=new PDFStr(maxK1Length*64);
	PDFStr* E=new PDFStr(maxK1Length*64);
	int round=0;
	bool okFlag=false;
	int i,j, K1Length;
	EVP_CIPHER_CTX *aesctx=EVP_CIPHER_CTX_new();
	while(round<64 || okFlag==false){
		K1Length=input->decrDataLen-saltLength+K->decrDataLen;
		if(owner){
			K1Length+=48;
		}
		K1->decrDataLen=K1Length*64;
		for(i=0; i<64; i++){
			for(j=0; j<input->decrDataLen-saltLength; j++){
				K1->decrData[i*K1Length+j]=input->decrData[j];
			}
			for(j=0; j<K->decrDataLen; j++){
				K1->decrData[i*K1Length+input->decrDataLen-saltLength+j]=K->decrData[j];
			}
			if(owner){
				for(j=0; j<48; j++){
					K1->decrData[i*K1Length+input->decrDataLen-saltLength+K->decrDataLen+j]=U->decrData[j];
				}
			}
		}
		// AES, CBC mode encrypt
		int aescount;
		result=EVP_CIPHER_CTX_reset(aesctx);
		result=EVP_EncryptInit_ex2(aesctx, EVP_aes_128_cbc(), &(K->decrData[0]), &(K->decrData[16]), NULL);
		if(result!=1){
			Log(LOG_ERROR, "EVP_EncryptInit failed"); return NULL;
		}
		result=EVP_CIPHER_CTX_set_padding(aesctx, 0);
		if(result!=1){
			Log(LOG_ERROR, "EVP_CIPHER_CTX_set_padding failed"); return NULL;
		}
		result=EVP_EncryptUpdate(aesctx, &(E->decrData[0]), &aescount, &(K1->decrData[0]), K1->decrDataLen);
		if(result!=1){
			Log(LOG_ERROR, "EVP_EncryptUpdate failed"); return NULL;
		}
		int aesfinalCount;
		result=EVP_EncryptFinal_ex(aesctx, &(E->decrData[aescount]), &aesfinalCount);
		if(result!=1){
			Log(LOG_ERROR, "EVP_EncryptFinal failed"); return NULL;
		}
		aescount+=aesfinalCount;
		E->decrDataLen=aescount;
		
		int remainder=0;
		for(i=0; i<16; i++){
			remainder*=256;
			remainder+=(unsigned int)E->decrData[i];
			remainder%=3;
		}
	  
		result=EVP_MD_CTX_reset(ctx);
		if(remainder==0){
			result=EVP_DigestInit_ex(ctx, EVP_sha256(), NULL);
			K->decrDataLen=32;
		}else if(remainder==1){
			result=EVP_DigestInit_ex(ctx, EVP_sha384(), NULL);
			K->decrDataLen=48;
		}else if(remainder==2){
			result=EVP_DigestInit_ex(ctx, EVP_sha512(), NULL);
			K->decrDataLen=64;
		}else{
			Log(LOG_ERROR, "Modulo error"); return NULL;
		}
		if(result!=1){
			Log(LOG_ERROR, "EVP_DigestInit failed"); return NULL;
		}
		result=EVP_DigestUpdate(ctx, &(E->decrData[0]), E->decrDataLen);
		if(result!=1){
			Log(LOG_ERROR, "EVP_DigestUpdate failed"); return NULL;
		}
		result=EVP_DigestFinal_ex(ctx, &(K->decrData[0]), &count);
		if(result!=1){
			Log(LOG_ERROR, "EVP_DigestFinal failed"); return NULL;
		}
		round++;
		
		if((unsigned int)E->decrData[E->decrDataLen-1]>(round-32)){
			okFlag=false;
		}else{
			okFlag=true;
		}
	}
	K->decrDataLen=32;
	return K;
}

PDFStr* Encryption::trialO6(PDFStr* pwd){
	if(error){
		return NULL;
	}
	// saslprep
	int result;
	char* in=new char[pwd->decrDataLen+1];
	int i;
	for(i=0; i<pwd->decrDataLen; i++){
		in[i]=pwd->decrData[i];
	}
	in[pwd->decrDataLen]='\0';
	char* out;
	int stringpreprc;
	result=gsasl_saslprep(&in[0], GSASL_ALLOW_UNASSIGNED, &out, &stringpreprc);
	if(result!=GSASL_OK){
		Log(LOG_ERROR, "GSASL SASLprep error(%d): %s\n", stringpreprc, gsasl_strerror(stringpreprc)); return NULL;
	}
	
	// concatenate pwd with Owner Validation Salt and U (48 bytes)
	int outLen=strlen(out);
	PDFStr* input=new PDFStr(outLen+56);
	for(i=0; i<outLen; i++){
		input->decrData[i]=out[i];
	}
	for(i=0; i<8; i++){
		input->decrData[outLen+i]=O->decrData[32+i];
	}
	for(i=0; i<48; i++){
		input->decrData[outLen+8+i]=U->decrData[i];
	}
	if(LOG_LEVEL>=LOG_DEBUG){
		Log(LOG_DEBUG, "Input + Owner Validation Salt + U:");
		DumpPDFStr(input);
	}
	return hash6(input, true, 56);
}


PDFStr* Encryption::fileEncryptionKey(PDFStr* pwd){
	if(error){
		return NULL;
	}
	int i, j;
	if(R<=4){
		// MD5 (16 bytes) version
		int fromPwd=min(32, pwd->decrDataLen);
		int fromPad=32-fromPwd;
		unsigned char paddedPwd[32];
		for(i=0; i<fromPwd; i++){
			paddedPwd[i]=pwd->decrData[i];
		}
		for(i=0; i<fromPad; i++){
			paddedPwd[i+fromPwd]=PADDING[i];
		}
		if(LOG_LEVEL>=LOG_DEBUG){
			Log(LOG_DEBUG, "Padded password:");
			for(i=0; i<32; i++){
				printf("%02x ", paddedPwd[i]);
			}
			cout << endl;
		}

		unsigned char hashed_md5[16];
		int result;

		EVP_MD_CTX* ctx=EVP_MD_CTX_new();
		result=EVP_DigestInit_ex(ctx, EVP_md5(), NULL);
		if(result!=1){
			Log(LOG_ERROR, "EVP_DigestInit failed"); return NULL;
		}
		// Padded password
		result=EVP_DigestUpdate(ctx, &paddedPwd[0], 32);
		if(result!=1){
			Log(LOG_ERROR, "EVP_DigestUpdate failed in padded password"); return NULL;
		}
		// O
		result=EVP_DigestUpdate(ctx, &(O->decrData[0]), O->decrDataLen);
		if(result!=1){
			Log(LOG_ERROR, "EVP_DigestUpdate failed in O"); return NULL;
		}
		// P
		unsigned char p_c[4];
		p_c[0]=(P[ 7]?128:0)+(P[ 6]?64:0)+(P[ 5]?32:0)+(P[ 4]?16:0)+(P[ 3]?8:0)+(P[ 2]?4:0)+(P[ 1]?2:0)+(P[ 0]?1:0);
		p_c[1]=(P[15]?128:0)+(P[14]?64:0)+(P[13]?32:0)+(P[12]?16:0)+(P[11]?8:0)+(P[10]?4:0)+(P[ 9]?2:0)+(P[ 8]?1:0);
		p_c[2]=(P[23]?128:0)+(P[22]?64:0)+(P[21]?32:0)+(P[20]?16:0)+(P[19]?8:0)+(P[18]?4:0)+(P[17]?2:0)+(P[16]?1:0);
		p_c[3]=(P[31]?128:0)+(P[30]?64:0)+(P[29]?32:0)+(P[28]?16:0)+(P[27]?8:0)+(P[26]?4:0)+(P[25]?2:0)+(P[24]?1:0);
		result=EVP_DigestUpdate(ctx, &p_c[0], 4);
		if(result!=1){
			Log(LOG_ERROR, "EVP_DigestUpdate failed in P"); return NULL;
		}
		// ID[0]
		result=EVP_DigestUpdate(ctx, &(IDs[0]->decrData[0]), IDs[0]->decrDataLen);
		if(result!=1){
			Log(LOG_ERROR, "EVP_DigestUpdate failed in ID[0]"); return NULL;
		}
		// 0xffffffff
		if(R>=4 && !encryptMeta){
			unsigned char meta[4]={255, 255, 255, 255};
			result=EVP_DigestUpdate(ctx, &meta[0], 4);
			if(result!=1){
				Log(LOG_ERROR, "EVP_DigestUpdate failed in metadata"); return NULL;
			}
		}
		// close the hash
		unsigned int count;
		result=EVP_DigestFinal_ex(ctx, &hashed_md5[0], &count);
		if(result!=1){
			Log(LOG_ERROR, "EVP_DigestFinal failed"); return NULL;
		}
		
		if(R>=3){
			unsigned char hash_input[Length_bytes];
			result=EVP_MD_CTX_reset(ctx);
			if(result!=1){
				Log(LOG_ERROR, "EVP_MD_CTX_reset failed"); return NULL;
			}
			result=EVP_DigestInit_ex(ctx, EVP_md5(), NULL);
			if(result!=1){
				Log(LOG_ERROR, "EVP_DigestInit failed"); return NULL;
			}
			for(i=0; i<50; i++){
				for(j=0; j<Length_bytes; j++){
					hash_input[j]=hashed_md5[j];
				}
				result=EVP_MD_CTX_reset(ctx);
				if(result!=1){
					Log(LOG_ERROR, "EVP_MD_CTX_reset failed"); return NULL;
				}
				result=EVP_DigestInit_ex(ctx, EVP_md5(), NULL);
				if(result!=1){
					Log(LOG_ERROR, "EVP_DigestInit failed"); return NULL;
				}
				result=EVP_DigestUpdate(ctx, &hash_input[0], Length_bytes);
				if(result!=1){
					Log(LOG_ERROR, "EVP_DigestUpdate failed in loop"); return NULL;
				}
				result=EVP_DigestFinal_ex(ctx, &hashed_md5[0], &count);
				if(result!=1){
					Log(LOG_ERROR, "EVP_DigestFinal failed in loop"); return NULL;
				}
			}
		}
	  PDFStr* fek=new PDFStr(Length_bytes);
		for(i=0; i<Length_bytes; i++){
			fek->decrData[i]=hashed_md5[i];
		}
		fek->decrData[Length_bytes+1]='\0';
		return fek;
	}else if(R==6){
		Log(LOG_ERROR, "FEK for R=6 should be obtained from fileEncryptionKey6"); return NULL;
	}
	Log(LOG_ERROR, "%d is invalid as R", R); return NULL;
	return NULL;
}

PDFStr* Encryption::fileEncryptionKey6(PDFStr* pwd, bool owner){
	// saslprep
	int result;
	char* in=new char[pwd->decrDataLen+1];
	int i;
	for(i=0; i<pwd->decrDataLen; i++){
		in[i]=pwd->decrData[i];
	}
	in[pwd->decrDataLen]='\0';
	char* out;
	int stringpreprc;
	result=gsasl_saslprep(&in[0], GSASL_ALLOW_UNASSIGNED, &out, &stringpreprc);
	if(result!=GSASL_OK){
		Log(LOG_ERROR, "GSASL SASLprep error(%d): %s\n", stringpreprc, gsasl_strerror(stringpreprc)); return NULL;
	}

	// user (owner=false): (pwd)(User key salt) -> hash -> decrypt UE
	// owner (owner=true): (pwd)(Owner key salt)(U) -> hash -> decrypt OE
	int outLen=strlen(out);
	int inLength=outLen+8;
	int saltLength=8;
	if(owner){
		inLength+=48;
		saltLength+=48;
	}
	PDFStr* input=new PDFStr(inLength);
	for(i=0; i<outLen; i++){
		input->decrData[i]=out[i];
	}
	if(owner){
		for(i=0; i<8; i++){
			input->decrData[outLen+i]=O->decrData[40+i];
		}
		for(i=0; i<48; i++){
			input->decrData[outLen+8+i]=U->decrData[i];
		}
	}else{
		for(i=0; i<8; i++){
			input->decrData[outLen+i]=U->decrData[40+i];
		}
	}
	PDFStr* key=hash6(input, owner, saltLength);
	PDFStr* fek=new PDFStr(32);
	
	// decrypt UE/OE AES-256 no padding, zero vector as iv
	unsigned char iv[16];
	for(i=0; i<16; i++){
		iv[i]='\0';
	}
	int aescount;
	EVP_CIPHER_CTX *aesctx=EVP_CIPHER_CTX_new();
	result=EVP_DecryptInit_ex2(aesctx, EVP_aes_256_cbc(), &(key->decrData[0]), &iv[0], NULL);
	if(result!=1){
		Log(LOG_ERROR, "EVP_DecryptInit failed"); return NULL;
	}
	result=EVP_CIPHER_CTX_set_padding(aesctx, 0);
	if(result!=1){
		Log(LOG_ERROR, "EVP_CIPHER_CTX_set_padding failed"); return NULL;
	}
	if(owner){
		result=EVP_DecryptUpdate(aesctx, &(fek->decrData[0]), &aescount, &(OE->decrData[0]), OE->decrDataLen);
	}else{
		result=EVP_DecryptUpdate(aesctx, &(fek->decrData[0]), &aescount, &(UE->decrData[0]), UE->decrDataLen);
	}
	if(result!=1){
		Log(LOG_ERROR, "EVP_DecryptUpdate failed"); return NULL;
	}
	int aesfinalCount;
	result=EVP_DecryptFinal_ex(aesctx, &(fek->decrData[aescount]), &aesfinalCount);
	if(result!=1){
		Log(LOG_ERROR, "EVP_EncryptFinal failed"); return NULL;
	}
	aescount+=aesfinalCount;
	return fek;
}

PDFStr* Encryption::encryptFEK6(PDFStr* pwd, bool owner){
	// saslprep
	int result;
	char* in=new char[pwd->decrDataLen+1];
	int i;
	for(i=0; i<pwd->decrDataLen; i++){
		in[i]=pwd->decrData[i];
	}
	in[pwd->decrDataLen]='\0';
	char* out;
	int stringpreprc;
	result=gsasl_saslprep(&in[0], GSASL_ALLOW_UNASSIGNED, &out, &stringpreprc);
	if(result!=GSASL_OK){
		Log(LOG_ERROR, "GSASL SASLprep error(%d): %s\n", stringpreprc, gsasl_strerror(stringpreprc));
		return NULL;
	}

	// user (owner=false): (pwd)(User key salt) -> hash
	// owner (owner=true): (pwd)(Owner key salt)(U) -> hash
	int inLength=strlen(out)+8;
	int saltLength=8;
	if(owner){
		inLength+=48;
		saltLength+=48;
	}
	PDFStr* input=new PDFStr(inLength);
	for(i=0; i<strlen(out); i++){
		input->decrData[i]=out[i];
	}
	if(owner){
		for(i=0; i<8; i++){
			input->decrData[strlen(out)+i]=O->decrData[40+i];
		}
		for(i=0; i<48; i++){
			input->decrData[strlen(out)+8+i]=U->decrData[i];
		}
	}else{
		for(i=0; i<8; i++){
			input->decrData[strlen(out)+i]=U->decrData[40+i];
		}
	}
	PDFStr* key=hash6(input, owner, saltLength);
	PDFStr* encrypted_fek=new PDFStr(32);
	// encrypt FEK by AES-256 no padding, zero vector as iv
	unsigned char iv[16];
	for(i=0; i<16; i++){
		iv[i]='\0';
	}
	int aescount;
	EVP_CIPHER_CTX *aesctx=EVP_CIPHER_CTX_new();
	result=EVP_EncryptInit_ex2(aesctx, EVP_aes_256_cbc(), &(key->decrData[0]), &iv[0], NULL);
	if(result!=1){
		Log(LOG_ERROR, "EVP_EncryptInit failed");
		return NULL;
	}
	result=EVP_CIPHER_CTX_set_padding(aesctx, 0);
	if(result!=1){
		Log(LOG_ERROR, "EVP_CIPHER_CTX_set_padding failed");
		return NULL;
	}
	result=EVP_EncryptUpdate(aesctx, &(encrypted_fek->decrData[0]), &aescount, &(FEK->decrData[0]), FEK->decrDataLen);
	if(result!=1){
		Log(LOG_ERROR, "EVP_DecryptUpdate failed");
		return NULL;
	}
	int aesfinalCount;
	result=EVP_EncryptFinal_ex(aesctx, &(encrypted_fek->decrData[aescount]), &aesfinalCount);
	if(result!=1){
		Log(LOG_ERROR, "EVP_EncryptFinal failed");
		return NULL;
	}
	aescount+=aesfinalCount;
	return encrypted_fek;
}

PDFStr* Encryption::RC4EncryptionKey(PDFStr* pwd){
	if(error){
		return NULL;
	}
	int i, j;
	if(R<=4){
		// MD5 (16 bytes) version
		int fromPwd=min(32, pwd->decrDataLen);
		int fromPad=32-fromPwd;
		unsigned char paddedPwd[32];
		for(i=0; i<fromPwd; i++){
			paddedPwd[i]=pwd->decrData[i];
		}
		for(i=0; i<fromPad; i++){
			paddedPwd[i+fromPwd]=PADDING[i];
		}
		if(LOG_LEVEL>=LOG_DEBUG){
			Log(LOG_DEBUG, "Padded password:");
			for(i=0; i<32; i++){
				printf("%02x ", paddedPwd[i]);
			}
			cout << endl;
		}
		unsigned char hashed_md5[16];
		int result;

		EVP_MD_CTX* ctx=EVP_MD_CTX_new();
		result=EVP_DigestInit_ex(ctx, EVP_md5(), NULL);
		if(result!=1){
			Log(LOG_ERROR, "EVP_DigestInit failed"); return NULL;
		}
		// Padded password
		result=EVP_DigestUpdate(ctx, &paddedPwd[0], 32);
		if(result!=1){
			Log(LOG_ERROR, "EVP_DigestUpdate failed in padded password"); return NULL;
		}

		// close the hash
		unsigned int count;
		result=EVP_DigestFinal_ex(ctx, &hashed_md5[0], &count);
		if(result!=1){
			Log(LOG_ERROR, "EVP_DigestFinal failed"); return NULL;
		}

		if(R>=3){
			unsigned char hash_input[16];
			result=EVP_MD_CTX_reset(ctx);
			if(result!=1){
				Log(LOG_ERROR, "EVP_MD_CTX_reset failed"); return NULL;
			}
			result=EVP_DigestInit_ex(ctx, EVP_md5(), NULL);
			if(result!=1){
				Log(LOG_ERROR, "EVP_DigestInit failed"); return NULL;
			}
			for(i=0; i<50; i++){
				for(j=0; j<16; j++){
					hash_input[j]=hashed_md5[j];
				}
				result=EVP_MD_CTX_reset(ctx);
				if(result!=1){
					Log(LOG_ERROR, "EVP_MD_CTX_reset failed"); return NULL;
				}
				result=EVP_DigestInit_ex(ctx, EVP_md5(), NULL);
				if(result!=1){
					Log(LOG_ERROR, "EVP_DigestInit failed"); return NULL;
				}
				result=EVP_DigestUpdate(ctx, &hash_input[0], 16);
				if(result!=1){
					Log(LOG_ERROR, "EVP_DigestUpdate failed in loop"); return NULL;
				}
				result=EVP_DigestFinal_ex(ctx, &hashed_md5[0], &count);
				if(result!=1){
					Log(LOG_ERROR, "EVP_DigestFinal failed in loop");	return NULL;
				}
			}
		}
		PDFStr* fek=new PDFStr(Length_bytes);
		for(i=0; i<Length_bytes; i++){
			fek->decrData[i]=hashed_md5[i];
		}
		fek->decrData[Length_bytes+1]='\0';
		return fek;
	}else if(R==6){
		Log(LOG_ERROR, "R=6 does not use RC4"); return NULL;
	}
	Log(LOG_ERROR, "%d is invalid as R", R); return NULL;
}


PDFStr* Encryption::DecryptO(PDFStr* RC4fek){
	int i,j;
	if(R==2){
		// NEED TESTING !
		unsigned char unencrypted[32];
		unsigned char encrypted_rc4[32];
		for(i=0; i<32; i++){
			encrypted_rc4[i]=O->decrData[i];
		}
		int result;
		int rc4count;
		int rc4finalCount;
		EVP_CIPHER_CTX *rc4ctx=EVP_CIPHER_CTX_new();
		result=EVP_DecryptInit_ex2(rc4ctx, EVP_rc4(), NULL, NULL, NULL);
		if(result!=1){
			Log(LOG_ERROR, "EVP_DecryptInit failed"); return NULL;
		}
		result=EVP_CIPHER_CTX_set_key_length(rc4ctx, Length_bytes);
		if(result!=1){
			Log(LOG_ERROR, "EVP_CIPHER_CTX_set_key_length failed"); return NULL;
		}
		Log(LOG_DEBUG, "Key length: %d", EVP_CIPHER_CTX_get_key_length(rc4ctx));
		result=EVP_DecryptInit_ex2(rc4ctx, NULL, RC4fek->decrData, NULL, NULL);
		if(result!=1){
			Log(LOG_ERROR, "EVP_EncryptInit failed"); return NULL;
		}
		result=EVP_DecryptUpdate(rc4ctx, &unencrypted[0], &rc4count, &encrypted_rc4[0], 32);
		if(result!=1){
			Log(LOG_ERROR, "EVP_DecryptUpdate failed"); return NULL;
		}
		result=EVP_DecryptFinal_ex(rc4ctx, &(unencrypted[rc4count]), &rc4finalCount);
		if(result!=1){
			Log(LOG_ERROR, "EVP_DecryptFinal failed"); return NULL;
		}
		rc4count+=rc4finalCount;
		PDFStr* tUser=new PDFStr(rc4count);
		for(i=0; i<rc4count; i++){
			tUser->decrData[i]=unencrypted[i];
		}
		tUser->decrData[rc4count]='\0';
		return tUser;
	}else if(R==3 || R==4){
		unsigned char encrypted_rc4[32];
		unsigned char unencrypted[32];
		for(i=0; i<32; i++){
			unencrypted[i]=O->decrData[i];
		}
		int result;
		int rc4count;
		int rc4finalCount;
		EVP_CIPHER_CTX *rc4ctx=EVP_CIPHER_CTX_new();
		result=EVP_DecryptInit_ex2(rc4ctx, EVP_rc4(), NULL, NULL, NULL);
		result=EVP_CIPHER_CTX_set_key_length(rc4ctx, Length_bytes);
		if(result!=1){
			Log(LOG_ERROR, "EVP_CIPHER_CTX_set_key_length failed"); return NULL;
		}
		Log(LOG_DEBUG, "Key length: %d", EVP_CIPHER_CTX_get_key_length(rc4ctx));
		result=EVP_DecryptInit_ex2(rc4ctx, NULL, NULL, NULL, NULL);
		if(result!=1){
			Log(LOG_ERROR, "EVP_EncryptInit failed"); return NULL;
		}
		for(i=19; i>=0; i--){
			unsigned char fek_i[Length_bytes];
			for(j=0; j<32; j++){
				encrypted_rc4[j]=unencrypted[j];
			}
			for(j=0; j<Length_bytes; j++){
				fek_i[j]=(RC4fek->decrData[j])^((unsigned char)i);
			}
			result=EVP_CIPHER_CTX_reset(rc4ctx);
			if(result!=1){
				Log(LOG_ERROR, "EVP_CIPHER_CTX_reset failed"); return NULL;
			}
			result=EVP_DecryptInit_ex2(rc4ctx, EVP_rc4(), NULL, NULL, NULL);
			if(result!=1){
				Log(LOG_ERROR, "EVP_DecryptInit failed"); return NULL;
			}
			result=EVP_CIPHER_CTX_set_key_length(rc4ctx, Length_bytes);
			if(result!=1){
				Log(LOG_ERROR, "EVP_CIPHER_CTX_set_key_length failed"); return NULL;
			}
			result=EVP_DecryptInit_ex2(rc4ctx, NULL, &fek_i[0], NULL, NULL);
			if(result!=1){
				Log(LOG_ERROR, "EVP_DecryptInit failed"); return NULL;
			}
			result=EVP_DecryptUpdate(rc4ctx, &unencrypted[0], &rc4count, &encrypted_rc4[0], 32);
			if(result!=1){
				Log(LOG_ERROR, "EVP_DecryptUpdate failed"); return NULL;
			}
			result=EVP_DecryptFinal_ex(rc4ctx, &(unencrypted[rc4count]), &rc4finalCount);
			if(result!=1){
				Log(LOG_ERROR, "EVP_DecryptFinal failed"); return NULL;
			}
			rc4count+=rc4finalCount;
		}
		PDFStr* tUser=new PDFStr(rc4count);
		for(i=0; i<rc4count; i++){
			tUser->decrData[i]=unencrypted[i];
		}
		tUser->decrData[rc4count]='\0';
		return tUser;
	}
	Log(LOG_ERROR, "%d is invalid as R", R);
	return NULL;
}

void Encryption::EncryptO(unsigned char* paddedUserPwd, PDFStr* RC4fek){
	int i,j;
	if(R==2){
		unsigned char encrypted_rc4[32];
		unsigned char unencrypted[32];
		for(i=0; i<32; i++){
			unencrypted[i]=paddedUserPwd[i];
		}
		int result;
		int rc4count;
		int rc4finalCount;
		EVP_CIPHER_CTX *rc4ctx=EVP_CIPHER_CTX_new();
		result=EVP_EncryptInit_ex2(rc4ctx, EVP_rc4(), NULL, NULL, NULL);
		if(result!=1){
			Log(LOG_ERROR, "EVP_EncryptInit failed"); return;
		}
		result=EVP_CIPHER_CTX_set_key_length(rc4ctx, Length_bytes);
		if(result!=1){
			Log(LOG_ERROR, "EVP_CIPHER_CTX_set_key_length failed");	return;
		}
		Log(LOG_DEBUG, "Key length: %d", EVP_CIPHER_CTX_get_key_length(rc4ctx));
		result=EVP_EncryptInit_ex2(rc4ctx, NULL, RC4fek->decrData, NULL, NULL);
		if(result!=1){
			Log(LOG_ERROR, "EVP_EncryptInit failed"); return;
		}
		result=EVP_EncryptUpdate(rc4ctx, &encrypted_rc4[0], &rc4count, &unencrypted[0], 32);
		if(result!=1){
			Log(LOG_ERROR, "EVP_EncryptUpdate failed");
			return;
		}
		result=EVP_EncryptFinal_ex(rc4ctx, &(encrypted_rc4[rc4count]), &rc4finalCount);
		if(result!=1){
			Log(LOG_ERROR, "EVP_EncryptFinal failed"); return;
		}
		rc4count+=rc4finalCount;
	  O=new PDFStr(rc4count);
		for(i=0; i<rc4count; i++){
			O->decrData[i]=encrypted_rc4[i];
		}
		O->decrData[rc4count]='\0';
	}else if(R==3 || R==4){
		unsigned char encrypted_rc4[32];
		unsigned char unencrypted[32];
		for(i=0; i<32; i++){
			encrypted_rc4[i]=paddedUserPwd[i];
		}
		int result;
		int rc4count;
		int rc4finalCount;
		EVP_CIPHER_CTX *rc4ctx=EVP_CIPHER_CTX_new();
		result=EVP_EncryptInit_ex2(rc4ctx, EVP_rc4(), NULL, NULL, NULL);
		if(result!=1){
			Log(LOG_ERROR, "EVP_EncryptInit failed3");return;
		}
		for(i=0; i<=19; i++){
			unsigned char fek_i[Length_bytes];
			for(j=0; j<32; j++){
				unencrypted[j]=encrypted_rc4[j];
			}
			for(j=0; j<Length_bytes; j++){
				fek_i[j]=(RC4fek->decrData[j])^((unsigned char)i);
			}
			result=EVP_CIPHER_CTX_reset(rc4ctx);
			if(result!=1){
				Log(LOG_ERROR, "EVP_CIPHER_CTX_reset failed"); return;
			}
			result=EVP_EncryptInit_ex2(rc4ctx, EVP_rc4(), NULL, NULL, NULL);
			if(result!=1){
				Log(LOG_ERROR, "EVP_EncryptInit failed4"); return;
			}
			result=EVP_CIPHER_CTX_set_key_length(rc4ctx, Length_bytes);
			if(result!=1){
				Log(LOG_ERROR, "EVP_CIPHER_CTX_set_key_length failed");	return;
			}
			result=EVP_EncryptInit_ex2(rc4ctx, NULL, &fek_i[0], NULL, NULL);
			if(result!=1){
				Log(LOG_ERROR, "EVP_EncryptInit failed5"); return;
			}
			result=EVP_EncryptUpdate(rc4ctx, &encrypted_rc4[0], &rc4count, &unencrypted[0], 32);
			if(result!=1){
				Log(LOG_ERROR, "EVP_EncryptUpdate failed");
				return;
			}
			result=EVP_EncryptFinal_ex(rc4ctx, &(encrypted_rc4[rc4count]), &rc4finalCount);
			if(result!=1){
				Log(LOG_ERROR, "EVP_EncryptFinal failed"); return;
			}
			rc4count+=rc4finalCount;
		}
	  O=new PDFStr(rc4count);
		for(i=0; i<rc4count; i++){
			O->decrData[i]=encrypted_rc4[i];
		}
		O->decrData[rc4count]='\0';
	}
}

void Encryption::prepareIV(unsigned char* iv){
	srand((unsigned int)time(NULL));
	int i;
	for(i=0; i<16; i++){
		int r=rand();
		iv[i]=(unsigned char)r%256;
	}
}

int Encryption::GetV(){
	return V;
}


// V=1 5 bytes, R=2
// V=2 >5 bytes (set to 16 bytes), R=3
// V=4 16 bytes, R=4
// V=5 32 bytes, R=6
bool Encryption::SetV(int V_new){
	switch(V_new){
	case 1:
		V=1;
		R=2;
		Length=40;
		Length_bytes=5;
		break;
	case 2:
		V=2;
		R=3;
		Length=128;
		Length_bytes=16;
		break;
	case 4:
		V=4;
		R=4;
		Length=128;
		Length_bytes=16;
		break;
	case 5:
		V=5;
		R=6;
		Length=256;
		Length_bytes=32;
		break;
	default:
		return false;
	}
	return true;
}

bool Encryption::SetKeyLength(int length_new){
	if(V==2 && length_new%8==0 && length_new>=40 && length_new<=128){
		Length=length_new;
		Length_bytes=length_new/8;
		return true;
	}
	return false;
}


void Encryption::SetCFM(const char* CFM_new){
	// set the CFM of StdCF to the given value
	CF=new Dictionary(CF);
	int StdCFIndex=CF->Search("StdCF");
	if(StdCFIndex>=0){
		CF->Delete(StdCFIndex);
	}
	Dictionary* StdCF=new Dictionary();
	StdCF->Append("CFM", (unsigned char*)CFM_new, Type::Name);
	StdCF->Append("Length", &Length_bytes, Type::Int);
	CF->Append("StdCF", StdCF, Type::Dict);
	
	// set "StdCF" to StmF and StrF
	StmF=(unsigned char*)"StdCF";
	StrF=(unsigned char*)"StdCF";
}


void Encryption::SetPwd(Array* ID, PDFStr* userPwd, PDFStr* ownerPwd){
	// copy ID to IDs
	PDFStr* idValue;
	int i;
	int j;
	for(i=0; i<2; i++){
		if(ID->Read(i, (void**)&idValue, Type::String)){
			IDs[i]=new PDFStr(16);
			for(j=0; j<16; j++){
				IDs[i]->decrData[j]=idValue->decrData[j];
			}
		}
	}
	if(R<=4){
		// ownerPwd -> RC4fek
		PDFStr* RC4fek=RC4EncryptionKey(ownerPwd);

		// userPwd -> padded userPwd
		int fromPwd=min(32, userPwd->decrDataLen);
		int fromPad=32-fromPwd;
		unsigned char paddedUserPwd[32];
		for(i=0; i<fromPwd; i++){
			paddedUserPwd[i]=userPwd->decrData[i];
		}
		for(i=0; i<fromPad; i++){
			paddedUserPwd[i+fromPwd]=PADDING[i];
		}

		// RC4fek, paddedUserPwd -> O
		EncryptO(paddedUserPwd, RC4fek);
		if(LOG_LEVEL>=LOG_DEBUG){
			Log(LOG_DEBUG, "O:");
			DumpPDFStr(O);
		}
		
		// userPwd -> FEK
		FEK=fileEncryptionKey(userPwd);
		if(LOG_LEVEL>=LOG_DEBUG){
			Log(LOG_DEBUG, "FEK:");
			DumpPDFStr(FEK);
		}

		// FEK -> U_short (16) + arbitrary (16) -> U
		PDFStr* U_short=trialU(FEK);
		U=new PDFStr(32);
		for(i=0; i<U_short->decrDataLen; i++){
			U->decrData[i]=U_short->decrData[i];
		}
		for(i=U_short->decrDataLen; i<32; i++){
			U->decrData[i]=0;
		}
		if(LOG_LEVEL>=LOG_DEBUG){
			Log(LOG_DEBUG, "U: ");
			DumpPDFStr(U);
		}
	}else if(R==6){
		// prepare FEK (32 bytes, random)
		FEK=new PDFStr(32);
		srand((unsigned int)time(NULL));
		int i;
		for(i=0; i<32; i++){
			FEK->decrData[i]=(unsigned char)(rand()%256);
		}

		// prepare salt parts of U and O
		U=new PDFStr(48);
		O=new PDFStr(48);
		for(i=0; i<16; i++){
			U->decrData[32+i]=(unsigned char)(rand()%256);
			O->decrData[32+i]=(unsigned char)(rand()%256);
		}

		// U and UE are first calculated
		// because hash for O and OE uses U
		PDFStr* tU=trialU6(userPwd);
		for(i=0; i<tU->decrDataLen; i++){
			U->decrData[i]=tU->decrData[i];
		}

		UE=encryptFEK6(userPwd, false);

		if(LOG_LEVEL>=LOG_DEBUG){
			Log(LOG_DEBUG, "U:");
			DumpPDFStr(U);
			Log(LOG_DEBUG, "UE:");
			DumpPDFStr(UE);
		}

		// O and OE
		PDFStr* tO=trialO6(ownerPwd);
		for(i=0; i<tO->decrDataLen; i++){
			O->decrData[i]=tO->decrData[i];
		}
		OE=encryptFEK6(ownerPwd, true);
		
		if(LOG_LEVEL>=LOG_DEBUG){
			Log(LOG_DEBUG, "O:");
			DumpPDFStr(O);
			Log(LOG_DEBUG, "OE:");
			DumpPDFStr(OE);
		}

		// Perms
		int aescount;
		int result;
		EVP_CIPHER_CTX *aesctx=EVP_CIPHER_CTX_new();
		unsigned char iv[16];
		PDFStr* Perms_decoded=new PDFStr(16);
		for(i=0; i<16; i++){
			iv[i]='\0';
			Perms_decoded->decrData[i]='\0';
		}
		Perms=new PDFStr(16);
		// prepare Perms_decoded
		// 0-3: from P
		int j;
		for(i=0; i<4; i++){
			for(j=0; j<8; j++){
				if(P[i*8+j]){
					Perms_decoded->decrData[i]+=(1<<j);
				}
			}
		}
		// 4-7: FF
		for(i=4; i<8; i++){
			Perms_decoded->decrData[i]=0xff;
		}
		// 8: encryptMeta
		Perms_decoded->decrData[8]=encryptMeta?'T':'F';
		// 9-11: adb
		Perms_decoded->decrData[9]='a';
		Perms_decoded->decrData[10]='d';
		Perms_decoded->decrData[11]='b';
		
		result=EVP_EncryptInit_ex2(aesctx, EVP_aes_256_cbc(), &(FEK->decrData[0]), &iv[0], NULL);
		if(result!=1){
			Log(LOG_ERROR, "EVP_EncryptInit failed");
			error=true;	return;
		}
		result=EVP_CIPHER_CTX_set_padding(aesctx, 0);
		if(result!=1){
			Log(LOG_ERROR, "EVP_CIPHER_CTX_set_padding failed");
			error=true; return;
		}
		result=EVP_EncryptUpdate(aesctx, &(Perms->decrData[0]), &aescount, &(Perms_decoded->decrData[0]), 16);
		if(result!=1){
			Log(LOG_ERROR, "EVP_EncryptUpdate failed");
		  error=true;	return;
		}
		int aesfinalCount;
		result=EVP_EncryptFinal_ex(aesctx, &(Perms->decrData[aescount]), &aesfinalCount);
		if(result!=1){
			Log(LOG_ERROR, "EVP_EncryptFinal failed");
			error=true;	return;
		}
		aescount+=aesfinalCount;

		if(LOG_LEVEL>=LOG_DEBUG){
			Log(LOG_DEBUG, "Perms (before encryption):");
			DumpPDFStr(Perms_decoded);
			Log(LOG_DEBUG, "Perms (encrypted)");
			DumpPDFStr(Perms);
		}
	}
	FEKObtained=true;
}

void Encryption::SetP(bool* P_new){
	bool Vge3=(V>=3);
	P[0]=false;
	P[1]=false;
	P[2]=P_new[0];
	P[3]=P_new[1];
	P[4]=P_new[2];
	P[5]=P_new[3];
	P[6]=true;
	P[7]=true;
	P[8]=Vge3 && P_new[4];
	P[9]=true;
	P[10]=Vge3 && P_new[5];
	P[11]=Vge3 && P_new[6];
	int i;
	for(i=12; i<32; i++){
		P[i]=Vge3;
	}
}

void Encryption::PrintP(){
	printf("Print the document            (bit 3)  is %s\n", P[2]?"enabled":"disabled");
	printf("Modify the contents           (bit 4)  is %s\n", P[3]?"enabled":"disabled");
	printf("Copy text and graphics        (bit 5)  is %s\n", P[4]?"enabled":"disabled");
	printf("Add or modify annotations     (bit 6)  is %s\n", P[5]?"enabled":"disabled");
	printf("Fill in form fields           (bit 9)  is %s\n", P[8]?"enabled":"disabled");
	printf("Assemble the document         (bit 11) is %s\n", P[10]?"enabled":"disabled");
	printf("Faithfully print the document (bit 12) is %s\n", P[11]?"enabled":"disabled");
}

Dictionary* Encryption::ExportDict(){
	Dictionary* ret=new Dictionary();
	O->isHexStr=true;
	U->isHexStr=true;
	if(R==6){
		OE->isHexStr=true;
		UE->isHexStr=true;
		Perms->isHexStr=true;
	}
	ret->Append("R", &R, Type::Int);
	ret->Append("O", O, Type::String);
	ret->Append("U", U, Type::String);
	if(R==6){
		ret->Append("OE", OE, Type::String);
		ret->Append("UE", UE, Type::String);
	}
	int* P_i=new int();
	*P_i=-1;
  int i;
	for(i=0; i<32; i++){
		if(P[i]==false){
			*P_i-=1<<i;
		}
	}
	ret->Append("P", P_i, Type::Int);
	if(R==6){
		ret->Append("Perms", Perms, Type::String);
	}
	ret->Append("EncryptMetaData", &encryptMeta, Type::Bool);
	ret->Append("Filter", (unsigned char*)"Standard", Type::Name);
	ret->Append("V", &V, Type::Int);
	ret->Append("Length", &Length, Type::Int);
	if(V>=4){
		ret->Append("CF", CF, Type::Dict);
		ret->Append("StmF", StmF, Type::Name);
		ret->Append("StrF", StrF, Type::Name);
	}
	return ret;
}
