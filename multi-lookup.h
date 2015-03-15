typedef struct threadstuff{
	FILE* filep;
	int* tid;
	queue* queuep;
	int* finvalp;
	int numf;
	pthread_mutex_t* qlock;
	pthread_mutex_t* flock;
	pthread_cond_t* condvar;
	pthread_cond_t* condfree;
} stuf;
