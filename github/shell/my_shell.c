#include  <stdio.h>
#include  <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdbool.h>
#include <signal.h>

#define MAX_INPUT_SIZE 1024
#define MAX_TOKEN_SIZE 64
#define MAX_NUM_TOKENS 64

/* Splits the string by space and returns the array of tokens
*
*/
bool repeat=true;
int fpgid = 0;
char **tokenize(char *line)
{
  char **tokens = (char **)malloc(MAX_NUM_TOKENS * sizeof(char *));
  char *token = (char *)malloc(MAX_TOKEN_SIZE * sizeof(char));
  int i, tokenIndex = 0, tokenNo = 0;

  for(i =0; i < strlen(line); i++){

    char readChar = line[i];

    if (readChar == ' ' || readChar == '\n' || readChar == '\t'){
      token[tokenIndex] = '\0';
      if (tokenIndex != 0){
	tokens[tokenNo] = (char*)malloc(MAX_TOKEN_SIZE*sizeof(char));
	strcpy(tokens[tokenNo++], token);
	tokenIndex = 0; 
      }
    } else {
      token[tokenIndex++] = readChar;
    }
  }
 
  free(token);
  tokens[tokenNo] = NULL ;
  return tokens;
}
void intHandler(int sig){
	repeat = false;
	//printf("ctrl-c clicked\n");
	if(fpgid>0)
	{
	kill(-fpgid, SIGKILL);}
	//signal(SIGINT, intHandler);
	
}

pid_t my_execute(char ** tokens, bool fg, bool bg){
	if (tokens[0]==NULL) return -1;
	if (strcmp(tokens[0],"exit")==0) return -2;
	if (strcmp(tokens[0],"cd")==0){
		
		if (tokens[1]==NULL || tokens[2] != NULL){
			printf("Shell: Incorrect command\n");
		}
		else{
			if (chdir(tokens[1]) != 0) 
				perror("Error");
		}
		return -1;
	}
	else{
		pid_t rc = fork();
		
		if(fpgid == 0) fpgid = rc;
		if (rc < 0){
			fprintf(stderr, "fork failed\n");
			exit(1);
		}
		else if (rc==0){
			if(!bg){
			setpgid(0,fpgid);
			signal(SIGTTIN,SIG_IGN);			
			}
			signal(SIGINT,SIG_IGN);
			
			execvp(tokens[0], tokens);
			perror("Error");
			exit(1);

			//printf("Should not be printed" , (int) getpid());
		}
		else {
			signal(SIGINT,intHandler);
			if(fg){
				int wc = waitpid(rc,NULL,0);
				fpgid = 0;
			}
			
		}
		return rc;
			
	}
}

int main(int argc, char* argv[]) {
	char  line[MAX_INPUT_SIZE];            
	char  **tokens;             

	while(1) {			
		/* BEGIN: TAKING INPUT */
		int tokensI=0;
		int currI = 0;
		bool bg=false;
		bzero(line, sizeof(line));
		printf("$ ");
		scanf("%[^\n]", line);
		getchar();
		pid_t p;
		bool fg=true, w=false, ter=false;
		repeat=true;
		line[strlen(line)] = '\n'; //terminate with new line
		while ((p=waitpid(-1, NULL, WNOHANG)) > 0)
    	{
			printf("Shell: Background process finished\n");
    	}
		tokens = tokenize(line);
		if (tokens[0]==NULL){free(tokens); continue;}
		pid_t pids[MAX_NUM_TOKENS];
		for(int i=0; i<MAX_NUM_TOKENS; i++)pids[i]=0;
		int pid_count = 0;
		fpgid =  0;
		//char **token = (char **)malloc(MAX_NUM_TOKENS * sizeof(char *));
		do{ 
			
			if(tokens[tokensI]!=NULL && strcmp(tokens[tokensI], "&")==0){
				fg=false;bg=true;
			}
			if(tokens[tokensI]!=NULL && strcmp(tokens[tokensI], "&&&")==0){
				w=true; fg=false;
			}
			if(tokens[tokensI]==NULL || strcmp(tokens[tokensI], "&")==0){
				repeat=false;
			}
			if(tokens[tokensI]==NULL||strcmp(tokens[tokensI], "&&")==0||strcmp(tokens[tokensI], "&&&")==0||strcmp(tokens[tokensI], "&")==0){
				free(tokens[tokensI]);
				tokens[tokensI] = NULL;
				pids[pid_count] = my_execute(tokens+currI,fg, bg);
				
				pid_count++;
				currI=tokensI+1;
			}
			if (pids[pid_count-1]==-2){
				ter = true;
				repeat= false;
			}
		
			tokensI++;
		}
		while(repeat);
		if(w)
		for (int i=0; i<=pid_count; i++){
			if(pids[i]>0)waitpid(pids[i],NULL,0);
		}
       
		for(int i=0;i<tokensI;i++){
			if(tokens[i]!=NULL)
			free(tokens[i]);
		}
		free(tokens);
//		if(token){free(token);}
		if(ter){
			kill (0,SIGKILL);
			return 0;
		}

	}
	return 0;
}
