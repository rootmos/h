FROM archlinux:latest

RUN pacman -Suy --noconfirm base-devel wget git


RUN pacman -S --noconfirm github-cli
RUN gh --repo rootmos/nodejs-shared release download --pattern "nodejs-shared*x86_64*"
RUN pacman --noconfirm -U nodejs-shared*x86_64*

RUN git clone https://github.com/rootmos/h
WORKDIR /h

ENV UNPRIVILEGED=arch

RUN useradd -m $UNPRIVILEGED
RUN chown -R $UNPRIVILEGED:$UNPRIVILEGED .
RUN tee -a /etc/sudoers.d/$UNPRIVILEGED <<< "$UNPRIVILEGED ALL = NOPASSWD: $(which pacman)"

RUN sudo -u $UNPRIVILEGED build/archlinux/mk

WORKDIR /h/build/archlinux
#RUN sudo -u $UNPRIVILEGED makepkg -si --noconfirm
