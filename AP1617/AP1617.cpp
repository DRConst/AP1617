// AP1617.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <random>
#include "mpi.h"
#include <iostream>
#include <thread>
int numElems = 1000;

int *toSort = new int[numElems];

int numProcs = 5;

int numPivots = (numProcs - 1) * numProcs;

int *pivots = new int[numPivots];

int elemsPerProc = numElems / 5;

int *chosenPivots = new int[(numProcs - 1)];

int **partitionedSets = new int*[numPivots];
int *partitionCnts = new int[numPivots];

void genRand()
{
	srand(NULL);

	for (int i = 0; i<numElems; i++) {
		toSort[i] = rand();
	}
}


void quicksort(int *array, int lo, int hi)
{
	int i = lo, j = hi, h;
	int x = array[(lo + hi) / 2];

	//partition
	do
	{
		while (array[i]<x) i++;
		while (array[j]>x) j--;
		if (i <= j)
		{
			h = array[i];
			array[i] = array[j];
			array[j] = h;
			i++; j--;
		}
	} while (i <= j);

	//recursion
	if (lo<j) quicksort(array,lo, j);
	if (i<hi) quicksort(array,i, hi);
}


void doFirstStep(int threadID)
{
	//Run the quicksort locally
	quicksort(toSort, threadID * elemsPerProc, (threadID + 1) * elemsPerProc);

	//Select the local samples
	int numPivots = numProcs - 1;

	int step = elemsPerProc / numPivots;
	int j = 0;

	//choose the pivots
	for(int i = step; i <= elemsPerProc; i += step)
	{
		pivots[(numProcs - 1) * threadID + j] = toSort[threadID * elemsPerProc + i];
		j++;
	}
}

void doSecondStep(int threadID)
{
	
	//count the number of elems to each pivot
	int *pivotIndices = new int[numProcs - 1];
	int  j = 0;
	for(int i = (numProcs - 1) * threadID; i < (numProcs - 1) * ( threadID + 1) && j < numProcs - 1; i++)
	{
			if(toSort[i] >= chosenPivots[j])
			{
				pivotIndices[j++] = i - 1;
			}
	}
}


int main(int argc, char**argv) 
{
	//Intialize the array
	genRand();

	//Have each thread do a quicksort
	std::thread *t0 = new std::thread(doFirstStep, 0);
	std::thread *t1 = new std::thread(doFirstStep, 1);
	std::thread *t2 = new std::thread(doFirstStep, 2);
	std::thread *t3 = new std::thread(doFirstStep, 3);
	std::thread *t4 = new std::thread(doFirstStep, 4);

	t0->join();
	t1->join();
	t2->join();
	t3->join();
	t4->join();

	delete(t0);
	delete(t1);
	delete(t2);
	delete(t3);
	delete(t4);
	/*for (int i = 0; i < (numProcs - 1) * numProcs; i++)
		std::cout << pivots[i] << " ";

	std::cout << "\n\n\n";*/


	//Sort the pivots
	quicksort(pivots, 0, (numProcs - 1) * numProcs - 1);

	//Get the best pivots
	int step = numPivots / numProcs - 1;
	int j = 0;

	for (int i = step; i <= numPivots; i += step)
	{
		chosenPivots[j] = pivots[i];
		j++;
	}

	t0 = new std::thread(doSecondStep, 0);
	t1 = new std::thread(doSecondStep, 1);
	t2 = new std::thread(doSecondStep, 2);
	t3 = new std::thread(doSecondStep, 3);
	t4 = new std::thread(doSecondStep, 4);

	t0->join();
	t1->join();
	t2->join();
	t3->join();
	t4->join();

	delete(t0);
	delete(t1);
	delete(t2);
	delete(t3);
	delete(t4);


	/*for (int i = 0; i < (numProcs - 1); i++)
		std::cout << chosenPivots[i] << " ";*/

	/*for (int i = 0; i < (numProcs - 1) * numProcs; i++)
		std::cout << pivots[i] << " ";

	std::cout << "\n\n\n";

	for (int i = 0; i < numElems; i++)
		std::cout << toSort[i]<< " ";*/


    return 0;
}

