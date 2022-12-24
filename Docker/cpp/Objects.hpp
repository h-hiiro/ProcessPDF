// classes of objects

#include <vector>
#include <string>
#include <cstddef>

#define OBJECTS_INCLUDED

using namespace std;

class Type{
public:
	static const int Bool    =0;
	static const int Int     =1;
	static const int Real    =2;
	static const int String  =3;
	static const int Name    =4;
	static const int Array   =5;
	static const int Dict    =6;
	static const int Stream  =7;
	static const int Null    =8;
	static const int Indirect=9;
};

class Delimiter{
public:
	static const int LP  =0;  // (
	static const int RP  =1;  // )
	static const int LT  =2;  // <
	static const int GT  =3;  // >
	static const int LSB =4;  // [
	static const int RSB =5;  // ]
	static const int LCB =6;  // {
	static const int RCB =7;  // }
	static const int SOL =8;  // /
	static const int PER =9;  // %
	static const int LLT =10; // <<
	static const int GGT =11; // >>
};

// Dictionary object
class Dictionary{
private:
	vector<unsigned char*> keys;
	vector<void*> values;
	vector<int> types;
public:
	Dictionary();
	Dictionary(Dictionary* original);
	void Append(unsigned char* key, void* value, int type);
	void Print(int indent);
	void Print();
	int Search(unsigned char* key);
	int Search(const char* key);
	bool Read(unsigned char* key, void** value, int* type);
	bool Read(const char* key, void** value, int* type);
	bool Read(unsigned char* key, void** value, int type);
	bool Read(const char* key, void** value, int type);
	bool Read(int index, unsigned char** key, void** value, int* type);
	void Merge(Dictionary dict2);
	int getSize();
	void Delete(int index);
	bool Update(unsigned char* key, void* value, int type);
};

// Array object
class Array{
private:
	vector<void*> values;
	vector<int> types;
public:
	Array();
	void Append(void* value, int type);
	void Print();
	void Print(int indent);
	int getSize();
	bool Read(int index, void** value, int* type);
	bool Read(int index, void** value, int type);
};

// Indirect object
class Indirect{
public:
	int objNumber; // negative objNumber means the Indirect is not yet initialized
	int genNumber;
	int position; // negative position means value includes the value
	bool used;
	int type;
	bool objStream;
	int objStreamNumber;
	int objStreamIndex;
	void* value;
	Indirect();
};

// String object
// decr and encr mean 'decrypted' and 'encrypted' respectively
class PDFStr{
public:
	int decrDataLen;
	int encrDataLen;
	bool isDecrypted;
	unsigned char* decrData;
	unsigned char* encrData;
	bool isHexStr;
	PDFStr();
	PDFStr(int dDL);
};

// Stream object
class Stream{
public:
	int objNumber;
	int genNumber;
	Dictionary StmDict;
	unsigned char* decoData; // decrypted, decoded
	unsigned char* encoData; // decrypted, encoded
	unsigned char* encrData; // encrypted, encoded
	int decoDataLen; // len(decoData)
	int encoDataLen; // len(encoData)
	int encrDataLen; // len(encrData)
	bool isDecrypted;
	Stream();
	bool Decode();
	bool Encode();
};

class Page{
public:
	Page();
	Indirect* Parent;
	Dictionary* PageDict;
	int objNumber;
};


char* printObj(void* value, int type);

int unsignedstrlen(unsigned char* a);
bool unsignedstrcmp(unsigned char* a, unsigned char* b);
bool unsignedstrcmp(unsigned char* a, const char* b);
void unsignedstrcpy(unsigned char* dest, unsigned char* data);

int decodeData(unsigned char* encoded, unsigned char* filter, Dictionary* parm, int encodedLength, unsigned char** decoded);
int encodeData(unsigned char* decoded, unsigned char* filter, Dictionary* parm, int decodedLength, unsigned char** encoded);

int PNGPredictor(unsigned char** pointer, int length, Dictionary* columns);

int byteSize(int n);

PDFStr* dateString();

class PDFVersion{
private:
	int major;
	int minor;
	bool error;
	bool valid;
public:
	PDFVersion();
	bool set(char* label);
	bool isValid();
	void print();
	char v[4];
};
