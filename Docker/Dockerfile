FROM ubuntu:latest AS builder
WORKDIR /source/openssl
RUN apt-get -y update
RUN apt-get -y install wget perl build-essential zlib1g-dev
RUN wget https://www.openssl.org/source/openssl-3.0.7.tar.gz &&\
		tar xzf openssl-3.0.7.tar.gz
WORKDIR /source/openssl/openssl-3.0.7
RUN ./Configure no-shared -static && make && make install
# RUN ./Configure && make && make test && make install
WORKDIR /source/gsasl
RUN wget https://ftp.gnu.org/gnu/gsasl/gsasl-2.2.0.tar.gz &&\
		tar xzf gsasl-2.2.0.tar.gz
WORKDIR /source/gsasl/gsasl-2.2.0
RUN ./configure && make && make install
WORKDIR /source/cpp
COPY cpp ./
RUN make

FROM busybox
WORKDIR /source
COPY --from=builder /source/*.o ./
COPY sample ./
# ENTRYPOINT ["./readPDF.o", "HELLO.pdf"]