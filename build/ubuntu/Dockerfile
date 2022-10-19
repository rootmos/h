FROM ubuntu:22.04

RUN apt-get update
RUN apt-get install -y dh-make bzr-builddeb
RUN apt-get install -y sudo

WORKDIR /workdir
ENTRYPOINT [ "./run" ]
