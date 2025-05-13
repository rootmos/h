ROOT=$(readlink -f "$K_DIR/../..")

export NODEJS_PREFIX="$K_DIR/nodejs-shared/usr"
export LD_LIBRARY_PATH="$NODEJS_PREFIX/lib:$LD_LIBRARY_PATH"

shell() {
    $SHELL
}


export DORKER_DOCKERFILE=ubuntu.dockerfile

build() {
    dorker build
}

run() {
    dorker run -i -m type=bind,src="$ROOT",dst=/workdir
}
