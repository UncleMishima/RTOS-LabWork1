#include <cstdlib>
#include <iostream>
#include <pthread.h>
#include <string>
#include <stdio.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <thread>


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
        exit(EXIT_FAILURE);
    }
}


int main(int argc, char* argv[])
{
    //argv[1] for input file, argv[2] for output file
    //if (argc > 1) for (int i = 0; i < sizeof(argv)/sizeof(char); i++) cout << argv[i] << endl;

    int inputFileDesc;

    //input.txt file
    char* input = argv[1];	
	
    if ((inputFileDesc = open(input, O_RDONLY, 0666)) == -1)
	{
        cerr << "Error: file open" << endl;
        exit(EXIT_FAILURE);
    }


    int inputSize;
	
    if((inputSize = lseek(inputFileDesc, 0, SEEK_END)) == -1)
	{
        cerr << "Error: lseek - SEEK_END" << endl;
        exit(EXIT_FAILURE);
    }


    keygen_params keyParam;
    unsigned char* key = new unsigned char[inputSize];
    unsigned char* outputText = new unsigned char[inputSize];
	unsigned char* msg = new unsigned char[inputSize];

	if(lseek(inputFileDesc, 0, SEEK_SET) == -1)
	{
        cerr << "Error: lseek - SEEK_SET" << endl;
        exit(EXIT_FAILURE);
    }
	
	if((inputSize = read(inputFileDesc, msg, inputSize)) == -1)
	{
        cerr << "Error: file read" << endl;
        exit(EXIT_FAILURE);
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
        exit(EXIT_FAILURE);
    }

    if(pthread_join(keygenThread, (void**)&key) != 0)
    {
        cerr << "Error: pthread_join()" << endl;
        exit(EXIT_FAILURE);
    }

	int coreNumber = thread::hardware_concurrency();
	//cout << coreNumber << endl;

    int status;

    if((status = pthread_barrier_init(&barrier, NULL, coreNumber)) != 0)
    {
        cerr << "Error: pthread_barrier_init()" << endl;
        exit(EXIT_FAILURE);
    }

	
	
    pthread_t cryptThread[coreNumber];

    for(size_t i = 0; i < coreNumber - 1; i++)
    {
        crypt_params* cryptParametrs = new crypt_params();

        cryptParametrs->key = key;
        cryptParametrs->size = inputSize;
        cryptParametrs->outputText = outputText;
        cryptParametrs->msg = msg;


		if(i == 0) cryptParametrs->downIndex = 0; 
        else cryptParametrs->downIndex = inputSize / (coreNumber - 1) * i;

        if(i == 9) cryptParametrs->topIndex = inputSize; 
        else cryptParametrs->topIndex = inputSize / (coreNumber - 1) * (i + 1);

		
        pthread_create(&cryptThread[i], NULL, crypt, cryptParametrs);
    }

    if((status = pthread_barrier_wait(&barrier)) == PTHREAD_BARRIER_SERIAL_THREAD)
    {
        pthread_barrier_destroy(&barrier);
    }
    else if(status != 0)
    {
        cerr << "Error: pthread_barrier_destroy()" << endl;
        exit(EXIT_FAILURE);
    }


    int outputFileDesc;

    //output.txt file
    char* output = argv[2];	

    if ((outputFileDesc = open(output, O_WRONLY | O_CREAT, 0666)) == -1)
    {
        cerr << "Error: output file doesn't open" << endl;
        exit(EXIT_FAILURE);
    }


    int outputFile;

    if((outputFile = write(outputFileDesc, outputText, inputSize)) == -1)
    {
        cerr << "Error: file write" << endl;
        exit(EXIT_FAILURE);
    }

    close(outputFileDesc);

	close(inputFileDesc);

    
    delete[] key;
    delete[] outputText;
	delete[] msg;

    
    return EXIT_SUCCESS;
}
