#include "stdafx.h"
#include "random"
#include "mpi.h"
#include <iostream>
#include <limits>
#include <ratio>
#include <chrono>

int numElems;

int *toSort;

int elemsPerProc;

void genRand()
{
	std::random_device rd; // obtain a random number from hardware
	std::mt19937 eng; // seed the generator
	std::uniform_int_distribution<> distr(0, 10000000); // define the range

	for (int i = 0; i<numElems; i++) {
		toSort[i] = distr(eng);
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
	if (lo<j) quicksort(array, lo, j);
	if (i<hi) quicksort(array, i, hi);
}

int main(int argc, char **argv)
{
	// ----- TIMES ----- //

	std::chrono::duration<double, std::milli> temp_time;

	// ----- Communication Times ----- //

	double process_communication_time = 0.0;
	std::chrono::high_resolution_clock::time_point start_communication_time, end_communication_time;

	// ----- Execution Times ----- //

	double process_execution_time = 0.0;
	std::chrono::high_resolution_clock::time_point start_execution_time, end_execution_time;

	numElems = atoi(argv[1]);

	toSort = new int[numElems];

	// ----- MPI ----- //
	

	// Initialize the MPI environment
	MPI_Init(&argc, &argv);

	// Get the number of processes
	int numProcs;
	MPI_Comm_size(MPI_COMM_WORLD, &numProcs);
	
	// Get the rank of the process
	int procRank;
	MPI_Comm_rank(MPI_COMM_WORLD, &procRank);

	//Have proc 0 init the array
	if(!procRank)
	{
		genRand();
	}

	// ----- START EXECUTION TIME ----- //
	start_execution_time = std::chrono::high_resolution_clock::now();

	int *elemsPerProcv = new int[numProcs];
	int *displs = new int[numProcs];


	for(int i = 0; i < numProcs - 1; i++)
	{
		elemsPerProcv[i] = numElems / numProcs;

		if (!i)
			displs[0] = 0;
		else
			displs[i] = displs[i - 1] + elemsPerProcv[i - 1];
	}

	elemsPerProcv[numProcs - 1] = numElems / numProcs + numElems % numProcs;
	displs[numProcs - 1] = displs[numProcs - 1 - 1] + elemsPerProcv[numProcs - 1  - 1];;
	elemsPerProc = elemsPerProcv[procRank];


	int *elems = new int[elemsPerProc];

	start_communication_time = std::chrono::high_resolution_clock::now();
	//Scatter the array to all the processes
	MPI_Scatterv(toSort, elemsPerProcv, displs, MPI_INT, elems, elemsPerProc, MPI_INT, 0, MPI_COMM_WORLD);
	end_communication_time = std::chrono::high_resolution_clock::now();
	temp_time = end_communication_time - start_communication_time;
	process_communication_time += temp_time.count();

	delete toSort;

	//Each process quicksorts its elems array
	quicksort(elems, 0, elemsPerProc - 1);

	//Select the local samples
	int numPivots = numProcs - 1;

	int step = elemsPerProc / numPivots;
	int j = 0;


	int *pivots;

	//if proc0 then alloc for all the pivots, else just alloc the local pivots
	if (!procRank)
		pivots = new int[(numProcs - 1) * numProcs];
	else
		pivots = new int[numProcs - 1];

	//choose the pivots
	for (int i = 0; i < elemsPerProc; i += step)
	{
		pivots[j++] = elems[i];
	}


	start_communication_time = std::chrono::high_resolution_clock::now();
	//proc0 gathers all the pivots
	MPI_Gather(pivots, numPivots, MPI_INT, pivots, numPivots, MPI_INT, 0, MPI_COMM_WORLD);
	end_communication_time = std::chrono::high_resolution_clock::now();
	temp_time = end_communication_time - start_communication_time;
	process_communication_time += temp_time.count();


	int *chosenPivots = new int[numPivots];

	//proc0 now sorts the pivots and chooses the best ones
	if(!procRank)
	{
		quicksort(pivots, 0, numPivots * numProcs - 1);

		step = numPivots;

		j = 0;
		//choose the pivots
		for (int i = step; i < numPivots * numProcs; i += step)
		{
			chosenPivots[j++] = pivots[i];
		}
	}



	start_communication_time = std::chrono::high_resolution_clock::now();
	//MPI_Barrier(MPI_COMM_WORLD);
	//Broadcast the chosen pivots
	MPI_Bcast(chosenPivots, numPivots, MPI_INT, 0, MPI_COMM_WORLD);
	end_communication_time = std::chrono::high_resolution_clock::now();
	temp_time = end_communication_time - start_communication_time;
	process_communication_time += temp_time.count();
	//Now that each proc has a pivot, partition data acordingly
	//Each proc keeps a a list of offsets, plus a list of lenghts. There are numProcs sections
	int *data = new int[numProcs];
	int *lens = new int[numProcs];
	int currentPos = 0;
	int classindex;
	for ( classindex = 0; classindex < numProcs ; classindex++)
	{
		data[classindex] = currentPos;
		lens[classindex] = 0;
		while ((currentPos < elemsPerProc)	&& (elems[currentPos] <= chosenPivots[classindex]))
		{
			lens[classindex]++;
			currentPos++;
		}
	}

	data[numProcs - 1] = currentPos;
	lens[numProcs - 1] = elemsPerProc - currentPos;


	MPI_Barrier(MPI_COMM_WORLD);


	int *recvLens = new int[numProcs];
	int *recvOffsets = new int[numProcs];
	int *recvBuff;
	int totalLen = 0;
	//For each proc, gather the lenghts of the sections, alloc a buff, then gather the data

	for(int classProc = 0; classProc < numProcs; classProc++)
	{
		start_communication_time = std::chrono::high_resolution_clock::now();
		//classProc is equivalent to the class
		MPI_Gather(&lens[classProc], 1, MPI_INT, recvLens, 1, MPI_INT, classProc, MPI_COMM_WORLD);
		end_communication_time = std::chrono::high_resolution_clock::now();
		temp_time = end_communication_time - start_communication_time;
		process_communication_time += temp_time.count();

		

		//Sum the lenghts. Get the offsets
		//The offsets are calculated to be used on top of the class offset
		if(procRank == classProc)
		{
			
			for(int i = 0; i < numProcs; i++)
			{
				totalLen += recvLens[i];

				if(!i)
					recvOffsets[i] = 0;
				else
				{
					//The offset to the ith class is the previous start + len
					//This way we can avoid another Gather
					recvOffsets[i] = recvOffsets[i - 1] + recvLens[i - 1];
				}
			}
			
			recvBuff = new int[totalLen];

		}

		start_communication_time = std::chrono::high_resolution_clock::now();
		MPI_Gatherv(&elems[data[classProc]], lens[classProc], MPI_INT, recvBuff, recvLens, recvOffsets, MPI_INT, classProc, MPI_COMM_WORLD);
		end_communication_time = std::chrono::high_resolution_clock::now();
		temp_time = end_communication_time - start_communication_time;
		process_communication_time += temp_time.count();
		

		quicksort(recvBuff, 0, totalLen - 1);

		
		if (procRank == classProc) {

			quicksort(recvBuff, 0, totalLen - 1);

		}


	}


	start_communication_time = std::chrono::high_resolution_clock::now();
	//Proc0 gathers the lengths of all the parts, should be equal to numElems
	MPI_Gather(&totalLen, 1, MPI_INT, recvLens, 1, MPI_INT, 0, MPI_COMM_WORLD);
	end_communication_time = std::chrono::high_resolution_clock::now();
	temp_time = end_communication_time - start_communication_time;
	process_communication_time += temp_time.count();



	int *finalRes = new int[numElems];
	
	if (!procRank)
		for (int i = 0; i < numElems; i++)
		{
			finalRes[i] = -1;
		}

	delete[] recvOffsets;

	recvOffsets = new int[numProcs];

	if(!procRank)
		for(int i = 0; i < numProcs; i++)
		{
			
			if(!i)
				recvOffsets[0] = 0;
			else
				recvOffsets[i] = recvOffsets[i-1] + recvLens[i-1];
		}
	

	start_communication_time = std::chrono::high_resolution_clock::now();
	MPI_Gatherv(recvBuff, totalLen, MPI_INT, finalRes, recvLens, recvOffsets, MPI_INT, 0, MPI_COMM_WORLD);
	end_communication_time = std::chrono::high_resolution_clock::now();
	temp_time = end_communication_time - start_communication_time;
	process_communication_time += temp_time.count();



	end_execution_time = std::chrono::high_resolution_clock::now();
	temp_time = end_execution_time - start_execution_time;
	process_execution_time += temp_time.count();

	if (!procRank) {
		std::cout << "Total Time " << process_execution_time;
		std::cout << "\nTotal Communication Time " << process_communication_time;
		std::cout << "\nTotal Execution Time " << process_execution_time - process_communication_time;
		std::cout << "\n";
	}

	MPI_Barrier(MPI_COMM_WORLD);

	MPI_Finalize();

}

