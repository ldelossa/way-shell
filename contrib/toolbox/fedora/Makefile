all: build create run

.PHONY:
build:
	podman build -t way-shell:latest .

.PHONY:
create:
	toolbox create --image way-shell:latest way-shell

.PHONY:
run:
	toolbox run --container way-shell way-shell


