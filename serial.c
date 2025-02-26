#include <dirent.h> 
#include <stdio.h> 
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>
#include <time.h>
#include <pthread.h>

#define BUFFER_SIZE 1048576 // 1MB
#define NUM_TH 8

pthread_mutex_t dumpLock;
pthread_cond_t dumpCond;
pthread_mutex_t lock, Indexlock;

char **files = NULL;
int nfiles = 0;
FILE *f_out;
char tempArgv[50];
int total_in = 0, total_out = 0;

int globIndex = 0;
int dumpIndex = 0;

typedef struct {
    unsigned char *frame; 
    int CompFrameSize; 
	int readyFlag;
} frameStruct;

frameStruct *sharedBuffer = NULL; //buffer to hold compressed frames


void *processFiles(void *arg);
void *dumpFiles(void *arg);

int cmp(const void *a, const void *b) {
	return strcmp(*(char **) a, *(char **) b);
}

int main(int argc, char **argv) {
	// time computation header
	struct timespec start, end;
	clock_gettime(CLOCK_MONOTONIC, &start);
	// end of time computation header

	// do not modify the main function before this point!

	assert(argc == 2);

	DIR *d;
	struct dirent *dir;

	//Initialize locks and condition variables:
	pthread_mutex_init(&lock, NULL);
	pthread_mutex_init(&Indexlock, NULL);
	pthread_mutex_init(&dumpLock, NULL);
	pthread_cond_init(&dumpCond, NULL);

	d = opendir(argv[1]);
	if(d == NULL) {
		printf("An error has occurred\n");
		return 0;
	}

	// create sorted list of PPM files
	while ((dir = readdir(d)) != NULL) {
		files = realloc(files, (nfiles+1)*sizeof(char *));
		assert(files != NULL);

		int len = strlen(dir->d_name);
		if(dir->d_name[len-4] == '.' && dir->d_name[len-3] == 'p' && dir->d_name[len-2] == 'p' && dir->d_name[len-1] == 'm') {
			files[nfiles] = strdup(dir->d_name);
			assert(files[nfiles] != NULL);

			nfiles++;
		}
	}
	closedir(d);
	qsort(files, nfiles, sizeof(char *), cmp);

	
	f_out = fopen("video.vzip", "w");
	assert(f_out != NULL);
	
	//copy directory path 
	strcpy(tempArgv, argv[1]);
	
	sharedBuffer = malloc(nfiles * sizeof(frameStruct));
	//initialize each struct in the buffer to not ready
	for (int i = 0; i < nfiles; i++) {
		sharedBuffer[i].readyFlag = 0; 
	}

	pthread_t threads[NUM_TH];

	//distribute workload among threads based on gloablIndex
	for (int i = 0; i < NUM_TH; i++) {
		pthread_create(&threads[i], NULL, processFiles, NULL);
	}

	pthread_t dumpThread;
	pthread_create(&dumpThread, NULL, dumpFiles, NULL);

	//wait for threads:
	for (int i = 0; i < NUM_TH; i++) {
		pthread_join(threads[i], NULL);
	}
	pthread_join(dumpThread, NULL);


	free(sharedBuffer);

	pthread_mutex_destroy(&lock);
	pthread_mutex_destroy(&dumpLock);
	pthread_mutex_destroy(&Indexlock);
	pthread_cond_destroy(&dumpCond);

	fclose(f_out);

	printf("Compression rate: %.2lf%%\n", 100.0*(total_in-total_out)/total_in);

	// release list of files
	for(int i=0; i < nfiles; i++)
		free(files[i]);
	free(files);


	// do not modify the main function after this point!

	// time computation footer
	clock_gettime(CLOCK_MONOTONIC, &end);
	printf("Time: %.2f seconds\n", ((double)end.tv_sec+1.0e-9*end.tv_nsec)-((double)start.tv_sec+1.0e-9*start.tv_nsec));
	// end of time computation footer

	return 0;
}

// create a single zipped package with all PPM files in lexicographical order
void *processFiles(void *arg){
	while (1) {
		pthread_mutex_lock(&Indexlock);
		if (globIndex >= nfiles) {
			pthread_mutex_unlock(&Indexlock);
			break; //all files have been processed
		}
		int i = globIndex++;
		pthread_mutex_unlock(&Indexlock);
		
		//getting full file path
		int len = strlen(tempArgv)+strlen(files[i])+2;
		char *full_path = malloc(len*sizeof(char));
		assert(full_path != NULL);
		strcpy(full_path, tempArgv);
		strcat(full_path, "/");
		strcat(full_path, files[i]);

		unsigned char buffer_in[BUFFER_SIZE];
		unsigned char buffer_out[BUFFER_SIZE];

		// load file
		FILE *f_in = fopen(full_path, "r");
		assert(f_in != NULL);
		int nbytes = fread(buffer_in, sizeof(unsigned char), BUFFER_SIZE, f_in);
		fclose(f_in);

		// zip file
		z_stream strm;
		int ret = deflateInit(&strm, 9);
		assert(ret == Z_OK);
		strm.avail_in = nbytes;
		strm.next_in = buffer_in;
		strm.avail_out = BUFFER_SIZE;
		strm.next_out = buffer_out;

		ret = deflate(&strm, Z_FINISH);
		assert(ret == Z_STREAM_END);

		int nbytes_zipped = BUFFER_SIZE-strm.avail_out; //compressed size

		//current frame
		sharedBuffer[i].frame = malloc(nbytes_zipped);
		memcpy(sharedBuffer[i].frame, buffer_out, nbytes_zipped);
		sharedBuffer[i].CompFrameSize = nbytes_zipped;

		pthread_mutex_lock(&lock);
		total_out += nbytes_zipped;
		total_in += nbytes;
		pthread_mutex_unlock(&lock);

		free(full_path);


		pthread_mutex_lock(&dumpLock);
		//marking the frame as ready to dump
		sharedBuffer[i].readyFlag = 1;

		if (i == dumpIndex) {
			pthread_cond_signal(&dumpCond);
		}
		pthread_mutex_unlock(&dumpLock);

	}
}

//dump zipped files in order as they become ready using dumpIndex
void *dumpFiles(void *arg) {
    while (1) {
		pthread_mutex_lock(&dumpLock);
		while (dumpIndex < nfiles && !sharedBuffer[dumpIndex].readyFlag) {
			pthread_cond_wait(&dumpCond, &dumpLock);
		}

		//all files processed:
		if (dumpIndex >= nfiles) 
		{ 
			pthread_mutex_unlock(&dumpLock);
			break;
		}

		//dump zipped file from sharedBuffer in order
		fwrite(&sharedBuffer[dumpIndex].CompFrameSize, sizeof(int), 1, f_out);
		fwrite(sharedBuffer[dumpIndex].frame, sizeof(unsigned char), sharedBuffer[dumpIndex].CompFrameSize, f_out);

		dumpIndex++;
		pthread_mutex_unlock(&dumpLock);
    }
    return NULL;
}
