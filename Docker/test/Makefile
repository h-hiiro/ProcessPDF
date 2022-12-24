
all: readPDF.o exportPDF.o changePerms.o verifyPDFSign.o addSignature.o

lib_r=PDFParser.o PDFVersion.o Objects.o Encryption.o
lib_e=PDFParser.o PDFVersion.o Objects.o Encryption.o PDFExporter.o
lib_c=PDFParser.o PDFVersion.o Objects.o Encryption.o PDFExporter.o
lib_v=PDFParser.o PDFVersion.o Objects.o Encryption.o
lib_a=PDFParser.o PDFVersion.o Objects.o Encryption.o PDFExporter.o

# header files: in /usr/include
# lib files: in /lib/x86_64-linux-gnu
# pkg-config --libs gsasl --static

# option=-g -l ssl -l crypto -lz /usr/local/lib/libgsasl.a -static
option=-g -l ssl -l crypto -lz -lgsasl -static


readPDF.o: readPDF_main.o $(lib_r)
	$(CXX) readPDF_main.o $(lib_r) $(option) -o readPDF.o 

exportPDF.o: exportPDF_main.o $(lib_e)
	$(CXX) exportPDF_main.o $(lib_e) $(option) -o exportPDF.o

changePerms.o: changePerms_main.o $(lib_c)
	$(CXX) changePerms_main.o $(lib_c) $(option) -o changePerms.o

verifyPDFSign.o: verifyPDFSign_main.o $(lib_v)
	$(CXX) verifyPDFSign_main.o $(lib_v) $(option) -o verifyPDFSign.o

addSignature.o: addSignature_main.o $(lib_a)
	$(CXX) addSignature_main.o $(lib_a) $(option) -o addSignature.o

.cpp.o:
	$(CXX) -c -g $<

exportPDF_main.o: PDFParser.hpp PDFVersion.hpp Objects.hpp PDFExporter.hpp
readPDF_main.o: PDFParser.hpp PDFVersion.hpp Objects.hpp
changePerms_main.o: PDFParser.hpp PDFVersion.hpp Objects.hpp PDFExporter.hpp
PDFParser.o: PDFParser.hpp PDFVersion.hpp Objects.hpp Encryption.hpp
PDFVersion.o: PDFVersion.hpp
Objects.o: Objects.hpp
Encryption.o: Encryption.hpp Objects.hpp
PDFExporter.o: PDFExporter.hpp PDFParser.hpp PDFVersion.hpp Objects.hpp
