#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

void wc(FILE *ofile, FILE *infile, char *inname) {
  int lines, words, characters;
  bool inword;
  char ch;
  if(inname==NULL) return;
  if(ofile==NULL) ofile=stdout;
  if(infile==NULL) infile=stdin;

  lines=0;
  words=0;
  characters=0;
  inword=false;

  while((ch=getc(infile))!=EOF){
	characters++;
	if(ch=='\n')
	  lines++;
	if(!isspace(ch)&&!inword){
	  words++;
	  inword=true;
	}
	if(isspace(ch)&&inword)
	  inword=false;
  }

  fprintf(ofile, "lines: %d words: %d characters: %d %s\n",
  lines, words, characters, inname);
}

int main (int argc, char *argv[]) {

  char* out_file_name = NULL;
	char* inname = NULL;
	if(argc == 1) wc(NULL,NULL,NULL);
	else if(argc == 2){
		inname = argv[1];
		FILE *infile = fopen(inname,"r");
		if(!infile){
			perror("Input file opening failed");
			exit(1);
		}
		wc(NULL,infile,inname);
	}
	else if(argc == 3){
		inname = argv[1];
		FILE *infile = fopen(inname,"r");
		if(!infile){
			perror("Input file opening failed");
			exit(1);
		}
		FILE *out_file = fopen(argv[2],"w");
		if(!out_file){
			perror("Output file opening failed");
			exit(1);
		}
		wc(out_file,infile,inname);
	}
	else{
		printf("arguments error\n");
		exit(1);
	}
	return 0;

}
