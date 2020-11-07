.PHONY : build clean prepare

default :
	@echo "======================================="
	@echo "Please use 'make build' command to build it.."
	@echo "Please use 'make clean' command to clean all."
	@echo "======================================="

CC = cc

INCLUDES += -I../../../src -I/usr/local/include -I/usr/local/opt/nghttp2/include
LIBS += -L../ -L../../../ -L/usr/local/lib -L/usr/local/opt/nghttp2/lib
CFLAGS += -g0 -O3 -shared -fPIC
MICRO += -fno-omit-frame-pointer -Wno-implicit-fallthrough -Wall -Wextra -Wno-unused-parameter
DLL += -lcore -llua -lnghttp2

prepare:
# 	@git clone https://github.com/nghttp2/nghttp2.git -b v1.41.0
	@git clone https://gitee.com/CandyMi/nghttp2.git -b v1.41.0
	@mkdir build && cmake -D CMAKE_INSTALL_PREFIX=/usr/local/opt ENABLE_THREADS=OFF ENABLE_EXAMPLES=OFF .. && make && make install

build:
	@$(CC) -o lhpack.so lhpack.c $(INCLUDES) $(LIBS) $(CFLAGS) $(MICRO) $(DLL)
	@mv *.so ../../

clean:
	@rm -rf *.so main