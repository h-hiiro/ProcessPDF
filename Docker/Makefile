build: Dockerfile
	docker build -t processpdf:1.0 --progress=plain .

build-no-cache: Dockerfile
	docker build --no-cache -t processpdf:1.0 .

run:
	docker container run -it --name "ProcessPDF" --rm processpdf:1.0
