TEMPLATE = app
TARGET = 
LIBS +=  -lusb-1.0
# Input
HEADERS += src/si570synth.h
FORMS += gui/si570synth.ui
SOURCES += src/main.cpp \
           src/si570synth.cpp
RESOURCES += resources/si570synth.qrc
