.PHONY : build

default :
	@echo "======================================="
	@echo "Please use 'make build' command to build it.."
	@echo "Please use 'make rebuild' command to build it.."
	@echo "======================================="

INCLUDES += -I../ -I../../ -I../../../ -I../../src -I/usr/local/include
LIBS = -L../ -L../../ -L../../../ -L/usr/local/lib
# CFLAGS = -Wall -O3 -fPIC --shared -DJEMALLOC -ljemalloc -Wl,-rpath,. -Wl,-rpath,.. -Wl,-rpath,/usr/local/lib
# CFLAGS = -Wall -O3 -fPIC --shared -DTCMALLOC -ltcmalloc -Wl,-rpath,. -Wl,-rpath,.. -Wl,-rpath,/usr/local/lib
CFLAGS = -Wall -O3 -fPIC --shared -Wl,-rpath,. -Wl,-rpath,.. -Wl,-rpath,/usr/local/lib

# 构建lkcp.so依赖库
build:
	@$(CC) -o lkcp.so lkcp.c ikcp.c $(CFLAGS) $(INCLUDES) $(LIBS) -lcore
	@mv *.so ../