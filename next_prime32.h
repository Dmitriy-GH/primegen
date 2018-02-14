#pragma once
// next_prime(uint32_t x) ������� ��� ��������� ���������� �������� ����� ������� ����������
// ���������� 0 ���� ���������� �� �������
// ��� ������������ ������ next_prime(0)
// ���������� 16�� ������ ��� ������ � 6 �� ��� ��� ��� ������������� �����

// ��� ������������� ������������� � ������ ������ ��������� ��������� ��������:
// prime_bitmap_t pb = {0};
// x = next_prime(x, &pb);

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>  
#include <string.h>

#define PRIME_BUF 262144 // ������ ������ ��� ���� ������ (����������� ������ �������� 2-��, ��� ���������� ����������� init[])

typedef struct {
	uint32_t bitmap[PRIME_BUF/16]; // ��������
	uint32_t limit; // �� ������� ��������� ������ (����� ������)
} prime_bitmap_t;

// ��������� ���������� �������� �����
uint32_t next_prime(uint32_t x, prime_bitmap_t* pb)
{
	static uint8_t* cache = NULL; // ��� ������ ���� ������� ��� ������������� ������
	static uint8_t* cache_end = NULL; // �����

	if(!cache && x != 0) { // �������������� ��������� ������ ��� ���
		cache = (uint8_t*) malloc(6540);
		if(!cache) {
			printf("No memory for cache\n");
			return 0;
		}
		cache_end = cache + 6540;
		// ������������� ���� ��� ���������� ������� ������ �������� PRIME_BUF �� 1048576
		uint8_t init[] = {1,2,1,2,1,2,3,1,3,2,1,2,3,3,1,3,2,1,3,2,3,4,2,1,2,1,2,7,2,3,1,5,1,3,3,2,3,3,1,5,1,2,1,6,6,2,1,2,3,1,5,3,3,3,1,3,2,1,5,7,2,1,2,7,3,5,1,2,3,4,3,3,2,3,4,2,4,5,1,5,1,3,2,3,4,2,1,2,6,4,2,4,2,3,6,1,9,3,5,3,3,1,3,5,3,3,1,3,3,2,1,6,5,1,2,3,3,1,6,2,3,4,5,4,5,4,3,3,2,4,3,2,4,2,7,5,6,1,5,1,2,1,5,7,2,1,2,7,2,1,2,10,2,4,5,4,2,3,3,7,2,3,3,4,3,6,2,3,1,5};
		memcpy(cache, init, sizeof(init));
		// ������������ ����
		uint32_t p = 5;
		for(int i = 0; i < sizeof(init); i++) p += ((uint32_t)cache[i]) << 1;
		uint8_t* next = cache + sizeof(init);
		while(p <= 65537 && next < cache_end) {
			uint32_t x = next_prime(p, pb);
			*next = (x - p) >> 1;
			next++;
			p = x;
		}
	}

	if(x < 5) {
		if(x == 0) { // ������������ ������
			if(cache) {
				free(cache);
				cache = NULL;
			}
			return 0;
		} else {
			return (x < 2) ? 2 : ((x < 3) ? 3 : 5);
		}
	} else {
		uint32_t* bitmap = pb->bitmap;
		x = ((x - 1) | 1);
		uint32_t step = ((x % 3)^3) << 1;
		if(step == 6) { // x ������ 3
			step = 4;
			x += 2;
		} else {
			x += step;
		}
		if(x >= pb->limit || x < pb->limit - PRIME_BUF) pb->limit = 0; // x ����������� � ������� ������
		uint32_t next = x & (PRIME_BUF - 1);

		while(true) {
			if(pb->limit) { // ���� �������� �� ������ ��������
				// ����� ���������� ����� x
				if(!step) {
					step = ((pb->limit - PRIME_BUF + next) % 3) << 1;
					if(!step) {
						step = 4;
						next += 2;
					}
				}
				for(; next < PRIME_BUF; next += step) {
					step ^= 6;
					if((bitmap[next >> 6] & (1 << ((next >> 1) & 31))) == 0) {
						return pb->limit + next - PRIME_BUF;
					}
				}
				next = 1;
				step = 0;
			}
			// ��� ��� ��������� ��������, ������������� ���������� ������
			if(!pb->limit) pb->limit = x - (x & (PRIME_BUF - 1));
			uint32_t start = pb->limit;
			pb->limit += PRIME_BUF; 
			if(pb->limit < start) { // ���������� �����������
				printf("No more primes :(\n");
				return 0;
			}
			memset(bitmap, 0, PRIME_BUF/16);
			uint8_t* cur = cache;
			uint32_t prime_next = 5; // ������ ������� ��� ���������� ������
			while(true) {
				uint32_t j = prime_next * prime_next; 
				if(j >= pb->limit) break; // ������ ��������� �� ����
				if(prime_next > 0xFFFF) { // ���������� ����������� j
					printf("No more primes :(\n");
					return 0;
				}
				uint32_t skip3 = prime_next % 3; // ��� �������� ������� 3
				if(j < start) { // ������������ ��� prime_next*prime_next+2N*prime_next
					uint32_t block = prime_next * 6;
					uint32_t start_block = start - (start - j) % block;
					if(start_block + (prime_next << (skip3^3)) < start) {
						j = start_block + block;
					} else {
						skip3 ^= 3; 
						j = start_block + (prime_next << skip3);
					}
				}
				if(j < pb->limit) {
					uint32_t step; 
					if(prime_next > PRIME_BUF) {
						step = PRIME_BUF*4; // ������ �� ���������� �����������
					} else {
						step = ((uint32_t)prime_next) << skip3; 
					}
					for(uint32_t i = (j & (PRIME_BUF - 1)); i < PRIME_BUF; i += step) { // ����������� ������� prime_next
						if(skip3 & 1) { // ��������� ������ 3
							step <<= 1;
						} else {
							step >>= 1;
						}
						skip3 ^= 3;
						bitmap[i >> 6] |= (1 << ((i >> 1) & 31));
					}
				}
				if(cur < cache_end) { // �������� ������� �� ����
					prime_next += ((uint32_t)*cur) << 1;
					cur++;
				} else {  // ���������� ��� � ����
					printf("Cache error\n");
					return 0;
				}
			}
		}
	}
	return 0;
}

// ������������ �������
uint32_t next_prime(uint32_t x)
{
	static prime_bitmap_t* pb = NULL;
	if(x == 0) {
		if(pb) {
			free(pb);
			pb = NULL;
		}
	} else if(!pb) {
		pb = (prime_bitmap_t*) calloc(1, sizeof(prime_bitmap_t));
		if(!pb) { 
			printf("No memory for prime bitmap\n");
			x = 0;
		}
	}
	return next_prime(x, pb);
}
