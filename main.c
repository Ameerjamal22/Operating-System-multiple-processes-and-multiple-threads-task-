#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h> // for the fork function and pipes
#include <sys/wait.h> // for the wait and other functions
#include <errno.h> // for the fifo and multiple processes waiting
#include <sys/stat.h> //for the fifo
#include <sys/types.h>// for the fifo
#include <fcntl.h> // to open the fifo .
#include <time.h>
#include <sys/time.h>
#include <pthread.h>

#define SIZE 100

int matrixA[SIZE][SIZE] ; // generated via student id (id array)
int matrixB[SIZE][SIZE] ; // generated via student id and birth year (idb array)
int resultMatrix_naiveeApproach[SIZE][SIZE] ;
int resultMatrix_multiProcessesApproach[SIZE][SIZE] ;
int resultMatrix_multiJoinableThreadsApproach[SIZE][SIZE] ;
int resultMatrix_multiDetachedThreadsApproach[SIZE][SIZE] ;

int id[] = { 1 , 2 , 1 , 1 , 2 , 2 , 6 } ;
int idb[] = { 1 , 2 , 1 , 1 , 2 , 2 , 6 , 2 , 0 , 0 , 3 } ;



// genrate matrcies .. done
// matrix multiplication naivee way ... done
// matrix multiplication using multiprocesses .. done




void printMatrix( ){

    for (int i = 0 ; i < SIZE ; i ++ ){
        for(int j = 0 ; j < SIZE ; j ++ ){
            printf("%d " , resultMatrix_multiJoinableThreadsApproach[i][j]) ;
        }
        printf("\n") ;
    }

    for (int i = 0 ; i < SIZE ; i ++ ){
        for(int j = 0 ; j < SIZE ; j ++ ){
            printf("%d " , resultMatrix_naiveeApproach[i][j]) ;
        }
        printf("\n") ;
    }

    for (int i = 0 ; i < SIZE ; i ++ ){
        for(int j = 0 ; j < SIZE ; j ++ ){
            printf("%d " , resultMatrix_multiProcessesApproach[i][j]) ;
        }
        printf("\n") ;
    }

}


void childProcessHandler( int i , int numberofprocesses , int ChildRowShare , int parentToChild_pipes[][2] , int childToParent_pipes[][2] ){

    // child process code : .........................

    /*
        the child process number x reads from the parent child reads the range of rows that it have to evaluate
        from the parent -> child pipe number x . so other pipes ends will be closed and the write end of parent -> child pipe number x will be closed
        after reading the range of rows child process will evaluate the result rows after performing matrix multiplication for a certain chunk of rows of the first matrix
        then the child process will write the range on the child -> parent pipe number x , so the read end of this pipe will be closed and the other pipes except x will
        be closed from both ends in the child x then after writing the range , the child x writes the resulting rows one by one to the child parent pipe , and then after
        finishing writing the resulting rows to the pipes the child process x will terminate with zero exit code .
    */


    // closing the unecessary pipes in child process number (i) .
    for (int j = 0 ; j < numberofprocesses ; j ++ ){

        if ( j != i ){
            close( parentToChild_pipes[j][0]) ;
            close( parentToChild_pipes[j][1]) ;
            close( childToParent_pipes[j][0]) ;
            close( childToParent_pipes[j][1]) ;
        }

    }
    close(parentToChild_pipes[i][1]) ; // closing the parent -> child write pipe end since child dont need to write on it .
    close(childToParent_pipes[i][0]) ; // closing the child -> parent read pipe end since the we dont need to read from it .

    int childProcessRange[2] ; // creating array of two numbers to hold the range we read from the pipe .

    // reading the range from the parent -> child (i) pipe and checking for errors .
    if ( read( parentToChild_pipes[i][0] , childProcessRange , sizeof(int) * 2) == -1 ){
        printf("there was an error while reading the range in the %d child process \n" , i ) ; // displaying an error message if reading from pipes failed .
    }

    close(parentToChild_pipes[i][0]) ; // closing the read end of the parent -> child (i) pipe after reading is done .

    // since we need to divide the range into the child processes the last child process will take the extra remaining rows .
    if ( i == numberofprocesses - 1 ){
        childProcessRange[1] = SIZE ; // adjusting the end of the last child process to be the last row .
    }


    /*
        multiplies a certain range of first matrix rows with all the columns of the second matrix to get a certain range of rows from the result matrix .
    */
    for (int l = childProcessRange[0] ; l < childProcessRange[1] ; l ++ ){

        for (int j = 0 ; j < SIZE ; j ++ ){

            resultMatrix_multiProcessesApproach[l][j] = 0 ;

            for (int k = 0 ; k < SIZE ; k ++ ){

                resultMatrix_multiProcessesApproach[l][j] += matrixA[l][k] * matrixB[k][j] ;
                //printf( " %d \n"  , resultMatrix_multiProcessesApproach[l][j] ) ;
            }

        }
    }

    /*
        writing the range of result matrix rows that the child process computed
        to the child -> parent pipe number (i) and cheking for errors afterwards .
    */
    if ( write( childToParent_pipes[i][1] , childProcessRange , sizeof(int) * 2 ) == -1 ){
        printf("there was an error while the child process %d was writing the range on the pipe\n" , i + 1 ) ; // displaying an error message if some an error occured .
    }

    /*
        writing the range of result rows one by one on the child -> parent pipe (i) .
    */
    for (int l = childProcessRange[0] ; l < childProcessRange[1] ; l ++ ){

        if ( write(childToParent_pipes[i][1] , &resultMatrix_multiProcessesApproach[l] , sizeof(int) * SIZE  ) == -1 ){
            printf("there was an error while the %d child process was writing the %d row of the matrix to the pipe \n", i + 1 , l + 1 ) ; // dsiplaying an error message if an error has occured while writing rows
        }

    }
    close( childToParent_pipes[i][1]) ; // closing the child -> parent (i) write end after the writing operations are done .

    return ;

}





#define NUMBER_OF_THREADS 4 // number of threads that will be used to multiply the matrix .

int threadShare = SIZE / NUMBER_OF_THREADS ; // computing the share of rows that each thread has to work on based on the size of the matrix and the number of threads .

/*
    child joinable thread function that for each thread number (i) will compute the range that each child thread want to process
    based on the share of every thread in its index i the last thread will be responsible of processing the remaining rows .
*/
void *ChildThread( int i ){

    int threadRangeOfRows[2] ; // defining an array of two integer that will store the range of rows which each thread has to process .

    threadRangeOfRows[0] = ( i * threadShare ) ; // computing the start of the range .
    threadRangeOfRows[1] = ( (i + 1) * threadShare ) ;// computing the end of the range .

    // extending the range of the last thread to cover all the matrix rows .
    if ( i == NUMBER_OF_THREADS - 1 ){
        threadRangeOfRows[1] = SIZE ;
    }

    /*
        iterating through the rows that the thread is resposible of multplying and mutliplying each row of them by all the columns of the second matrix .
    */
    for (int l = threadRangeOfRows[0] ; l < threadRangeOfRows[1] ; l ++ ){

        for (int j = 0 ; j < SIZE ; j ++ ){

            resultMatrix_multiJoinableThreadsApproach[l][j] = 0 ; // initializing the current result cell of zero in order to be able to increment it .

            for (int k = 0 ; k < SIZE ; k ++ ){

                resultMatrix_multiJoinableThreadsApproach[l][j] += matrixA[l][k] * matrixB[k][j] ;

            }

        }
    }



}

/*
    to compute the time of execution for the detached threads we want to declare the tools of calculating the time globally
    so we will be able to use them in the child detached thread function since all of the threads have the shared adress space of the row
    then when we enter the first detached thread we will take the start time of the execution and after each thread finish processing the matrix
    by taking the end time of execution the maximum number of them will be the time of execution for the detached threads approach .
*/

// used to calculate the time .
struct timeval start4, end4 ;
double time_taken4 ;

/*
    the detached thread function is the same as the joinable thread function except the extra things to compute the time which is
    explained above .
*/
void *ChildDetachedThread( int i ){

    double noOfOpsDoneByThread = 4 ; // to store the number of operations done by each thread .
    int threadRangeOfRows[2] ;

    threadRangeOfRows[0] = ( i * threadShare ) ;
    threadRangeOfRows[1] = ( (i + 1) * threadShare ) ;

    if ( i == NUMBER_OF_THREADS - 1 ){
        threadRangeOfRows[1] = SIZE ;
    }

    for (int l = threadRangeOfRows[0] ; l < threadRangeOfRows[1] ; l ++ ){

        for (int j = 0 ; j < SIZE ; j ++ ){

            resultMatrix_multiDetachedThreadsApproach[l][j] = 0 ;

            for (int k = 0 ; k < SIZE ; k ++ ){

                resultMatrix_multiDetachedThreadsApproach[l][j] += matrixA[l][k] * matrixB[k][j] ;
                noOfOpsDoneByThread += 2 ;

            }

        }
    }



        gettimeofday(&end4, NULL); // getting the end of execution time for each detached thread to compute the time .

        time_taken4 = (end4.tv_sec - start4.tv_sec) * 1e6;
        time_taken4 = (time_taken4 + (end4.tv_usec - start4.tv_usec)) * 1e-6;

        printf("this is the time taken by thread %d:%.6f\n" , i + 1 , NUMBER_OF_THREADS, time_taken4 ) ;// displaying the time that took the thread to finish processing (time after first thread was created ) .



}


int main(int argc , char* argv[] )
{

    /*
        in the generation of the first matrix an id array is defined globally , and we use counter
        and moduls to know which number we want to fill the matrix cell with , and the same thing happen
        for matrix b except we add to the array the birth year .
    */

    int cnt = 0 ;

    for (int i = 0 ; i < SIZE ; i ++ ){
        for (int j = 0 ; j < SIZE ; j ++ ){

            matrixA[i][j] = id[(cnt % 7)] ;
            matrixB[i][j] = idb[(cnt % 11)] ;
            cnt ++ ;
        }
    }

// Naivee approach : ...................................................................................................................................................................................................................


    // used to calculate the time that the naivee approach takes until it finsishes multiplying the matrices . the time here is the time elapsed that includes scheduling and waiting in ready queue
    struct timeval start1, end1;
    setbuf(stdout, NULL);
    gettimeofday(&start1, NULL) ; // gets the start time



    /*
        multiplication via naivee approach that uses one process that iterates that multiply
        each row with each cloumn which takes o(n^3) time complexity .
    */

    double numberOfBasicOperations = 0 ;

    for (int i = 0 ; i < SIZE ; i ++ ){
        for (int j = 0 ; j < SIZE ; j ++ ){

            resultMatrix_naiveeApproach[i][j] = 0 ;
            // the loop that traverses rows of the first matrix and columns of the second matrix
            for( int k = 0 ; k < SIZE ; k ++ ){

                resultMatrix_naiveeApproach[i][j] += matrixA[i][k] * matrixB[k][j] ;
                numberOfBasicOperations += 2 ;
                // here to measure the throughput every basic operation is counted , the throughput will be number of basic opertions per second .
            }


        }

    }



    gettimeofday(&end1, NULL); // gets the end time .

    // computing the time that the naivee approach took to multiplty two matrices with 6 digits after floating point to demonstrate what approach is better later on .
    double time_taken_naiveeApproach ;
    time_taken_naiveeApproach = (end1.tv_sec - start1.tv_sec) * 1e6;
    time_taken_naiveeApproach = (time_taken_naiveeApproach + (end1.tv_usec - start1.tv_usec)) * 1e-6;
    printf("Time taken using 1 process naivee approach: %.6f sec\n", time_taken_naiveeApproach);
    printf("Thoughput:number of basic operations :%.2f\n\n\n" , numberOfBasicOperations/time_taken_naiveeApproach ) ;



// Multiprocesses approach :......................................................................................................................................................................................................................


    // used to calculate the time that multiprocesses approach takes to finish mutliplting matrices , includes waiting time and everything .
    struct timeval start2, end2;
    setbuf(stdout, NULL);
    gettimeofday(&start2, NULL) ; // taking start time .



    // the same number of operations that are used in the naivee approach will be used here , except the operations are divided among processes .
    int numberofprocesses = 4 ; // variable number of procceses the value of it how many processes in multiplying matrices .
    int parentId = getpid() ; // getting the parent id before forking to help to distenguishing betweeen the parent and child processes .

    int parentToChild_pipes[numberofprocesses][2] ; // defining parent -> child (#number of processes) pipes that will be used to deliver the range to every child process from the parent process .
    int childToParent_pipes[numberofprocesses][2] ; // defining child -> parent ($number of processes) pipes that will be used to deliver the range and the result rows after each child process finishes multiplying its part

    for (int i = 0 ; i < numberofprocesses ; i ++ ){

        // creating the pipes one by one and displying an error message if an error has occured .
        if ( pipe( parentToChild_pipes[i] )  == -1 ){
            printf("there was an error while creating the %d pipe\n" , i + 1 ) ;
        }
        if ( pipe( childToParent_pipes[i] ) == -1 ){
            printf("there was an error while creating the %d pipe\n" , i + 1 ) ;
        }

    }

    int ChildRowShare = SIZE / numberofprocesses ; // computing the number of result matrix rows that each child process have to evaluate based on the number of child processes

    int childProcessIDs[numberofprocesses] ; // array of ids to fork the child processes .

    for (int i = 0 ; i < numberofprocesses ; i ++ ){

        // creating the child processes and making sure that all of them are childs of the parent process using the parent id .
        if ( getpid() == parentId ){

            childProcessIDs[i] = fork() ; // forking child process number (i) .
            // checking if an error has occured while forking the (i) child process , and if so and error message will be displayed .
            if ( childProcessIDs[i] == -1){
                printf("there was an error while forking the %d child process \n" , i + 1 ) ;
            }

        }
        if ( childProcessIDs[i] == 0 ){

            // child process area ...................
            childProcessHandler(i , numberofprocesses , ChildRowShare , parentToChild_pipes , childToParent_pipes ) ; // calling the child process function that will perform the task of it .
            return 0 ;// child process terminates here .

        }

    }

    // Parent process area .................

    /*
        the parent process computes the range of rows that each child process is resposible for multiplying and writes the range for the (ith) process on the
        parent -> child (ith) pipe , and it also closes both end of the pipe in that loop .
    */
    for (int i = 0 ; i < numberofprocesses ; i ++ ){

        close(parentToChild_pipes[i][0]) ; // closing the read end of (ith) parent -> child pipe since the parent does not read anything from that pipe .

        int currentRange[2] = { (i * ChildRowShare ) , ( (i + 1) * ChildRowShare ) } ; // defining the range for the (ith) child process .

        // writing the range on the parent -> child pipe and checking for errors .
        if ( write( parentToChild_pipes[i][1] , currentRange , sizeof(int) * 2 ) == -1 ){
            printf("there was an error while the parent was writing range on the pipe of the %d child process\n" , i + 1 ) ; // displaying an error message if there is an error in writing on the pipe end .
        }
        close(parentToChild_pipes[i][1]) ; // closing the write end of the (ith) child process parent -> child pipe .


    }

    /*
        after the parent process sent the ranges for each child process it iterates to collect the results from each child process .
        so the parent first read the range of each child through child -> parent (ith) pipe read end , and then after reading the ranges
        the parent reads the resulting rows of each range one by one and fills the result matrix .
    */
    for (int i = 0 ; i < numberofprocesses ; i ++ ){

        close(childToParent_pipes[i][1]) ; // closing the write end of the pipe since the direction is child -> parent .

        int currentRange[2] ; // defning the array of 2 integers that will hold the range of processed rows by each child process .

        // reading the range for the (ith child) from the read end of the child -> parent pipe number (i) , and checking for errors .
        if ( read( childToParent_pipes[i][0] , currentRange , sizeof(int) * 2 ) == -1 ){
            printf("there was an error while the parent process was reading the range from the %d pipe \n" , i + 1 ) ; // displaying an error message if there was an error while reading from the ith pipe
        }

        /*
            iterates through the range which the parent read before and reading the resulting rows one by one from the child -> parent (ith) pipe read end .
            then the rows are filled one by one in the result matrix of the main process .
        */
        for (int j = currentRange[0] ; j < currentRange[1] ; j ++ ){

            int tempRow[SIZE] ; // defines a temporary array to hold each row after reading it from the pipes .

            // reading the jth row from the ith child -> parent pipe read end , and checking for errors .
            if ( read( childToParent_pipes[i][0] , tempRow , sizeof(int) * SIZE ) == -1 ){
                printf("there was an error while the parent while reading the %d row from the %d process \n" , j + 1 , i + 1 ) ;
            }

            // filling the result matrix with the row read from the pipe .
            for (int k = 0 ; k < SIZE ; k ++ ){

                resultMatrix_multiProcessesApproach[j][k] = tempRow[k] ;

            }

        }

        close(childToParent_pipes[i][0]) ; // closing the child parent ith pipe read end after reading all range rows from it .

    }


    while( wait(NULL) && errno != ECHILD ){} // the parent wait for all the child processes to terminate .




    gettimeofday(&end2, NULL); // gets the finish time .

    double time_taken;

    // computing the time it took for this approach to multiply the matrices .
    time_taken = (end2.tv_sec - start2.tv_sec) * 1e6;
    time_taken = (time_taken + (end2.tv_usec - start2.tv_usec)) * 1e-6;
    printf("Time taken using %d child processes: %.6f sec\n", numberofprocesses ,time_taken);
    printf("Thoughput:number of basic operations (addition , multiplication )  per sec (using %d processes):%.2f\n\n\n" ,numberofprocesses ,  numberOfBasicOperations/time_taken ) ;

    //printMatrix() ;

// Multi Joinable threads approach : ...........................................................................................................................................................................................


    // used for computing the time .
    struct timeval start3, end3;
    setbuf(stdout, NULL);
    gettimeofday(&start3, NULL) ; // gets the start time


    // the same number of operations that naivee approach do is done here but the operations are divided among threads and since we can know when all the joinable threads end we can calculate it as a hole .
    pthread_t thread[NUMBER_OF_THREADS] ; //  creating the array that will store the joinable threads .

    /*
        creating the joinable threads one by one and passing the number of the thread to the thread function , which will
        help the thread to know the part that its responsible for multiplying  .
    */
    for (int i = 0 ; i < NUMBER_OF_THREADS ; i ++ ){

        // creating the thread number (i) and checking for errors .
        if ( pthread_create( &thread[i] , NULL , &ChildThread , i ) != 0 ){
            printf("there was an error while creating the %d thread \n" , i + 1 ) ;// displaying an error message if an error has occured .
        }

    }

    /*
        joining the joinable threads in the main thread (waiting for them to finish execution )
    */
    for (int i = 0 ; i < NUMBER_OF_THREADS ; i ++ ){

        // joining the thread number (i) and checking for errors . (waiting for the thread number (i) to finish ) .
        if ( pthread_join(thread[i] , NULL ) != 0 ){
            printf("something wrong happened while waiting for %d thread " , i + 1 ) ; // displaying an error message if an error has occured while waiting for the ith thread to finish .
        }

    }




    gettimeofday(&end3, NULL); // gets the finish time

    double time_taken3; // a variable to store the time of execution for the joinable threads approach .
    // computing the time it took for this approach to multiply the matrices .
    time_taken3 = (end3.tv_sec - start3.tv_sec) * 1e6;
    time_taken3 = (time_taken3 + (end3.tv_usec - start3.tv_usec)) * 1e-6;
    printf("Time taken using %d threads : %.6f sec\n", NUMBER_OF_THREADS ,time_taken3 );
    printf("Thoughput:number of basic operations (addition , multiplication )  per sec (using %d processes):%.2f\n\n\n" ,NUMBER_OF_THREADS , numberOfBasicOperations/time_taken3 ) ;
    //printMatrix() ;



// multi detached threads approach : ...............................................................................................................................

    setbuf(stdout, NULL);
    gettimeofday(&start4, NULL) ; // taking start time .

    // tools from the pthread library to create the detached threads .
    pthread_attr_t detachedThread ;

    pthread_attr_init(&detachedThread ) ;
    pthread_attr_setdetachstate(&detachedThread , PTHREAD_CREATE_DETACHED ) ;

    pthread_t detached_thread[NUMBER_OF_THREADS] ; // defining the array of detached threads .

    // here for the detached threads we will measure the throughput for each thread since we cant know their when they will finish in the main thread .

    /*
        creating the detached threads one by one by passing the address of the tool used to create detached threads for them and also passing the number
        of the thread as a parameter that will help the thread to know it part of computation .
    */
    for (int i = 0 ; i < NUMBER_OF_THREADS ; i ++ ){

        // creating the detached thread number i and checking for errors .
        if ( pthread_create( &detached_thread[i] ,&detachedThread , &ChildDetachedThread , i ) != 0 ){
            printf("there was an error while creating the %d thread \n" , i + 1 ) ; // displaying and error message if an error occured while creating the i thread .
        }

    }

    pthread_attr_destroy(&detachedThread) ; // destroying the tool used to create detached threads after finishing using it  .

    pthread_exit(0) ;// using pthread exit that waits  all the thread created by the process to finish before exiting the program (even if they are detached) .




}
