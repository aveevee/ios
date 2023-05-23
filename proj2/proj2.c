//Aidan Urmanbetov
//<xurman00>

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>
#include <semaphore.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <time.h>

#define MMAP(pointer) {(pointer) = mmap(NULL, sizeof(*(pointer)), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);}
#define MUNMAP(pointer) {munmap((pointer), sizeof(*(pointer)));} 

int client_num;            //NZ: počet zákazníků
int clerk_num;            //NU: počet úředníků
int max_client_time;       //TZ: Maximální čas v milisekundách, po který zákazník po svém vytvoření čeká, než vejde na poštu (eventuálně odchází s nepořízenou). 0<=TZ<=10000
int max_clerk_time;       //TU: Maximální délka přestávky úředníka v milisekundách. 0<=TU<=100
int max_post_time;         //F: Maximální čas v milisekundách, po kterém je uzavřena pošta pro nově příchozí. 0<=F<=10000

int *counter = NULL;    
int *client_id = NULL; 
int *clerk_id = NULL;
int *post_close = NULL; 
int *num_fronta = NULL; 
sem_t *fronta0 = NULL;
sem_t *fronta1 = NULL;
sem_t *fronta2 = NULL;
sem_t *sem_write = NULL; 
FILE *output_file = NULL;

//A function that checks the correctness of arguments
void check_arguments(int argc, char* argv[]){
    if(argc != 6){
        fprintf(stderr, "%s", "Wrong num of arguments\n");
        exit(1);
    }
    for(int i = 1; i < argc; ++i){
        int length = strlen(argv[i]);
        for(int j = 0; j < length; j++){
            if(argv[i][j] > '9' || argv[i][j] < '0'){
                fprintf(stderr, "%s", "Wrong format of input\n");
                exit(1);
            }
        }        
    }
    client_num = atoi(argv[1]);
    clerk_num = atoi(argv[2]);
    max_client_time = atoi(argv[3]);
    max_clerk_time = atoi(argv[4]);
    max_post_time = atoi(argv[5]);
    if((client_num < 1) || (clerk_num < 1) || (max_client_time < 0 || max_client_time > 10000) || 
    (max_clerk_time < 0 || max_clerk_time > 100) || (max_post_time < 0 || max_post_time > 10000)){
        fprintf(stderr, "%s", "Wrong format of input diapason\n");
        exit(1);
    }
}

//Open file
int open_file(){
    output_file = fopen("proj2.out", "w");
    if (output_file == NULL)
    {
        fprintf(stderr, "%s", "File cannot open");
        return 1;
    }
    setbuf(output_file, 0);
    return 0;
}

//A function that creates shared memory
int create(){
    MMAP(counter);
    MMAP(client_id);
    MMAP(clerk_id);
    MMAP(post_close);
    MMAP(fronta0);
    MMAP(fronta1);
    MMAP(fronta2);
    num_fronta = mmap(NULL, 3 * sizeof(int), PROT_READ| PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    num_fronta[0] = 0;
    num_fronta[1] = 0;
    num_fronta[2] = 0;
    MMAP(sem_write);
    if((counter == MAP_FAILED) || (post_close == MAP_FAILED) || 
    (fronta0 == MAP_FAILED) || (fronta1 == MAP_FAILED) ||
    (fronta2 == MAP_FAILED) || (num_fronta == MAP_FAILED) || 
    (client_id == MAP_FAILED) || (clerk_id == MAP_FAILED) ||  (sem_write == MAP_FAILED)){
        return 1;
    }
    return 0;
}

//The function that frees memory
void delete(){
    MUNMAP(counter);
    MUNMAP(client_id);
    MUNMAP(post_close);
    MUNMAP(clerk_id);
    MUNMAP(sem_write);
    MUNMAP(fronta0);
    MUNMAP(fronta1);
    MUNMAP(fronta2);
    munmap(num_fronta, 3* sizeof(int));
}

//A function that initialize semaphores
int init_sem(){
    if(((sem_init(fronta0, 1, 0) == -1) || (sem_init(fronta1, 1, 0) == -1) || (sem_init(fronta2, 1, 0) == -1) ||  (sem_init(sem_write, 1 , 1) == -1))){
        return 1;
    } 
    return 0;
}

//A function that destroys semaphores
void destroy_sem(){
    sem_destroy(sem_write);
    sem_destroy(fronta0);
    sem_destroy(fronta1);
    sem_destroy(fronta2);
}

//Process Client
void client_process(int *id_client){
    int idZ = 0;

    sem_wait(sem_write);
    ++(*id_client);
    idZ = *id_client;
    fprintf(output_file, "%d: Z %d: started\n", ++(*counter), idZ); //print start after startup
    sem_post(sem_write);

    srand(time(NULL) * getpid()); 
    usleep((rand() % (max_client_time)) * 1000); 
    
    //if post is close, client goes home
    if(*post_close == 1){
        sem_wait(sem_write);
        fprintf(output_file, "%d: Z %d: going home\n", ++(*counter), idZ);
        sem_post(sem_write);
        return;
    }
    int service_num = (rand() % 3) + 1; //get random number of service <1,3>
        
    sem_wait(sem_write);
    fprintf(output_file, "%d: Z %d: entering office for a service %d\n", ++(*counter), idZ, service_num);
    sem_post(sem_write);
            
    num_fronta[service_num-1] += 1; 

    //closes the semaphore of the selected service number
    if ((service_num-1 == 0)!= 0)
    {
        sem_wait(fronta0);
    }else if ((service_num-1 == 1) != 0)
    {
        sem_wait(fronta1);
    }else if ((service_num-1 == 2) != 0)
    {
        sem_wait(fronta2);
    }

    sem_wait(sem_write);
    fprintf(output_file, "%d: Z %d: called by office worker\n", ++(*counter), idZ);
    sem_post(sem_write);

    usleep((rand() % 10)*1000); //synchronization with the clerk to complete the request

    sem_wait(sem_write);
    fprintf(output_file, "%d: Z %d: going home\n", ++(*counter), idZ);
    sem_post(sem_write);        
}

//Process clerk
void clerk_process(int *id_clerk){
    int idU=0;

    sem_wait(sem_write);
    ++(*id_clerk);
    idU=*id_clerk;
    fprintf(output_file, "%d: U %d: started\n", ++(*counter), idU); //print start after startup
    sem_post(sem_write);

    srand(time(NULL) * getpid());

    while (true)
    {
        bool found = false;
        int type; 

        //A loop that checks if there are clients in the queue
        for (int i = 0; i < 3; i++)
        {  
            if(num_fronta[i] != 0){
                found = true;
                type = i;
            }
        }   
        
        //if post office close and no one in the queue prints "going home"
        if(*post_close == 1 && !found){
            sem_wait(sem_write);
            fprintf(output_file, "%d: U %d: going home\n", ++(*counter), idU);
            sem_post(sem_write);
            break;
        }else if(*post_close == 0 &&  !found){ //if post office open, but no one in the queue
            sem_wait(sem_write);
            fprintf(output_file, "%d: U %d: taking break\n", ++(*counter), idU);
            sem_post(sem_write);

            usleep((rand() % (max_clerk_time)) * 1000);

            sem_wait(sem_write);
            fprintf(output_file, "%d: U %d: break finished\n", ++(*counter), idU);
            sem_post(sem_write);  

            continue;
        }else if (found) //if there is someone in line
        {   
            sem_wait(sem_write);
            fprintf(output_file, "%d: U %d: serving a service of type %d\n", ++(*counter), idU, type+1);

            if (type == 0)
            {
                sem_post(fronta0);
            }else if (type == 1)
            {
                sem_post(fronta1);
            }else if (type == 2)
            {
                sem_post(fronta2);
            }

            sem_post(sem_write);

            num_fronta[type] -= 1;

            usleep((rand() % 10)*1000);//synchronization with the client

            sem_wait(sem_write);
            fprintf(output_file, "%d: U %d: service finished\n", ++(*counter), idU);
            sem_post(sem_write);
        }   
    }
}


int main(int argc, char* argv[]){
    clerk_id = 0; 
    counter = 0; 
    client_id  = 0; 
    post_close = 0;

    pid_t client_pid;
    pid_t clerk_pid;

    //check if the arguments are right
    check_arguments(argc, argv);
    if(open_file() != 0){
        fprintf(stderr, "%s", "Cannot open file\n");
        return 1;
    }

    //mmap shared memory
    if(create() != 0){
        fprintf(stderr, "%s", "mmap error!\n");
        delete();
        return 1;
    }

    //initialize semaphores
    if(init_sem() != 0){
        fprintf(stderr, "%s", "Cannot initialize semaphores\n");
        destroy_sem();
        delete();
        return 1;
    }

    //create client process
    for (int i = 0; i < client_num; i++)
    {
        client_pid = fork();
        if(client_pid == -1)
        {
            fprintf(stderr, "%s", "Error: fork\n");
            return 1;
        }
        else if (client_pid == 0)
        {
            client_process(client_id);
            exit(0);
        }
    }

    //create clerk process
    for (int i = 0; i < clerk_num; i++)
    {
        clerk_pid = fork();
        if(clerk_pid == -1)
        {
            fprintf(stderr, "%s", "Error: fork\n");
            return 1;
        }
        else if (clerk_pid == 0)
        {
            clerk_process(clerk_id);
            exit(0);
        }
    }

    usleep((rand() % (max_post_time/2 + 1) + (max_post_time/2)) * 1000);

    //Main process
    sem_wait(sem_write);
    fprintf(output_file, "%d: closing\n", ++(*counter));
    *post_close = 1;
    sem_post(sem_write);

    waitpid(client_pid, NULL, 0);
    waitpid(clerk_pid, NULL, 0);

    //checks that main process is done
    for (int i = 0; i < client_num + clerk_num + 1; i++){
        wait(NULL);
    }

    fclose(output_file);
    destroy_sem(); //destroy semaphores
    delete(); //free memory
}