SN = 683339521
TARGET= rpi3dev

.PHONY: build flash_sn flash monitor clean

build: nrf52840

nrf52840:
	west build -b nrf52840dk/nrf52840

bl654:
	west build -b bl654_usb

flash:
	west -v flash --openocd /usr/bin/openocd

debug:
	west debugserver

menuconfig:
	west build -t menuconfig

flash_sn:
	west -v flash -r nrfjprog --snr $(SN) --openocd /usr/bin/openocd

monitor:
	python3 -m serial.tools.miniterm --eol LF --raw /dev/ttyACM0 115200

rust:
	cargo build --release

rust-server:
	cargo run --example server --target=x86_64-unknown-linux-gnu

rust-server-deploy:
	cargo build --example server
	scp target/aarch64-unknown-linux-gnu/debug/examples/server $(TARGET):~

format:
	find src -iname *.c -o -iname *.h | xargs clang-format -i
	find include -iname *.h | xargs clang-format -i

clean:
	rm -rf build