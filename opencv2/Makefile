SRC	+= precomp.cpp
SRC	+= vibe_gpu.cpp
SRC	+= vibe.cu

TRG	= opencv_nonfree

OBJ	= $(subst .S,.o,$(subst .c,.o,$(subst .cpp,.o,$(SRC:.cu=.o))))
CC	= gcc
FLAGS	+= -Wall -O2 -mfpu=neon -funsafe-math-optimizations -fabi-version=8 -Wabi -march=armv7-a -fopenmp
FLAGS	+= -I.. -I/usr/local/cuda-6.5/targets/armv7-linux-gnueabihf/include
FLAGS	+= -DHAVE_OPENCV_GPU -DHAVE_CUDA
CFLAGS	+= $(FLAGS) -Ibgslibrary -Wno-unused-but-set-variable
CXXFLAGS	+= $(FLAGS) -std=c++11 -Ibgslibrary -Wno-unused-but-set-variable
LDFLAGS	+= -O2
LDFLAGS	+= -lglfw3 -lGLEW -lGL
LDFLAGS	+= -lopencv_core -lopencv_highgui -lopencv_imgproc -lopencv_gpu -lopencv_video
LDFLAGS	+= -lXxf86vm -lpthread -lXrandr -lXinerama -lXi -lXcursor -lX11 -ldl

OS_SIZE    = $(shell uname -m | sed -e "s/x86_64/64/" -e "s/armv7l/32/" -e "s/aarch64/64/")
OS_ARCH    = $(shell uname -m)
ARCH_FLAGS =

#SMS = 50 52
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
all: lib$(TRG).a

$(TRG): $(OBJ)
	$(CXX) $^ $(LDFLAGS) -o $@

$(TRG)_cuda: $(OBJ)
	/usr/local/cuda-6.5/bin/nvcc -ccbin g++ -m32 -Xcompiler -mfloat-abi=hard -Xlinker --dynamic-linker=/lib/ld-linux-armhf.so.3 $^ $(LDFLAGS) -o $@

lib$(TRG).a: $(OBJ)
	$(AR) rcs $@ $^

%.o: %.cu
	/usr/local/cuda-6.5/bin/nvcc -ccbin g++ -m32 -Xcompiler -mfloat-abi=hard -po maxrregcount=32 $(GENCODE_FLAGS) -o $@ -c $<

.PHONY: run
run: $(TRG)
	sync
	DISPLAY=:0 ./$(TRG) $(DEV) $(RES)

.PHONY: clean
clean:
	rm -f lib$(TRG).a $(OBJ)
