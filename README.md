<div align="center">
  
### Shell Emulator

[![forthebadge](https://forthebadge.com/images/badges/made-with-c.svg)](https://forthebadge.com)

    
</div>

This is a program written in C to emulate a shell and demonstrate use of forks and pipes.

 `make run` to build and execute.
 
 The program supports:
 - single commands
 - pipes between two commands
 - arbitrary # of pipes
 - input and output redirection
 
 Example commands:
 - `ls -al`
 - `ps -u <username>`
 - `who | wc -l`
 - `ls /usr/bin | head -10 | tail -5`
 - `wc -l < inputfile`
 - `who > outputfile`
 - `wc -l < inputfile > outputfile`
 - `tr "A-Z" "a-z" < wcExample.cpp | tr -cs "a-z" '\012' | sort | uniq -c | sort -nr | head -1`
