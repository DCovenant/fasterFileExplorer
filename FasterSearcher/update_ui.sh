#!/bin/bash
# Script to copy generated ui_mainwindow.h to source directory for Qt Creator
cp build/FasterSearcher_autogen/include/ui_mainwindow.h . 2>/dev/null || echo "Build ui_mainwindow.h first"