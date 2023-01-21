all: ReadPDF.o
# all: ReadPDF.o ExportPDF.o

lib_r= log.o PDFParser.o Objects.o Encryption.o
lib_e= log.o PDFParser.o Objects.o Encryption.o PDFExporter.o
option=-g -l ssl -l crypto -lz -lgsasl -static

ReadPDF.o: ReadPDF_main.o $(lib_r)
	$(CXX) ReadPDF_main.o $(lib_r) $(option) -o ReadPDF.o
	cp ReadPDF.o ../

# ExportPDF.o: ExportPDF_main.o $(lib_e)
# 	$(CXX) ExportPDF_main.o $(lib_e) $(option) -o ExportPDF.o
# 	cp ExportPDF.o ../

.cpp.o:
	$(CXX) -c -g -Wno-format-security $<

ReadPDF_main.o: log.hpp variables.hpp ReadPDF_main.hpp PDFParser.hpp
