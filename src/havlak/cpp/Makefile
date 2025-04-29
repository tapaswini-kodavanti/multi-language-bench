# Copyright 2011 Google Inc.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# OPTS=-O2

# a.out:  mao-loops.o LoopTesterApp.o
# 	gcc $(OPTS) LoopTesterApp.o mao-loops.o -lc -lstdc++

# mao-loops.o: mao-loops.cc
# 	gcc $(OPTS) -c mao-loops.cc

# LoopTesterApp.o: LoopTesterApp.cc
# 	gcc $(OPTS) -c LoopTesterApp.cc

# run: 
# 	./a.out

# clean:
# 	rm -f *~
# 	rm -f *.o
# 	rm -f a.out


OPTS=-O2 -std=c++17
CXX=g++

# Default target that cleans first then builds
all: clean a.out

a.out: mao-loops.o LoopTesterApp.o tarjan-loops.o fwbw-loops.o
	$(CXX) $(OPTS) LoopTesterApp.o mao-loops.o tarjan-loops.o fwbw-loops.o -lc

mao-loops.o: mao-loops.cc
	$(CXX) $(OPTS) -c mao-loops.cc

tarjan-loops.o: tarjan-loops.cc
	$(CXX) $(OPTS) -c tarjan-loops.cc

fwbw-loops.o: fwbw-loops.cc
	$(CXX) $(OPTS) -c fwbw-loops.cc

LoopTesterApp.o: LoopTesterApp.cc
	$(CXX) $(OPTS) -c LoopTesterApp.cc

run:
	./a.out

clean:
	rm -f *~
	rm -f *.o
	rm -f a.out


