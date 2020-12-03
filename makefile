.PHONY : build clean prepare

default :
	@echo "======================================="
	@echo "Please use 'make build' command to build it.."
	@echo "Please use 'make clean' command to clean all."
	@echo "======================================="

CC = cc
RM = rm
MV = mv

INCLUDES += -I../../../src -I/usr/local/include -I/usr/local/opt/nghttp2/include
LIBS += -L../ -L../../../ -L/usr/local/lib -L/usr/local/opt/nghttp2/lib
CFLAGS += -g0 -O3 -shared -fPIC
MICRO += -fno-omit-frame-pointer -Wno-implicit-fallthrough -Wall -Wextra -Wno-unused-parameter
DLL += -lcore -lnghttp2

prepare:
#   @git clone https://github.com/nghttp2/nghttp2.git -b v1.41.0 && mkdir -p nghttp2/build
	@git clone https://gitee.com/CandyMi/nghttp2.git -b v1.41.0 && mkdir -p nghttp2/build
	@cd nghttp2/build && cmake -D CMAKE_INSTALL_PREFIX=/usr/local -D ENABLE_THREADS=OFF -D ENABLE_EXAMPLES=OFF -D ENABLE_DEBUG=OFF .. && make && make install

build:
	@$(CC) -o lhpack.so lhpack.c $(INCLUDES) $(LIBS) $(CFLAGS) $(MICRO) $(DLL)
	@$(MV) *.so ../../

clean:
	@$(RM) -rf main *.so
