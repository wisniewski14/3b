/*
 * Jesse Wisniewski 103130867
 * CSCI 3753 - Programming Assignment 3
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <fcntl.h>
#include <unistd.h>

#include "util.h"
#include "queue.h"
//#include "multi-lookup.h"

#define MINARGS 3
#define USAGE "<inputFilePath> <outputFilePath>"
#define SBUFSIZE 1025
#define INPUTFS "%1024s"
#define Q_SIZE 10
#define NUM_THREADS 10

typedef struct threadstuff{
	FILE* filep;
	queue* queuep;
	int* finvalp;
	int numf;
	pthread_mutex_t* qlock;
	pthread_mutex_t* flock;
	pthread_cond_t* condvar;
	pthread_cond_t* condfree;
} stuf;

void* Requester_function(void* vpoint){
	struct threadstuff* args = vpoint; 
	const int qsize = Q_SIZE;
	size_t bufsiz = SBUFSIZE;
	char** names = malloc(Q_SIZE * sizeof(char*));
	int i;
	for(i=0;i<qsize;i++){
		names[i] = malloc(SBUFSIZE * sizeof(char));
	}
	/* Read File and add to queue*/
	i=0;
	while(getline(&names[i],&bufsiz,args->filep) != -1){
		fprintf(stderr,"pushing %s\n",names[i]);
		pthread_mutex_lock(args->qlock);
			if(queue_is_full(args->queuep)){
				pthread_cond_wait(args->condvar,args->qlock);
			}
			queue_push(args->queuep,names[i]);
		pthread_mutex_unlock(args->qlock);
		i++;
		if (i==qsize){
			i=0;
		}
	}
	*args->finvalp=1;
	pthread_mutex_lock(args->qlock);
	pthread_cond_wait(args->condfree,args->qlock);
	pthread_mutex_unlock(args->qlock);
	for(i=0;i<qsize;i++){
		free(names[i]);
	}
	free(names);
	return NULL;
}

void* Resolver_function(void* vpoint){
	struct threadstuff* args = vpoint; 
	char firstipstr[INET6_ADDRSTRLEN];
	char outputstr[SBUFSIZE];
	char* name;
	int i,nsiz;
	int finsiz = args->numf;//(sizeof(*args->finvalp)/sizeof(int));
	fprintf(stderr,"\nSIZE: %d\n",finsiz);
	int notdone=1;
	while(notdone){
		fprintf(stderr, "FINISH array:");
		for(i=0;i<finsiz;i++){
			 fprintf(stderr, " %d ", args->finvalp[i]);
			
		}
		fprintf(stderr, "\n");
		pthread_mutex_lock(args->qlock);
		if(queue_is_empty(args->queuep)){
			pthread_mutex_unlock(args->qlock);
			notdone=0;
			for(i=0;i<finsiz;i++){
				if(args->finvalp[i]==0){
					notdone=1;
				}
			}
		}else{
				name = queue_pop(args->queuep);
				strcpy(outputstr, name);
			pthread_mutex_unlock(args->qlock);
			pthread_cond_signal(args->condvar);
			nsiz=strlen(outputstr);
			for(i=0;i<nsiz;i++){	
				if(outputstr[i]=='\n'){outputstr[i]=0;break;}
			}
		        /* Lookup hostname and get IP string */
		        if(dnslookup(outputstr, firstipstr, sizeof(firstipstr)) == UTIL_FAILURE){
			    fprintf(stderr, "dnslookup error: %s\n", outputstr);
			    strncpy(firstipstr, "", sizeof(firstipstr));
		        }
		        /* Write to Output File */
			strcat(outputstr,", ");
			strcat(outputstr,firstipstr);
			strcat(outputstr,"\n");
		        //fprintf(args->filep, "%s,%s\n", name, firstipstr);
			pthread_mutex_lock(args->flock);
		        if(write(fileno(args->filep),outputstr,strlen(outputstr))!=(int)strlen(outputstr)){
				fprintf(stderr,"\nWRITE ERROR\n");	
			}
			pthread_mutex_unlock(args->flock);
		}
	}	
	pthread_cond_broadcast(args->condfree);
	return NULL;
}





int main(int argc, char* argv[]){

    /* Local Vars */
    queue* myQ = malloc(sizeof(queue));
    const int qsize = Q_SIZE;
    int* finish = malloc((argc-2)*sizeof(int));
    struct threadstuff** argarr = malloc((argc-1)*sizeof(struct threadstuff*));
    FILE** inputfp= malloc((argc-2)*sizeof(FILE*));
    FILE* outputfp = NULL;
    char errorstr[SBUFSIZE];
    int i;
    pthread_t* threadid = malloc(((argc-2)+NUM_THREADS)*sizeof(pthread_t));
    pthread_mutex_t* qlock = malloc(sizeof(pthread_mutex_t));
    pthread_mutex_t* flock = malloc(sizeof(pthread_mutex_t));
    pthread_cond_t* condv = malloc(sizeof(pthread_cond_t));
    pthread_cond_t* condf = malloc(sizeof(pthread_cond_t));
    for(i=0;i<argc-1;i++){
        argarr[i] = malloc(sizeof(struct threadstuff));
    }
    pthread_mutex_init(qlock,NULL);
    pthread_mutex_init(flock,NULL);
    pthread_cond_init(condv,NULL);
    pthread_cond_init(condf,NULL);
    queue_init(myQ, qsize);
    /* Check Arguments */
    if(argc < MINARGS){
	fprintf(stderr, "Not enough arguments: %d\n", (argc - 1));
	fprintf(stderr, "Usage:\n %s %s\n", argv[0], USAGE);
	return EXIT_FAILURE;
    }
    /* Open Output File */
    outputfp = fopen(argv[(argc-1)], "w");
    if(!outputfp){
	perror("Error Opening Output File");
	return EXIT_FAILURE;
    }
    
    /* Loop Through Input Files */
    for(i=0; i<(argc-2); i++){
	
	/* Open Input File */
	inputfp[i] = fopen(argv[i+1], "r");
	if(!inputfp[i]){
	    sprintf(errorstr, "Error Opening Input File: %s", argv[i+1]);
	    perror(errorstr);
	    break;
	}	

        argarr[i]->filep = inputfp[i];
        argarr[i]->queuep = myQ;
        argarr[i]->qlock = qlock;
        argarr[i]->flock = flock;
        argarr[i]->condvar = condv;
        argarr[i]->condfree = condf;
	finish[i]=0;
        argarr[i]->finvalp = &finish[i];
    
        printf("Creating requester thread\n");
        pthread_create(&(threadid[i]), NULL, Requester_function, argarr[i]);
    

    }
    argarr[argc-2]->filep = outputfp;
    argarr[argc-2]->queuep = myQ;
    argarr[argc-2]->qlock = qlock;
    argarr[argc-2]->flock = flock;
    argarr[argc-2]->condvar = condv;
    argarr[argc-2]->condfree = condf;
    argarr[argc-2]->finvalp = finish;
    argarr[argc-2]->numf = argc-2;
    for(i=0;i<NUM_THREADS;i++){
        printf("Creating resolver thread\n");
        pthread_create(&(threadid[argc-2+i]), NULL, Resolver_function, argarr[argc-2]);
    }

    /* Close Output File */
    for(i=0;i<NUM_THREADS;i++){
        (void) pthread_join(threadid[argc-2+i], NULL);
    }
    fclose(outputfp);
    /* Close Input Files */
    for(i=0;i<argc-2;i++){
        (void) pthread_join(threadid[i], NULL);
    }
    for(i=0;i<(argc-2);i++){
	fclose(inputfp[i]);
    }
    for(i=0;i<argc-1;i++){
        free(argarr[i]);
    }
    queue_cleanup(myQ);
    free(myQ);
    free(qlock);
    free(flock);
    free(inputfp);
    free(argarr);
    free(condv);
    free(condf);
    free(finish);
    free(threadid);
    return EXIT_SUCCESS;
}
