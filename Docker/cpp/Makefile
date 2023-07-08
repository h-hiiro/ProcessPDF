
all: ReadPDF.o ExportPDF.o AddSignature.o ModifyMetadata.o

lib_r= log.o PDFParser.o Objects.o Encryption.o
lib_e= log.o PDFParser.o Objects.o Encryption.o PDFExporter.o
lib_a= log.o PDFParser.o Objects.o Encryption.o PDFExporter.o
lib_m= log.o PDFParser.o Objects.o Encryption.o PDFExporter.o
option=-g -l ssl -l crypto -lz -lgsasl -static

ReadPDF.o: ReadPDF_main.o $(lib_r)
	$(CXX) ReadPDF_main.o $(lib_r) $(option) -o ReadPDF.o
	cp ReadPDF.o ../

ExportPDF.o: ExportPDF_main.o $(lib_e)
	$(CXX) ExportPDF_main.o $(lib_e) $(option) -o ExportPDF.o
	cp ExportPDF.o ../

AddSignature.o: AddSignature_main.o $(lib_a)
	$(CXX) AddSignature_main.o $(lib_a) $(option) -o AddSignature.o
	cp AddSignature.o ../

ModifyMetadata.o: ModifyMetadata_main.o $(lib_a)
	$(CXX) ModifyMetadata_main.o $(lib_a) $(option) -o ModifyMetadata.o
	cp ModifyMetadata.o ../

.cpp.o:
	$(CXX) -c -g -Wno-format-security $<

ReadPDF_main.o: log.hpp variables.hpp ReadPDF_main.hpp PDFParser.hpp
