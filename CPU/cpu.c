#include <stdio.h>
#include <stdlib.h>
#include "common.h"

int main(int argc, char *argv[])
{
    if (argc != 2) {
	fprintf(stderr, "usage: cpu <string>\n");
	exit(1);
    }
    char *str = argv[1];
    while (1) {
	printf("%s\n", str);
	Spin(1);
    }
    return 0;
}
/**
 * ps – process status 

a- show processes from all users 

u- user-oriented format 

x- show processes without controlling terminal 
 

| Field   | Value       | Meaning                                      | 

| ------- | ----------- | -------------------------------------------- | 

| USER    | `baquon`    | Process owner                                | 

| PID     | `23666`     | Process ID                                   | 

| %CPU    | `96.5`      | Using ~96.5% of one CPU core                 | 

| %MEM    | `0.0`       | Almost no memory usage                       | 

| VSZ     | `442192144` | Virtual memory size                          | 

| RSS     | `768`       | Physical memory in RAM (KB)                  | 

| TTY     | `??`        | No controlling terminal (background process) | 

| STAT    | `R`         | Running                                      | 

| START   | `12:54PM`   | Start time                                   | 

| TIME    | `89:43.06`  | Total CPU time consumed                      | 

| COMMAND | `./cpu C`   | Executable and arguments                     | 

 */