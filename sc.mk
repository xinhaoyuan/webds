.PHONY: all rebuild

V:=@

all: build
	${V}cd build &&	make

build:
	${V}mkdir -p build && cd build && cmake ..

package: all
	${V}mkdir -p package/webds-local
	${V}cp -r html package/webds-local/
	${V}cp -r bin package/webds-local/
	${V}cp    build/main/main package/webds-local/
	${V}cp    webds package/ && chmod +x package/webds
	${V}cd build/plugins/ && find -iname "*.so" -exec cp '{}' ../../package/webds-local/ ';'
	@echo all files packaged into $(pwd)/package

rebuild:
	-${V}rm -rf build
	${V}mkdir build && cd build && cmake .. && make

clean: build
	-${V}rm -rf package build

