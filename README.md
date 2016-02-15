## Overview

This projects solves the tile game using a distrubte system.
The input is the board and puzzle pieces and output is the number of ways to use all these pieces and generate the board.

## TOOL
Require:
* MPI system

## METHOD
First we convert the tile game to set cover problem and solve the problem using dancing link algorithm.

Along the way to use dancing link, we distribute the recursive calls.