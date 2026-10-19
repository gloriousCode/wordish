.PHONY: c build test lint misc_checks clean config-emery config-gabbro

c: build

build:
	pebble build

test:
	mkdir -p build
	$(CC) -std=c99 -Wall -Wextra -Werror -Isrc \
		tests/text_layout_test.c \
		src/text_layout.c \
		src/text_lines.c \
		src/num2words.c \
		src/strings-ca.c \
		src/strings-de.c \
		src/strings-en_GB.c \
		src/strings-en_US.c \
		src/strings-es.c \
		src/strings-fr.c \
		src/strings-no.c \
		src/strings-sv.c \
		-o build/text_layout_test
	./build/text_layout_test
	$(CC) -std=c99 -Wall -Wextra -Werror -Wno-unused-parameter -Itests -Isrc \
		tests/watchface_runtime_test.c src/text_layout.c src/text_lines.c \
		src/num2words.c src/strings-*.c -o build/watchface_runtime_test
	./build/watchface_runtime_test
	$(CC) -std=c99 -Wall -Wextra -Werror -Wno-unused-parameter -DWORDISH_GENTLE_GESTURE -Itests -Isrc \
		tests/watchface_runtime_test.c src/text_layout.c src/text_lines.c \
		src/num2words.c src/strings-*.c -o build/watchface_gentle_runtime_test
	./build/watchface_gentle_runtime_test
	$(CC) -std=c99 -Wall -Wextra -Werror -Isrc tests/gentle_tap_test.c -o build/gentle_tap_test
	./build/gentle_tap_test
	$(CC) -std=c99 -Wall -Wextra -Werror -Itests/gentle_gesture_stubs -Isrc \
		tests/gentle_gesture_test.c -o build/gentle_gesture_test
	./build/gentle_gesture_test
	node --test tests/configuration_test.js
	$(CC) -std=c99 -Wall -Wextra -Werror -DPBL_PLATFORM_EMERY \
		-Itests/smooth_font_stubs -Isrc tests/smooth_font_renderer_test.c \
		-o build/smooth_font_renderer_test
	./build/smooth_font_renderer_test
	python3 tests/smooth_fonts_test.py
	python3 tests/capture_presets_test.py

lint: build
	node --check src/js/pebble-js-app.js

misc_checks: test

clean:
	rm -rf build

config-emery:
	pebble emu-app-config --emulator emery --file resources/configure-fuzzy-text-two.html

config-gabbro:
	pebble emu-app-config --emulator gabbro --file resources/configure-fuzzy-text-two.html

l: c
	deploypebble.sh load build/fuzzy-text-two.pbw
d: c
	deploypebble.sh reinstall build/fuzzy-text-two.pbw
