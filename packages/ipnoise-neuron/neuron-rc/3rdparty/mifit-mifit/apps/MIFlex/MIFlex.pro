######################################################################
# Automatically generated by qmake (1.07a) Tue Jun 19 12:24:16 2007
######################################################################

TEMPLATE = app
TARGET=MIFlex
CONFIG-=opengl
CONFIG-=qt
CONFIG+=console
DEPENDPATH += preferences
INCLUDEPATH += .

# to copy executable from build dir to source distribution dir
target.path = $$TOP_SRCDIR/apps/MIFit/
INSTALLS += target

LIBS += -L../../libs -lligand -lmiutil -lmolopt -lmap  -lmtz -lconflib -lchemlib -lmimath -lmiopengl -lnongui

VPATH += $${PWD}
HEADERS += *.h
SOURCES += common.cpp miflex.cpp