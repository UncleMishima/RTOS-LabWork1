#include <cstdlib>
#include <iostream>
#include <pthread.h>
#include <string>
#include <stdio.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>


#define NUM_OF_THREADS 10
#define NUM_OF_ALL_THREADS NUM_OF_THREADS + 1 // + 1 for main thread


using namespace std;


pthread_barrier_t barrier;


struct keygen_params
{
    size_t a;
    size_t c;
    size_t m;
    unsigned char seed;
    size_t keySize;
};


struct crypt_params
{
    unsigned char* msg;
    unsigned char* key;
    unsigned char* outputText;
    size_t size;
    size_t downIndex;
    size_t  topIndex;
};


void* keygen(void* arg)
{
    keygen_params* param = reinterpret_cast<keygen_params* >(arg);
    size_t a = param->a;
    size_t m = param->m;
    size_t c = param->c;
    size_t keySize = param->keySize;

    unsigned char* key = new unsigned char[keySize];
    unsigned char  element = param->seed;

    for(size_t i = 0; i < keySize; i++)
    {
        key[i] = (a * element + c) % m;
        element = key[i];
    }

    return key;
}


void* crypt(void* arg)
{
    crypt_params* param = reinterpret_cast<crypt_params* >(arg);
    unsigned char* msg = param->msg;
    unsigned char* key = param->key;
    unsigned char* outputText = param->outputText;
    size_t topIndex = param->topIndex;
    size_t downIndex = param->downIndex;

    while(downIndex < topIndex)
    {
        outputText[downIndex] = key[downIndex] ^ msg[downIndex];
        downIndex++;
    }

    param->outputText = outputText;
    delete(param);


    int status;

    if((status = pthread_barrier_wait(&barrier)) == PTHREAD_BARRIER_SERIAL_THREAD)
    {
        pthread_barrier_destroy(&barrier);
    }
    else if(status != 0)
    {
        cerr << "Error: pthread_barrier_destroy()" << endl;
        exit(0);
    }
}


int main(int argc, char* argv[])
{

	int inputFileDesc;
	
    if ((inputFileDesc = open("input.txt", O_RDONLY, 0666)) == -1)
	{
        cerr << "Error: file open" << endl;
        exit(0);
    }


    int inputSize;
	
    if((inputSize = lseek(inputFileDesc, 0, SEEK_END)) == -1)
	{
        cerr << "Error: lseek - SEEK_END" << endl;
        exit(0);
    }


    keygen_params keyParam;
    unsigned char* key = new unsigned char[inputSize];
    unsigned char* outputText = new unsigned char[inputSize];
	unsigned char* msg = new unsigned char[inputSize];

	if(lseek(inputFileDesc, 0, SEEK_SET) == -1)
	{
        cerr << "Error: lseek - SEEK_SET" << endl;
        exit(0);
    }
	
	if((inputSize = read(inputFileDesc, msg, inputSize)) == -1)
	{
        cerr << "Error: file read" << endl;
        exit(0);
    }
	

    keyParam.keySize = inputSize;
    keyParam.a = 32;
    keyParam.c = 50;
    keyParam.m = 43;
    keyParam.seed = 74;


    pthread_t keygenThread;

    if(pthread_create(&keygenThread, NULL, keygen, &keyParam) != 0)
    {
        cerr << "Error: pthread_create()" << endl;
        exit(0);
    }

    if(pthread_join(keygenThread, (void**)&key) != 0)
    {
        cerr << "Error: pthread_join()" << endl;
        exit(0);
    }


    int status;

    if((status = pthread_barrier_init(&barrier, NULL, NUM_OF_ALL_THREADS)) != 0)
    {
        cerr << "Error: pthread_barrier_init()" << endl;
        exit(0);
    }


    pthread_t cryptThread[NUM_OF_ALL_THREADS];

    for(size_t i = 0; i < NUM_OF_THREADS; i++)
    {
        crypt_params* cryptParametrs = new crypt_params();

        cryptParametrs->key = key;
        cryptParametrs->size = inputSize;
        cryptParametrs->outputText = outputText;
        cryptParametrs->msg = msg;


		if(i == 0) cryptParametrs->downIndex = 0; 
        else cryptParametrs->downIndex = inputSize / NUM_OF_THREADS * i;

        if(i == 9) cryptParametrs->topIndex = inputSize; 
        else cryptParametrs->topIndex = inputSize / NUM_OF_THREADS * (i + 1);

		
        pthread_create(&cryptThread[i], NULL, crypt, cryptParametrs);
    }

    if((status = pthread_barrier_wait(&barrier)) == PTHREAD_BARRIER_SERIAL_THREAD)
    {
        pthread_barrier_destroy(&barrier);
    }
    else if(status != 0)
    {
        cerr << "Error: pthread_barrier_destroy()" << endl;
        exit(0);
    }


    int outputFileDesc;

    if ((outputFileDesc = open("output.txt", O_WRONLY | O_CREAT, 0666)) == -1)
    {
        cerr << "Error: output file doesn't open" << endl;
        exit(0);
    }


    int outputFile;

    if((outputFile = write(outputFileDesc, outputText, inputSize)) == -1)
    {
        cerr << "Error: file write" << endl;
        exit(0);
    }

    close(outputFileDesc);

	close(inputFileDesc);

    
    delete[] key;
    delete[] outputText;
	delete[] msg;

    
    return 0;
}
