# Run "./Build.sh" if you wan to compile this library and install it in ~/opae_nlb_hls
#!/bin/bash

bash_main() {
	set -e
	cd "$(dirname "$0")"/build

	cmake -DCMAKE_BUILD_TYPE="Debug" ..
	cmake --build .
	sudo cmake --install . --prefix "/home/vmerola/opae_nlb_hls/"
}

bash_main "@"
