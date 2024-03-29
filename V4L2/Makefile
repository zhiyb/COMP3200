SRC	= helper.cpp main.cpp
SRC	+= yavta.c
SRC	+= cv.cpp
SRC	+= cv_gpu.cpp
#SRC	+= cv_cuda.cu
SRC	+= preview.cpp
SRC	+= input.cpp

TRG	= V4L2
DEV	= /dev/video0
#RES	= 2592 1944
#RES	= 1920 1080
#RES	= 1280 960
RES	= 640 480
#RES	= 320 240

OBJ	= $(subst .S,.o,$(subst .c,.o,$(subst .cpp,.o,$(SRC:.cu=.o))))
CC	= gcc
BGSLIB	= ../bgslibrary
#FLAGS	+= -g -pg
FLAGS	+= -Wall -O2 -mfpu=neon -funsafe-math-optimizations -fabi-version=8 -Wabi -march=armv7-a -fopenmp
FLAGS	+= -I. -I.. -I../bgslibrary -I/usr/local/cuda-6.5/targets/armv7-linux-gnueabihf/include
FLAGS	+= -DHAVE_OPENCV_GPU -DHAVE_CUDA
CFLAGS	+= $(FLAGS) -Wno-unused-but-set-variable
CXXFLAGS	+= $(FLAGS) -std=c++11 -I$(BGSLIB) -Wno-unused-but-set-variable
#LDFLAGS	+= -g
LDFLAGS	+= -O2
LDFLAGS += -L$(BGSLIB) -lbgslibrary
LDFLAGS	+= -L../opencv2 -lopencv_nonfree
LDFLAGS	+= -lglfw3 -lGLEW -lGL
LDFLAGS	+= -lopencv_core -lopencv_highgui -lopencv_imgproc -lopencv_gpu -lopencv_video
LDFLAGS	+= -lXxf86vm -lpthread -lXrandr -lXinerama -lXi -lXcursor -lX11 -ldl

OS_SIZE    = $(shell uname -m | sed -e "s/x86_64/64/" -e "s/armv7l/32/" -e "s/aarch64/64/")
OS_ARCH    = $(shell uname -m)
ARCH_FLAGS =

SMS = 50 52
# Gencode arguments
ifeq ($(OS_ARCH),armv7l)
SMS ?= 20 30 32 35 37 50 52
else
SMS ?= 11 20 30 35 37 50 52
endif

ifeq ($(SMS),)
$(info >>> WARNING - no SM architectures have been specified - waiving sample <<<)
SAMPLE_ENABLED := 0
endif

ifeq ($(GENCODE_FLAGS),)
# Generate SASS code for each SM architecture listed in $(SMS)
$(foreach sm,$(SMS),$(eval GENCODE_FLAGS += -gencode arch=compute_$(sm),code=sm_$(sm)))

# Generate PTX code from the highest SM architecture in $(SMS) to guarantee forward-compatibility
HIGHEST_SM := $(lastword $(sort $(SMS)))
ifneq ($(HIGHEST_SM),)
GENCODE_FLAGS += -gencode arch=compute_$(HIGHEST_SM),code=compute_$(HIGHEST_SM)
endif
endif

.PHONY: all
all: $(TRG)

$(TRG): $(OBJ)
	/usr/local/cuda-6.5/bin/nvcc -ccbin g++ -m32 -Xcompiler -mfloat-abi=hard -Xlinker --dynamic-linker=/lib/ld-linux-armhf.so.3 $^ $(LDFLAGS) -o $@
#$(CXX) $^ $(LDFLAGS) -o $@

%.o: %.cu
	/usr/local/cuda-6.5/bin/nvcc -ccbin g++ -m32 -Xcompiler -mfloat-abi=hard -po maxrregcount=32 $(GENCODE_FLAGS) -o $@ -c $<
#	/usr/local/cuda-6.5/bin/nvcc -ccbin g++ -m32 -Xcompiler "-g -pg" -Xcompiler -mfloat-abi=hard -po maxrregcount=32 $(GENCODE_FLAGS) -o $@ -c $<

.PHONY: run
run: $(TRG)
	sync
	sudo ~/kernel/modprobe_reset.sh
	DISPLAY=:0 ./$(TRG) $(DEV) $(RES)

.PHONY: clean
clean:
	rm -f $(TRG) $(OBJ)
