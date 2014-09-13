#-------------------------------------------------
#
# Project created by QtCreator 2014-09-11T20:15:18
#
#-------------------------------------------------

QT       -= core gui

TARGET = repeator
CONFIG   += console
CONFIG   -= app_bundle
LIBS = -lwsock32
TEMPLATE = app


SOURCES += \
    main.c \
    udprepeator.c

HEADERS += \
    udprepeator.h
