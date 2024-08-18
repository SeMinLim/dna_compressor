#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <algorithm>
#include <vector>
#include <iostream>
#include <fstream>
#include <string>
#include <unordered_map>
#include <map>
using namespace std;


#define KMERLENGTH 256
#define ENCKMERBUFUNIT 32
#define ENCKMERBUFSIZE 8
#define BINARYRWUNIT 8
#define RDCREFSIZE 268435456


vector<uint64_t> referenceRdc;


uint32_t refSizeOrg = 2836860451;
uint32_t refSizeRdc = 268435456;
uint32_t refSizeRead = 0;
uint32_t refSizeInst = 0;


uint64_t verification_x[ENCKMERBUFSIZE] = {0, };
uint64_t verification_y[ENCKMERBUFSIZE] = {0, };


void refShrinker( char *filename ) {
	ifstream f_reference_original(filename, ios::binary);
	while ( refSizeInst < refSizeRdc ) {
		uint64_t encKmer[ENCKMERBUFSIZE] = {0, };

		// Read
		for ( uint32_t j = 0; j < ENCKMERBUFSIZE; j ++ ) {
			f_reference_original.read(reinterpret_cast<char *>(&encKmer[j]), BINARYRWUNIT);
		}
		refSizeRead ++;

		// Verification purpose
		if ( refSizeInst == 0 ) {
			for ( uint32_t j = 0; j < ENCKMERBUFSIZE; j ++ ) {
				verification_x[j] = encKmer[j];
			}
		}

		// Insert 256-mers to the vector
		for ( uint32_t j = 0; j < ENCKMERBUFSIZE; j ++ ) {
			referenceRdc.push_back(encKmer[j]);
		}
		refSizeInst ++;

		if ( refSizeRead % 1000000 == 0 ) {
			printf( "Read: %u, Reduced Reference: %u\n", refSizeRead, refSizeInst );
			fflush( stdout );
		}
	}

	f_reference_original.close();
	printf( "Making Reduced Reference Book is Done!\n" );
	fflush( stdout );
}

void refWriter( char *filename ) {
	// Write the result
	ofstream f_reference_reduced(filename, ios::binary);
	for ( uint32_t i = 0; i < refSizeInst;  i ++ ) {
		for ( uint32_t j = 0; j < ENCKMERBUFSIZE; j ++ ) {
			f_reference_reduced.write(reinterpret_cast<char *>(&referenceRdcY[i*ENCKMERBUFSIZE + j]), BINARYRWUNIT);
		}
	}

	f_reference_reduced.close();

	printf( "Writing the Reduced Reference File is Done!\n" );
	fflush( stdout );
}


int main( void ) {
	char *filenameOriginal = "/mnt/ephemeral/hg19hg38RefBook256Mers.bin";
	char *filenameReduced = "/mnt/ephemeral/hg19hg38RefBook256Mers_256M_32LSB.bin";

	// Shrink Original Reference File
	refShrinker( filenameOriginal );

	// Write the Reduced Reference File
	refWriter( filenameReduced );

	// Verification
	ifstream f_verification(filenameReduced, ios::binary);
	for ( uint32_t i = 0; i < ENCKMERBUFSIZE; i ++ ) {
		f_verification.read(reinterpret_cast<char *>(&verification_y[i]), BINARYRWUNIT);
	}

	uint32_t cnt = 0;
	for ( uint32_t i = 0; i < ENCKMERBUFSIZE; i ++ ) {
		if ( verification_x[i] != verification_y[i] ) cnt ++;
	}
	if ( cnt == 0 ) printf( "Writing the Reduced Reference File is succeeded\n" );
	else printf( "Writing the Reduced Reference File is Failure\n" );
	fflush( stdout );

	return 0;
}
