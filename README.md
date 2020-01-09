# mocker

[![Build Status](https://travis-ci.com/Engineev/mocker.svg?token=t7LhMb4BZCM8Q58kCnsH&branch=master)](https://travis-ci.com/Engineev/mocker)

Hello. If you are reading this, then probably you are taking a compiler course
and going to implement your own compiler, and going to read my code. However,
the quality of code varies. Hence, I wrote this down as a guide and I wish it,
as well as my code, would be helpful.

## Overview

This project consists of three main directories: `compiler/`, `ir/` and
`ir-interpreter`. Most of the code is in `compiler/`. The implemtation of
my IR is in `ir/` and I also wrote a naive interpreter for my IR. It is only
a legacy issue that there are three directories. 

## Frontend

By frontend, I mean the parsing, semantic analysis and IRgen. The code of this
part is in the corresponding subdirectories of `compiler/`. 

### Parser 
I handcrafted a LL recursive descent parser. Some ad hoc hacks are used to 
simplify the implementation. In most cases, you can use parser generators such
as ANTLR to generate your LR parser. The only reason I handcrafted a parser is
that I thought it would take less time. Anyway, if you are going to implement
your own LL parser, maybe you could read the comments in `parse/parser.h`. I
wrote down the grammar there, which might be helpful.

### AST
My implementation of the AST is NOT good. When I wrote that part, I was not
familiar with that Java-like design pattern. Therefore, I used public fields
instead of make them read-only using getter. You could see my implementation of
my IR to understand what I mean here. Except this issue, there are no big 
problems.

Meanwhile, instead of recording everything in the AST node structure, I assign
each node a unique identifier and record the need information such as types in
separate containers. I think this design is better since it is much more 
flexible and elegant (personal opinion). 

### Semantic analysis
This part is quite straightforward.

### IRGen
I believe my code is quite clear. On some subtle things such as translating
array constructions, I wrote detailed comments (Cf. 
`Builder::translateNewArray`).


## Optimization

TODO

## Codegen

TODO

