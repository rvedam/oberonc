# Oberon Compiler on top of LLVM

## Project Description

This is an attempt to leverage AI tools to build an Oberon Compiler on top of LLVM
in an effort to be able to compile the Project Oberon operating system on top of 
various architectures (more on this later)

## Project Goals

- to compile the latest Oberon Specification
- to compile and run Project Oberon (the operating system) from the original source files
  as was specified in [projectoberon.net](https://projectoberon.net)
- Once kernel compiles, there are ideas on adding new modules to Project Oberon when and where
appropriate while still maintaining the spirit of Oberon

## Compiler Project Status

- language specification is currently fully implemented, but a more thorough test suite is required 
  due to the discovery of some simple issues (currently multi-module imports seems to work, along 
  with correctly accessing exported symbols, but I'm discovering new issues while writing simple programs
- 
