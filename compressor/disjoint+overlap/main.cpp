#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <pthread.h>
#include <time.h>
#include <vector>
#include <iostream>
#include <fstream>
#include <string>
#include <algorithm>
#include <unordered_map>
using namespace std;


#define NUMTHREAD 16
#define KMERLENGTH 10
#define REFINDEX 20
#define TESTSEQ 24


typedef struct PthreadArg {
	size_t seqIdx;
	size_t thrIdx;
	size_t subSeq;
}PthreadArg;


vector<string> sequences;
unordered_map<string, uint32_t> reference;
unordered_map<uint32_t, uint32_t> occurrence;


uint64_t seqSizeOrg = 0;
uint64_t seqSizeCmp = 0;
uint64_t varInt = 0;
//size_t usedReference = 524288;


pthread_mutex_t mutex;


static inline double timeChecker( void ) {
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return (double)(tv.tv_sec) + (double)(tv.tv_usec) / 1000000;
}

bool decendingOrder( pair<uint32_t, uint32_t>& x, pair<uint32_t, uint32_t>& y ) {
	return x.second > y.second;
}

void fastaReader( char *filename ) {
	sequences.clear();
	sequences.shrink_to_fit();
	string seqLine;

	ifstream f_data_sequences(filename);
	if ( !f_data_sequences.is_open() ) {
		printf( "File not found: %s\n", filename );
		exit(1);
	}

	while ( getline(f_data_sequences, seqLine) ) {
		if ( seqLine[0] != '>' ) {
			sequences.push_back(seqLine);
			seqSizeOrg += seqLine.size();
		}
	}

	f_data_sequences.close();
}

void refBookReader( char *filename ) {
	string refLine;

	ifstream f_data_reference(filename);
	if ( !f_data_reference.is_open() ) {
		printf( "File not found: %s\n", filename );
		exit(1);
	}

	uint32_t index = 0;
	while ( getline(f_data_reference, refLine) ) {
		//if ( index == usedReference ) break;
		string kmer;
		kmer.reserve(KMERLENGTH);		
		for ( size_t i = 0; i < KMERLENGTH; i ++ ) {
			kmer.push_back(refLine[i]);
		}
		if ( reference.insert(make_pair(kmer, index)).second == false ) {
			printf( "There's an issue on reference code book\n" );
			exit(1);
		}
		index++;
	}

	f_data_reference.close();

}

void occurWriter( char *filename ) {
	//Sort first before writing the result
	vector<pair<uint32_t, uint32_t>> occurrence_vec(occurrence.begin(), occurrence.end());
	sort(occurrence_vec.begin(), occurrence_vec.end(), decendingOrder);

	// Write the result
	ofstream f_data_result(filename);
	if ( !f_data_result.is_open() ) {
		printf( "File not found: %s\n", filename );
		exit(1);
	}

	for ( size_t i = 0; i < occurrence_vec.size(); i ++ ) {
		f_data_result << occurrence_vec[i].first << "," << occurrence_vec[i].second << "\n";
	}
	f_data_result << endl;

	f_data_result.close();
}

uint64_t varIntEncode( uint64_t value, unsigned char *buf ) {
	uint64_t encodedBytes = 0;
	do {
		uint8_t byte = value & 0x7F; 	// Get the lower 7 bits
		value >>= 7; 			// Shift right by 7 bits
		if (value > 0) byte |= 0x80; 	// Set the MSB if more bytes follow
		buf[encodedBytes++] = byte;
	} while (value > 0);
	
	return encodedBytes;
}

uint64_t varIntDecode( const unsigned char *buf, int *decodedBytes ) {
	uint64_t value = 0;
	uint8_t byte = 0;
	int shift = 0;
	int i = 0;
	do {
		byte = buf[i++];
		value |= (uint64_t)(byte & 0x7F) << shift;
		shift += 7;
	} while (byte & 0x80);
	
	*decodedBytes = i;
	
	return value;
}

void *compressor( void *pthreadArg ) {
	PthreadArg *arg = (PthreadArg *)pthreadArg;
	size_t sp = arg->thrIdx * arg->subSeq;
	size_t fp = 0;
	if ( arg->thrIdx + 1 == NUMTHREAD ) fp = sequences[arg->seqIdx].size();
	else fp = (arg->thrIdx + 1) * arg->subSeq;
	
	size_t flag = 0;
	while ( sp <= fp - KMERLENGTH) {
		string subseq = sequences[arg->seqIdx].substr(sp, KMERLENGTH);
		if ( reference.find(subseq) != reference.end() ) {
			// Variable Integer Scheme
			//unsigned char buf[10];
			//uint64_t encodedBytes = varIntEncode((uint64_t)reference.at(subseq), buf);
			pthread_mutex_lock(&mutex);
			seqSizeCmp++;
			if ( occurrence.insert(make_pair(reference.at(subseq), 1)).second == false ) {
				occurrence.at(reference.at(subseq)) += 1;
			}
			//varInt += encodedBytes;
			pthread_mutex_unlock(&mutex);
			flag = 1;
		} else flag = 0;
		
		if ( flag == 1 ) sp += KMERLENGTH;
		else sp += 1;
	}
	return 0;
}


int main() {
	char *filenameS = "../../data/sequences/hg38.fasta";
	char *filenameR = "../../data/references/hg19RefBook10Mers.txt";
	char *filenameO = "histogram.csv";

	// Read sequence file
	fastaReader( filenameS );

	// Read reference file
	refBookReader( filenameR );

	// Compression
	pthread_t pthread[NUMTHREAD];
	pthread_mutex_init(&mutex, NULL);
	PthreadArg pthreadArg[NUMTHREAD];

	double processStart = timeChecker();
	size_t subSeq = sequences[TESTSEQ].size() / NUMTHREAD;
	// Thread Create
	for ( size_t i = 0; i < NUMTHREAD; i ++ ) {
		pthreadArg[i].seqIdx = TESTSEQ;
		pthreadArg[i].thrIdx = i;
		pthreadArg[i].subSeq = subSeq;

		pthread_create(&pthread[i], NULL, compressor, (void *)&pthreadArg[i]);
	}
	// Thread Join
	for ( size_t j = 0; j < NUMTHREAD; j ++ ) {
		pthread_join(pthread[j], NULL);
	}
	// Mutex Destroy
	pthread_mutex_destroy(&mutex);
	double processFinish = timeChecker();
	double elapsedTime = processFinish - processStart;

	// Write the occurrence result
	occurWriter( filenameO );

	printf( "--------------------------------------------\n" );
	printf( "REFERENCE\n" );
	printf( "Reference Book [#KMER]: %ld\n", reference.size() );
	printf( "Reference Book [Size]: %0.4f MB\n", ((double)reference.size() * KMERLENGTH) / 1024 / 1024 );
	printf( "--------------------------------------------\n" );
	printf( "SEQUENCE\n" );
	printf( "Number of Base Pairs [Original]: %ld\n", sequences[TESTSEQ].size() );
	printf( "Original File Size: %0.4f MB\n", (double)sequences[TESTSEQ].size() / 1024 / 1024 );
	printf( "--------------------------------------------\n" );
	printf( "COMPRESSION RESULT\n" );
	printf( "Number of Base Pairs [Compressed]: %ld\n", seqSizeCmp * KMERLENGTH );
	printf( "Compressed File Size [Original]: %0.4f MB\n", 
		(double)(((sequences[TESTSEQ].size() - (seqSizeCmp * KMERLENGTH)) * 8) + (seqSizeCmp * REFINDEX)) / 8 / 1024 / 1024 );
	printf( "Compressed File Size [2-b Encd]: %0.4f MB\n", 
	     	(double)(((sequences[TESTSEQ].size() - (seqSizeCmp * KMERLENGTH)) * 2) + (seqSizeCmp * REFINDEX)) / 8 / 1024 / 1024 );
	printf( "Elapsed Time: %lf\n", elapsedTime );

	return 0;
}
