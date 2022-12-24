
all: ReadPDF.o

lib_r= log.o PDFParser.o Objects.o
option=-g -l ssl -l crypto -lz -lgsasl -static

ReadPDF.o: ReadPDF_main.o $(lib_r)
	$(CXX) ReadPDF_main.o $(lib_r) $(option) -o ReadPDF.o
	cp ReadPDF.o ../

.cpp.o:
	$(CXX) -c -g -Wno-format-security $<

ReadPDF_main.o: log.hpp variables.hpp ReadPDF_main.hpp PDFParser.hpp
