typedef struct threadstuff{
	FILE* filep;
	queue* queuep;
	int* finvalp;
	pthread_mutex_t* qlock;
	pthread_mutex_t* flock;
	pthread_cond_t* condvar;
} stuf;
