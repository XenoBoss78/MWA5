#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <err.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sysexits.h>
#include <fcntl.h>


/* This file and all derivatives (C) 2012 Neil Spring and the
 * University of Maryland.  Any use, copying, or distribution
 * without permission is prohibited.
 */

/* Each command to execute is a leaf in this tree. */
/* Commands are tied together in the tree, left side
   executed first. */
struct tree {
  enum { NONE = 0, AND, OR, SEMI, PIPE, SUBSHELL } conjunction;
  struct tree *left, *right;
  char **argv;
  char *input;
  char *output;
};

/* Array of strings to match the conjunction enum above.
   print the element using conj[tree->conjunction] */
/* Defined here since we might change the operations. */

static const char *conj[] __attribute__((unused)) = { "err", "&&", "||", ";", "|", "()" };

int recursive_helper(struct tree *t, int in_fd, int out_fd);

int execute(struct tree *t) {
  if( t != NULL){
    return recursive_helper(t, 1, 1);
  }
  return 0;
}

int recursive_helper(struct tree *t, int in_fd, int out_fd) {
  int fd, status, pipe_fd[2], pipe_stat;
  
  if(t->conjunction == NONE){  /*Checks if the current node is a command*/
    
    if(strcmp(t->argv[0], "cd") == 0){
      chdir(t->argv[1]);
    } else if(strcmp(t->argv[0],"exit") == 0){
      /*Exit the program*/
      exit(0);
    } else {
      /*Execute Command*/
      fd = fork();
      if (fd == -1) {
        perror("fork error");
      } else if (fd == 0) {          /* Child executing command */

        if (t->input) {
          in_fd = open(t->input, O_RDONLY);
		      if (in_fd < 0) {
		        perror("Unable to open input file.\n");     
		      }
		      if (dup2(in_fd, STDIN_FILENO) < 0) {
		        perror("dup2 input failed.\n");
		      }		     
		      close(in_fd);
	       }       	       

        if (t->output) {
          out_fd = open(t->output, O_WRONLY | O_CREAT | O_TRUNC, 0664);
          if (out_fd < 0) {
            perror("Unable to open output file.\n");
		      }		     
          if (dup2(out_fd, STDOUT_FILENO) < 0) {
            perror("dup2 output failed.\n");
		      }		     
		      close(out_fd);		  
	      }
        
        
        execvp(t->argv[0],t->argv);
        
        exit(EX_OSERR);   /*Due to the nature of exe*, this will only run if exe* fails*/
        printf("Failed to Execute %s\n", t->argv[0]);
        fflush(stdout);
        exit(-1);
      } else {
        wait(&status); /* Reaping */
        return status;
      }
    }
  } else if(t->conjunction == AND) {
    if (t->input) {
          in_fd = open(t->input, O_RDONLY);
		      if (in_fd < 0) {
		        perror("Unable to open pipe input file.\n");     
		      }     
	       }       	       

        if (t->output) {
          out_fd = open(t->output, O_WRONLY | O_CREAT | O_TRUNC, 0664);
          if (out_fd < 0) {
            perror("Unable to open pipe output file.\n");
          }
          }
    
    status = recursive_helper(t->left,in_fd,out_fd);  
      if (status == 0) {	    
        recursive_helper(t->right, in_fd, out_fd);
      }
  } else if(t->conjunction == PIPE) {
    
    if (t->left->output) {
      printf("Ambiguous output redirect.\n");	   
    } else if (t->right->input) {
      printf("Ambiguous input redirect.\n");
	 } else { /*No Ambiguous redirection*/

      if (t->input) {
          in_fd = open(t->input, O_RDONLY);
		      if (in_fd < 0) {
		        perror("Unable to open pipe input file.\n");     
		      }     
	       }       	       

        if (t->output) {
          out_fd = open(t->output, O_WRONLY | O_CREAT | O_TRUNC, 0664);
          if (out_fd < 0) {
            perror("Unable to open pipe output file.\n");
          }
          }
          	  
    
      /*Make the pipe*/

  pipe_stat = pipe(pipe_fd);
  if(pipe_stat < 0){
    perror("pipe error");
  }
  fd = fork();
  if (fd == -1) {
    perror("fork error");
   } else if(fd == 0) { /*Child*/
    
    close(pipe_fd[0]);
    if (dup2(pipe_fd[1], STDOUT_FILENO) < 0) {
      perror("dup2 error");
      }
	  recursive_helper(t->left, in_fd, pipe_fd[1]);		  
	  close(pipe_fd[1]);
	  exit(0);
    
    } else { /*Parent*/
    
    close(pipe_fd[1]);		  
    if (dup2(pipe_fd[0], STDIN_FILENO) < 0) {
      perror("dup2 error");
      }
    recursive_helper(t->right, pipe_fd[0], out_fd);	  
	  close(pipe_fd[0]);
	  wait(NULL); /* Reaping */
    
    }
          }
  } else if(t->conjunction == SUBSHELL){
    if (t->input) {
          in_fd = open(t->input, O_RDONLY);
		      if (in_fd < 0) {
		        perror("Unable to open subshell input file.\n");     
		      }     
	       }       	       

        if (t->output) {
          out_fd = open(t->output, O_WRONLY | O_CREAT | O_TRUNC, 0664);
          if (out_fd < 0) {
            perror("Unable to open subshell output file.\n");
          }
          }
    
    fd = fork();
    if (fd == -1) {
    perror("fork error");
   } else if(fd == 0) { /*Child subshell*/
      recursive_helper(t->left, in_fd,out_fd);
      exit(0);
  } else {
      wait(NULL); /*Parent reaping*/
  }
    }
  return 0;
}