TEMPLATE = app
TARGET = Si570Synth

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

LIBS +=  -lusb-1.0
# Input
HEADERS += src/si570synth.h
FORMS += gui/si570synth.ui
SOURCES += src/main.cpp \
           src/si570synth.cpp
RESOURCES += resources/si570synth.qrc
