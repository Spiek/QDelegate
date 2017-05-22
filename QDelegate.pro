QT += core
QT -= gui

CONFIG += c++14

TARGET = QDelegate
CONFIG += console
CONFIG -= app_bundle

TEMPLATE = app

HEADERS += include/qdelegate.h
SOURCES += src/main.cpp

INCLUDEPATH += include
