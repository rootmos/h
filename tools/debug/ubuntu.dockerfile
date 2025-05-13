FROM ubuntu:24.04

RUN apt-get update
RUN apt-get install -y --no-install-recommends git sudo gettext-base ca-certificates strace

RUN git clone -b ubuntu2404 https://github.com/rootmos/h
WORKDIR /h

ENV UNPRIVILEGED=ubuntu

RUN chown -R $UNPRIVILEGED:$UNPRIVILEGED .
RUN echo "$UNPRIVILEGED ALL = NOPASSWD: $(which apt-get)" | tee -a /etc/sudoers

#RUN sudo -u $UNPRIVILEGED --preserve-env=WORKDIR,TEST_OUTPUT_DIR,TIMEOUT,TRACE env SUDO=sudo build/ubuntu/mk
