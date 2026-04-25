SN = 683339521
RUNNER = jlink
TARGET= rpi3dev

# Signed app binary produced by sysbuild (in slot-0 format)
SIGNED_BIN = build/app/zephyr/zephyr.signed.bin

# Serial port used by MCUboot CDC-ACM during DFU
DFU_PORT ?= /dev/ttyACM0

.PHONY: build flash_sn flash monitor clean \
        build-all flash-all flash-app dfu-upload

# ---------------------------------------------------------------------------
# Plain app build (no MCUboot, app starts at 0x00000)
# ---------------------------------------------------------------------------
build: nrf52840

nrf52840:
	west build -b nrf52840dk/nrf52840

bl654:
	west build -b bl654_usb

flash:
	west -v flash --runner=$(RUNNER)

debug:
	west debugserver

menuconfig:
	west build -t menuconfig

flash_sn:
	west -v flash -r nrfjprog --snr $(SN) --runner=$(RUNNER)

# ---------------------------------------------------------------------------
# Sysbuild: MCUboot + app combined
# ---------------------------------------------------------------------------

# Build MCUboot + app together
build-all:
	west build -b nrf52840dk/nrf52840 --sysbuild

# Flash MCUboot + app (full initial programming)
flash-all: build-all
	west flash

# Flash only the app image (MCUboot already on device)
flash-app:
	west build -b nrf52840dk/nrf52840 --sysbuild --domain app
	west flash --domain app

# Upload a new firmware image while the device is in DFU (serial recovery) mode.
# Trigger DFU from the running app first (call dfu_enter_bootloader()), then run:
#   make dfu-upload
dfu-upload:
	mcumgr --conntype serial --connstring "$(DFU_PORT),baud=115200" \
	    image upload $(SIGNED_BIN)
	@echo "Upload done. MCUboot will swap images on next reboot."

# ---------------------------------------------------------------------------
# Misc
# ---------------------------------------------------------------------------
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

rust:
	cargo build --target=