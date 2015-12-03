TEMPLATE = app
CONFIG += c++11
CONFIG -= console
CONFIG -= app_bundle
CONFIG -= qt

QMAKE_INCDIR	+= D:\Programs\misc\opencv\build\include
QMAKE_INCDIR	+= D:\Programs\misc\opencv\build\include\opencv
QMAKE_LIBS	+= -lopencv_core2410.dll
QMAKE_LIBS	+= -lopencv_imgproc2410.dll
QMAKE_LIBS	+= -lopencv_highgui2410.dll
QMAKE_LIBS	+= -lopencv_objdetect2410.dll
QMAKE_LIBDIR	+= D:\Programs\misc\opencv\build\x86\mingw\bin
QMAKE_LIBDIR	+= D:\Programs\misc\opencv\build\x86\mingw\lib

SOURCES += \
    Object.cpp \
    objectDetection2.cpp

HEADERS += \
    Object.h
