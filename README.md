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

Please note I did not add the kernel sources to this project only some test files in the 
project-oberon directory. 

## Directory Structure

I'm currenty working on several different runtimes for this project:

- **runtime/platform** contains platform runtimes on the two architectures I mainly deal with daily
  (specifically x86\_64 and aarch64 to work with Apple Silicon). Apple Silicon just recently been implemented
- **runtime/boot** is a freestanding runtime to allow to boot Project Oberon.

## Compiler Project Status

- language specification is currently fully implemented, but a more thorough test suite is required 
  due to the discovery of some simple issues (currently multi-module imports seems to work, along 
  with correctly accessing exported symbols, but I'm discovering new issues while writing simple programs
- Kernel Compilation has been a work in progress (mainly due to missing features that were removed in the
  latest Oberon specification but would need to get re-added in order to compile the original source.)

