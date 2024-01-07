build: Dockerfile
	docker build -t hhiiro/processpdf:1.0 --progress=plain .

build-no-cache: Dockerfile
	docker image pull ubuntu:latest
	docker image pull busybox:latest
	docker build --no-cache -t hhiiro/processpdf:1.0 .

run:
	docker container run -it --name "ProcessPDF" -v ~/Docker_shared:/sample --rm hhiiro/processpdf:1.0

push:
	docker image push hhiiro/processpdf:1.0
