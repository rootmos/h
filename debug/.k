export DORKER_DOCKERFILE=ubuntu.dockerfile

build() {
    dorker build
}

run() {
    dorker run -i -m type=bind,src="$K_DIR/..",dst=/workdir
}
