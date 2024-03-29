# ProcessPDF
C++ program package to read and edit a PDF file

The package is provided as a Docker container.
Necessary library packages are automatically installed when ```docker build``` is performed.

## How to compile
```
cd Docker
docker build -t (image name) .
```
```Makefile``` in the Docker directory is for the package development.

## How to use the compiled application
The Docker image generated by the above process includes the following 5 executables in the ```/source/``` directory.

- **ReadPDF.o** Read a PDF file
- **ExportPDF.o** Export a PDF file
- **ReadFormData.o** Read form data in a PDF file
- **AddSignature.o** Add a signature in a PDF file
- **ModifyMetaData.o** Modify Metadata in a PDF file

The optional arguments applied when you execute an application are listed below.
You can check them by executing it with the ```-h``` option.

ExportPDF.o and ReadFormData.o can be tested at [https://rightsofar.info/pdftools.html](https://rightsofar.info/pdftools.html)

### ReadPDF.o: Read a PDF file
| Labels | Argument | Description |
| --- | --- | --- |
|-h, --help   | (no argument) | print the readme and exit |
|-i, --in     | filePath      | input PDF file path |
|-o, --owner  | password      | owner password |
|-u, --user   | password      | user password |
|-v, --verbose | verbosity    | verbosity of the output (see the bottom section for the details)|

### ExportPDF.o: Export a PDF file

| Labels | Argument | Description |
| --- | --- | --- |
|-h, --help    |(no argument) |print the readme and exit|
|-i, --in      |filePath      |input PDF file path|
|-o, --owner   |password      |original owner password|
|-u, --user    |password      |original user password|
|-v, --verbose |verbosity     |verbosity of the output (see the bottom section for the details)|
|-D, --Dest    |filePath      |destination path|
|-E, --Encrypt |(no argument) |encrypt the output|
|-L, --Length  |number        |key length in bits (default 128), meaningful only when V=2|
|-O, --Owner   |password      |new owner password|
|-P, --Perms   |perms label   |permissions ('1' means enabled, '0' means disabled, see below for the details)|
|-U, --User    |password      |new user password|
|-V, --Version |number       |encryption version (1, 2, 4, or 5, see below for the details)|

#### Permissions (-P)
The permissions label consists of six numbers.

- 0: print the document (bit 3)
- 1: modify the contents (bit 4)
- 2: copy text and graphics (bit 5)
- 3: add or modify annotations (bit 6)
- 4: fill in form fields (bit 9)
- 5: assemble the document (bit 11)
- 6: faithfully print the document (bit 12)

For example, 101011 means modifying the contents and adding or modifying annotations are prohibited.

#### Version (-V)
- 1: RC4, key length=40 bits     (PDF 1.1-1.7)
- 2: RC4, key length=40-128 bits (PDF 1.4-1.7)
- 4: AES, key length=128 bits    (PDF 1.5-1.7) (PDF 1.5 uses RC4)
- 5: AES, key length=256 bits    (PDF 2.0)

#### Rules for capitalized arguemnts for the output encryption
+ When the original is encrypted:
  You specify the passwords by the -O and -U arguments.
  The owner password must be specified, while the user password can be blank.
  You may specify new permissions by -P and encryption version by -V.
  If you don't use these arguments, the original values are inherted.
+ When the original is unencrypted:
  You specify the passwords by the -O and -U arguments.
  The owner password must be specified, while the user password can be blank.
  You may specify the permissions by -P and the encryption version by -V.
  The default value for the permission is '1111111', permitting all acts.
  The default value for the version is determined from the PDF file version.

### ReadFormData.o: Read form data in a PDF file
The form data are exported to a CSV file.

| Labels | Argument | Description |
| --- | --- | --- |
|-h, --help   | (no argument) | print the readme and exit |
|-i, --in     | filePath      | input PDF file path |
|-o, --owner  | password      | owner password |
|-u, --user   | password      | user password |
|-v, --verbose | verbosity    | verbosity of the output (see the bottom section for the details)|
|-D, --Dest    |filePath      |destination path|

### AddSignature.o: Add a signature in a PDF file

| Labels | Argument | Description |
| --- | --- | --- |
|-h, --help   |(no argument)| print the readme and exit|
|-i, --in     | filePath     | input PDF file path|
|-o, --owner   |password      |original owner password|
|-u, --user    |password      |original user password|
|-v, --verbose |verbosity     |verbosity of the output (see the bottom section for the details)|
|-C, --Cert    |filePath      |certificate PEM file(s)|
|-D, --Dest    |filePath      |destination path|
|-N, --Number  |pageNum       |page number where the sign is added (default 1)|
|-P, --Perm    |permission    |permission label (see below for the details)|
|-R, --Private |filePath      |private key PEM file|
|-S, --Signer  |name          |signer name|
|-U, --Public  |filePath      |public key PEM file|

The -C option can appear more than once like ```-C file1 -C file2```.

#### Permission label (-P)
- 1: No change is allowed (default)
- 2: Filling forms is allowed
- 3: Filling forms and mofidying annotations are allowed

### ModifyMetaData.o: Modify Metadata in a PDF file
Currently, what the executable can do is only to remove all the metadata.
It cannot modify them.

| Labels | Argument | Description |
| --- | --- | --- |
|-h, --help    |(no argument) |print the readme and exit|
|-i, --in      |filePath      |input PDF file path|
|-o, --owner   |password      |owner password|
|-u, --user    |password      |user password|
|-v, --verbose |verbosity     |verbosity of the output (see the bottom section for the details)|
|-D, --Dest    |filePath      |destination path|
|-R, --Remove  |(no argument) |remove all metadata|

### Verbosity (-v: common to all the executables)
- 0: Only error(s)\n\
- 1: Error(s) and warnings\n\
- 2: Error(s), warnings, and information\n\
- 3: Error(s), warnings, information, and debug logs

## Technologies used in the package
- OpenSSL (version 3.0.7) [Website](https://www.openssl.org/) [GitHub Repository](https://github.com/openssl/openssl)
- gsasl (version 2.2.0) [Website](https://www.gnu.org/software/gsasl/) [GitHub Repository](https://gitlab.com/gsasl/gsasl)
- Ubuntu (c++ compiler, zlib, perl) [Docker Image](https://hub.docker.com/_/ubuntu)
- busybox [Docker Image](https://hub.docker.com/_/busybox)

