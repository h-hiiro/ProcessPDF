// class Encryption

#ifndef OBJECTS_INCLUDED
#include "Objects.hpp"
#endif
#include <openssl/evp.h>
#include <openssl/provider.h>
#include <openssl/err.h>

#define PADDING "\x28\xBF\x4E\x5E\x4E\x75\x8A\x41\x64\x00\x4E\x56\xFF\xFA\x01\x08\x2E\x2E\x00\xB6\xD0\x68\x3E\x80\x2F\x0C\xA9\xFE\x64\x53\x69\x7A"

class Encryption{
private:
	Dictionary* encryptDict;
	bool error;
	int V;                // Version of the encryption algorithm
	int Length;           // Length of the file encryption key (bits)
	int Length_bytes;     // Length of the FEK (bytes)
	bool CFexist;         // Whether CF dictionary exists or not
	Dictionary* CF;       // CF (crypt filter) dictionary
	unsigned char* StmF;  // Default filter name for Stream
	unsigned char* StrF;  // Default filter name for String
	unsigned char* Idn;   // Identity filter name "Identity"
	int R;                // Security handler revision
	PDFStr* O;PDFStr* OE; // Owner-related byte strings
	PDFStr* U;PDFStr* UE; // User-related byte strings
	bool P[32];           // Permission flags
	PDFStr* Perms;        // A byte string specifying permission
	PDFStr* FEK;          // File encryption key
	bool FEKObtained;     // Whether the FEK is obtained (= user or owner password is given)
	bool encryptMeta;     // Whether the metadata is encrypted
	PDFStr* IDs[2];
	PDFStr* fileEncryptionKey(PDFStr* pwd);
	PDFStr* fileEncryptionKey6(PDFStr* pwd, bool owner);
	PDFStr* encryptFEK6(PDFStr* pwd, bool owner);
	PDFStr* trialU(PDFStr* fek);
	PDFStr* trialU6(PDFStr* pwd);
	PDFStr* trialO6(PDFStr* pwd);
	PDFStr* hash6(PDFStr* input, bool owner, int saltLength);
	PDFStr* RC4EncryptionKey(PDFStr* pwd);
	PDFStr* DecryptO(PDFStr* RC4fek);
	void EncryptO(unsigned char* paddedUserPwd, PDFStr* RC4fek);
	bool execDecryption(unsigned char** encrypted, int* elength, unsigned char** decrypted, int* length, unsigned char* CFM, int objNumber, int genNumber);
	bool execEncryption(unsigned char** encrypted, int* elength, unsigned char** decrypted, int* length, unsigned char* CFM, int objNumber, int genNumber);
	void prepareIV(unsigned char* iv);
	bool getPerms6();
public:
	Encryption(Dictionary* encrypt, Array* ID);
	Encryption();
	Encryption(Encryption* original);
	bool AuthUser(PDFStr* pwd);
	bool AuthOwner(PDFStr* pwd);
	bool DecryptStream(Stream* stm);
	bool DecryptString(PDFStr* str, int objNumber, int genNumber);
	bool EncryptStream(Stream* stm);
	bool EncryptString(PDFStr* str, int objNumber, int genNumber);
	int GetV();
	bool SetV(int V_new);
	bool SetKeyLength(int length_new);
	void SetCFM(const char* CFM_new);
	void SetPwd(Array* ID, PDFStr* userPwd, PDFStr* ownerPwd);
	void SetP(bool* P_new);
	void PrintP();
	Dictionary* ExportDict();
	bool IsAuthenticated();
};
