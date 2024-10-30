 // 1 supplier many cooks with supplier constraints
 // supplier messages fixed
 // supplier done
 // cook and student without counter constraints
 // fixed get_foodId for critical deadlock condition
 // student for now takes 1 food at a time // fix this later
 // cook messages fixed
 // added readFile and usage
 // added ParseCommandLine feature
 // free memory and cleaned unnecessary things
 // added error management and signal handling

#include<stdio.h>
#include<stdlib.h>
#include<semaphore.h>
#include<sys/types.h>
#include<sys/wait.h>
#include<sys/mman.h>
#include<fcntl.h>
#include<sys/stat.h>
#include<unistd.h>
#include<time.h>
#include<string.h>
#include<errno.h>


#define SharedMemory "/SEM_DATA"
#define size sizeof(struct data)
int *ID;
int N,M,K,S,T,L;
char FOOD[][20] = {"soup", "main course", "desert"};
pid_t cur;

struct Parsed{
	char * filename;
};

void usage(){

	printf("Usage: Take parameters for Mess Simulation Program\n");
	printf("  Options:\n");
	printf("    -N  \tSpecifies Number of cooks (N>2) \n");
	printf("    -M  \tSpecifies Number of Students (M>N and M>T) \n");
	printf("    -T  \tSpecifies Number of Tables (T>0)\n");
	printf("    -L  \tSpecifies Number of Rounds a student eats (L>=3)\n");
	printf("    -S  \tSpecifies size of the counter (S>=4)\n");
	printf("    -F  \tTakes filename path\n");
exit(0);
	}

struct Parsed parseCommandLine(int argc, char *argv[]){
	int opt, count=0;
	int done[6] = {0,0,0,0,0,0};
	struct Parsed ret;
	char *tmp;
	while((opt = getopt(argc, argv, ":N:M:L:S:T:F:")) != -1)
	{
		switch(opt)
		{
			case 'F':
				if (done[0]!=0){
					printf("Invalid command line argument, F used twice\n");
					usage();
				}
				ret.filename = strdup(optarg);
				done[0] = 1;
				break;

			case 'N':
				if (done[1]!=0){
					printf("Invalid command line argument, N used twice\n"); usage();
				}
				done[1] = 1;
				N = strtol(optarg, &tmp, 10);
				if (*tmp  != '\0') {printf("Bad Input for N\n"); usage();} break;

			case 'T':
				if (done[2]!=0){
					printf("Invalid command line argument, T used twice\n");usage();
				}
				done[2] = 1;
				T = strtol(optarg, &tmp, 10);
				if (*tmp  != '\0' ) {printf("Bad Input for T\n"); usage();} break;

			case 'M':
				if (done[3]!=0){
					printf("Invalid command line argument, M used twice\n");usage();
				}
				done[3] = 1;
				M = strtol(optarg, &tmp, 10);
				if (*tmp  != '\0') {printf("Bad Input for M\n"); usage();} break;

			case 'S':
				if (done[4]!=0){
					printf("Invalid command line argument, S used twice\n");usage();
				}
				done[4] = 1;
				S = strtol(optarg, &tmp, 10);
				if (*tmp  != '\0') {printf("Bad Input for S\n"); usage();} break;

			case 'L':
				if (done[5]!=0){
					printf("Invalid command line argument, L used twice\n");usage();
				}
				done[5] = 1;
				L = strtol(optarg, &tmp, 10);
				if (*tmp  != '\0') {printf("Bad Input for L\n"); usage();} break;

			case ':':
				printf("option needs a value\n");
				usage();
				break;
			case '?':
				printf("unknown option: %c\n", optopt);
				usage();
		}
	}

	for(int y=0; y<1; y++)
	if(done[y]!=1){
		printf("Too few arguments\n");
		usage();
	}

	if (T<=0 || S<=3 || L<=2 || N<=2 || M<=N || M<=T){
		printf("Input With Invalid Constraints\n");
		usage();
		}
	return ret;
};

void readfile(char * filename, int times){
	ID = (int *) calloc(times, sizeof(int));
	FILE * fp; fp = fopen(filename, "r");
	char tmp[200]; strcpy(tmp, filename);
	int i=0;

	if( fp == 0 )
    {
        switch(errno)
        {
            case EPERM:
                perror("Operation not permitted");
                break;
            case ENOENT:
            case ENFILE:
				strcat(tmp, " not found");
                perror(tmp);
                break;
            case EACCES:
                perror(filename);
                break;
            case ENAMETOOLONG:
                perror("Filename is too long");
                break;
            default:
                fprintf(stderr,"Unknown error\n");
        }
        exit(1);
    }
	char temp;
	while(i<times  && !feof(fp)){
		fscanf(fp, "%c", &temp);
		if(temp=='P') {*(ID+i)=0; i++;}
		else if(temp=='C'){ *(ID+i)=1; i++;}
		else if(temp=='D'){ *(ID+i)=2;i++;}
		temp = '*';
	}

	if(i < times) { printf("Not enough values in %s \n", filename);
		printf("Contains only %d values, need %d\n", i, times);
		exit(1);}
	fclose(fp);
}

struct data {
	int total_delivered, total_supplied, total_taken;
	int on_kitchen, on_counter, blocked;
	int kitchen[3], counter[3];

	sem_t kit_avail; // counting semaphore
	sem_t kit_ready, count_ready; // Semaphore for kitchen ready  // binary
	sem_t kit_access, count_access, cook_ready, block;  // semaphore for accessing kitchen // binary

	sem_t stud_mut, table_mut;
	int stud_at_cou, table_avail, table;
} * shmp;

void init(){
	// create shared memory
	int shmfd = shm_open(SharedMemory, O_CREAT | O_RDWR,  0666);
	if(ftruncate(shmfd, size) == -1) {perror("ftruncate failure"); exit(0);}
	shmp = (struct data *) mmap(NULL, size, PROT_READ|PROT_WRITE, MAP_SHARED, shmfd, 0);

	// initialize shared memory
	shmp->total_supplied = shmp->total_delivered = shmp->total_taken = 0;
	shmp->on_kitchen = shmp->on_counter=0;
	for(int i=0;i<3;i++)
		shmp->kitchen[i] = shmp->counter[i] = 0;

	K = 2*L*M+1;
	shmp->blocked = 0;
	if(sem_init(&shmp->kit_avail, 1, K)==-1) {perror ("sem_init"); exit (1);}
	if(sem_init(&shmp->kit_access, 1, 1)==-1) {perror ("sem_init"); exit (1);}
	if(sem_init(&shmp->kit_ready, 1, 0)==-1) {perror ("sem_init"); exit (1);}

	if(sem_init(&shmp->cook_ready, 1, 1)==-1) {perror ("sem_init"); exit (1);}
	if(sem_init(&shmp->block, 1, 0)==-1) {perror ("sem_init"); exit (1);}
	if(sem_init(&shmp->count_access, 1, 1)==-1) {perror ("sem_init"); exit (1);}
	if(sem_init(&shmp->count_ready, 1, 0)==-1) {perror ("sem_init"); exit (1);}

	if(sem_init(&shmp->stud_mut, 1, 1)==-1) {perror ("sem_init"); exit (1);}
	if(sem_init(&shmp->table_mut, 1, T)==-1) {perror ("sem_init"); exit (1);}
	shmp->stud_at_cou = 0;
	shmp->table=0; // updated by stud_mut
	shmp->table_avail = T; // updated by stud_mut
}

int isFullMeal(){
	for(int c=0; c<3; c++){
		if(shmp->counter[c] <=0) return 0;
	}
	return 1;
}

int get_foodId(int critical){
	// if there is a element not on the counter get it
	if(critical)
	for(int c=0; c<3; c++)
		if(shmp->counter[c] == 0) return c;

	// else get any random food on kitchen
	srand(time(0));
	int ind;
	do {
		ind = rand()%3;
	} while(shmp->kitchen[ind] == 0);

	return ind;
}

void supplier(int id){
	int foodId = *(ID+id);

	sem_wait(&shmp->kit_avail);
	sem_wait(&shmp->kit_access);
		// Entering the kitchen
		printf("The supplier is going to the kitchen to deliver %s:   "
			"Kitchen items P:%d, C:%d, D:%d = %d\n",
			FOOD[foodId], shmp->kitchen[0], shmp->kitchen[1], shmp->kitchen[2], shmp->on_kitchen);

		// update
		shmp->on_kitchen++;
		shmp->kitchen[foodId]++;
		shmp->total_supplied++;

		// After Delivery
		printf("The supplier delivered %s - after delivery:  "
			"Kitchen items P:%d, C:%d, D:%d = %d\n",
			FOOD[foodId], shmp->kitchen[0], shmp->kitchen[1], shmp->kitchen[2], shmp->on_kitchen);

	sem_post(&shmp->kit_ready);
	sem_post(&shmp->kit_access);

}

void cook(int cookId){

	while(1){
		int foodId;

		// cook going to the kitchen
		sem_wait(&shmp->kit_ready);
		sem_wait(&shmp->kit_access);
			if(shmp->total_taken == 3*L*M){
				// After finishing placing all plates
				printf("Cook %d finished serving - items at kitchen: %d - "
					"going home - GOODBYE!!\n",
					cookId, shmp->on_kitchen);
				sem_post(&shmp->kit_ready);
				sem_post(&shmp->kit_access);
				sem_post(&shmp->count_ready);
				sem_post(&shmp->count_access);
				return;
			}

			else if(shmp->on_kitchen == 0  && shmp->total_supplied < 3*L*M){
				// release lock for supplier and wait for delivery;
				sem_post(&shmp->kit_access);
			}

			else {
				// there is some food on the kitchen,  so deliver it
				foodId = get_foodId(0);
				if (shmp->on_counter== S-1 || shmp->on_counter==S-2)
					foodId = get_foodId(1);

				if (shmp->kitchen[foodId] == 0){
					// release lock for supplier and wait for delivery;
					sem_post(&shmp->kit_access);
					continue;
				}

				// waiting for deliveries
				printf("cook %d is going to the kitchen to wait for a plate - "
					"Kitchen items P:%d, C:%d, D:%d = %d\n",
					cookId, shmp->kitchen[0], shmp->kitchen[1], shmp->kitchen[2], shmp->on_kitchen);

					// update
					shmp->on_kitchen--;
					shmp->kitchen[foodId]--;
					shmp->total_taken++;

				// check if it was the last delivery
				// if it was release the kit_Ready lock as supplier wont be there
				if(shmp->total_taken == 3*L*M)
					sem_post(&shmp->kit_ready);

				sem_post(&shmp->kit_avail); // when food is taken from kitchen increase the available counter
				sem_post(&shmp->kit_access); // always release the lock, to be used by supplier if any


				// only cooks with food can do this
				// cook going to the counter

				sem_wait(&shmp->cook_ready);
				sem_wait(&shmp->count_access);
					if(shmp->on_counter == S){
						shmp->blocked = 1;
						sem_post(&shmp->count_ready);
						sem_post(&shmp->count_access);
						sem_wait(&shmp->block); // the student can remove this
					}

					// Going to the counter
					printf("Cook %d is going to the counter to deliver %s - "
						"Counter items P:%d, C:%d, D:%d = %d\n",
						cookId, FOOD[foodId], shmp->counter[0], shmp->counter[1], shmp->counter[2], shmp->on_counter);

					// update counter
					shmp->on_counter++;
					shmp->counter[foodId]++;
					shmp->total_delivered++;

					// After Delivery
					printf("Cook %d placed %s on the counter -  "
						"Counter items P:%d, C:%d, D:%d = %d\n",
						cookId, FOOD[foodId], shmp->counter[0], shmp->counter[1], shmp->counter[2], shmp->on_counter);

				sem_post(&shmp->count_ready);
				sem_post(&shmp->count_access);
				sem_post(&shmp->cook_ready);


			} // else
	}
}

void student(int id){
	int Round=1;
	int done[L+1];
	for(int z=0; z<L+1;z++) done[z]=0;
	while(Round <= L){
		sem_wait(&shmp->count_ready);
		sem_wait(&shmp->count_access);
			if(!done[Round]){
				// Arrving at the counter for food
				sem_wait(&shmp->stud_mut);
					shmp->stud_at_cou++;
				printf("Student %d is going to the counter (round %d) - "
					"# of students at counter: %d and "
					"Counter items P:%d, C:%d, D:%d = %d\n",
					id, Round, shmp->stud_at_cou, shmp->counter[0], shmp->counter[1], shmp->counter[2], shmp->on_counter);
				sem_post(&shmp->stud_mut);
				done[Round]=1;
			}

			if( isFullMeal() ) {
				// update counter
				shmp->on_counter-=3;
				for(int c=0; c<3; c++) shmp->counter[c]--;

				sem_wait(&shmp->table_mut);
					sem_wait(&shmp->stud_mut);

						// waiting to get a table
						printf("Student %d got food and is going to get a table (round %d)"
							" -  # of empty tables: %d\n",
							id, Round, shmp->table_avail);
							shmp->stud_at_cou--;
					sem_post(&shmp->stud_mut);

					int mytable;
					sem_wait(&shmp->stud_mut);
						shmp->table++; mytable = shmp->table;
						shmp->table_avail -= 1;

						// sitting to eat
						printf("Student %d sat at table %d to eat  (round %d)"
							" - empty tables %d\n", id, mytable, Round, shmp->table_avail);

						shmp->table_avail++;
						shmp->table %= T;
					sem_post(&shmp->stud_mut);

					if (Round!=L){
						sem_wait(&shmp->stud_mut);
						// done eating, going to the counter again
						printf("Student %d left table %d to eat again (round %d)"
							" -  empty tables: %d\n",
							id, mytable, Round+1, shmp->table_avail);
						sem_post(&shmp->stud_mut);
					}
				sem_post(&shmp->table_mut);
				Round++;
			}

		if(shmp->total_delivered == 3*L*M) {sem_post(&shmp->count_ready);
			printf("Counter items P:%d, C:%d, D:%d = %d\n",
				shmp->counter[0], shmp->counter[1], shmp->counter[2], shmp->on_counter);
			}
		if (shmp->blocked == 1) sem_post(&shmp->block); // release block

		sem_post(&shmp->count_access);
	}
}

void finish(){
	shm_unlink(SharedMemory); // Unlink the shared memory
	if(sem_destroy(&shmp->kit_avail) == -1) {perror("sem_destroy"); exit(1);}
	if(sem_destroy(&shmp->kit_access) == -1) {perror("sem_destroy"); exit(1);}
	if(sem_destroy(&shmp->kit_ready) == -1) {perror("sem_destroy"); exit(1);}
	if(sem_destroy(&shmp->count_ready) == -1) {perror("sem_destroy"); exit(1);}
	if(sem_destroy(&shmp->count_access) == -1) {perror("sem_destroy"); exit(1);}
	if(sem_destroy(&shmp->cook_ready) == -1) {perror("sem_destroy"); exit(1);}
	if(sem_destroy(&shmp->block) == -1) {perror("sem_destroy"); exit(1);}
	if(sem_destroy(&shmp->stud_mut) == -1) {perror("sem_destroy"); exit(1);}
	if(sem_destroy(&shmp->table_mut) == -1) {perror("sem_destroy"); exit(1);}
}

void handler(){
	printf("\nSignal Ctrl + C Caught\n");
	printf("freeing memory created on heap\n"); free(ID);
	finish();
	printf("bye\n");
}

int main(int argc, char *argv[]){
	signal(SIGINT, handler);
	pid_t pid;
	struct Parsed values = parseCommandLine(argc, argv);
	readfile(values.filename, 3*L*M);   // it works properly and puts food id in ID
	init(); // init after parsing to fix k = 2*l*m+1

	pid = fork();
	if (pid < 0 ){
		perror("forking failed for process 2"); exit(0);
	}
	else if(pid == 0){
		// child process
		for(int i=0; i<3*L*M; i++)
			supplier(i);

		// Done Delivering
		printf("The supplier finished supplying - GOODBYE!\n");
		exit(0);
	}
	else{
		signal(SIGCHLD, SIG_IGN);
	}

	for(int cookId=0; cookId<N; cookId++){
		pid = fork();
		if (pid < 0 ){
			perror("forking failed for process cook"); exit(0);
		}
		else if(pid == 0){
			cook(cookId);
			exit(0);
		}
		else {
			signal(SIGCHLD, SIG_IGN);
		}
	}

	for(int studentId=0; studentId<M; studentId++){
		pid = fork();
		if (pid < 0 ){
			perror("forking failed for process 2"); exit(0);
		}
		else if(pid == 0){
			student(studentId);
			printf("Student %d is done eating L=%d times"
				" -  going home -  GOODBYE!!!\n",
				studentId, L);

			exit(0);
		}

		else {
			signal(SIGCHLD, SIG_IGN);
		}
	}

	int status;
	for(int j=0; j< M+N+1; j++)
		wait(&status);

	finish();

return 0;
}
