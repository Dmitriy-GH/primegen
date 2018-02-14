// ��������� ������� ����� � �������� ��������� �� 2^64
#define _CRT_SECURE_NO_WARNINGS
#include <time.h>
#ifdef WIN32
#define strtoull(X,Y,Z) _strtoui64(X,Y,Z)
#pragma warning(disable: 4996) // fopen()
#endif
#include "next_prime64.h" 

// ���������� ���������� � ����
bool prime_store(uint64_t x, FILE* f, int width)
{
	char buf[32];
	int len = sprintf(buf, "%llu", x);
	if(width > 0) {
		static int line = 0;
		buf[len++] = ',';
		line += len;
		if(line >= width) {
			line = 0;
			buf[len++] = 0xD;
			buf[len++] = 0xA;
		}
	} else {
		buf[len++] = 0xD;
		buf[len++] = 0xA;
	}
	if(fwrite(buf, 1, len, f) != len) {
		printf("error write data\n");
		return false;
	} else {
		return true;
	}
}
//***************************************************************************************
// ������������� ������
#include <thread>
#include <mutex>  
#include <vector>

// ��������� ���������
#define ST_INIT 1 // ����� � �������
#define ST_CALC 2 // ��������������
#define ST_END  3 // ������
#define ST_ERR  4 // ���� ������

// ������� ��� ������� ���������
typedef struct {
	uint64_t from; // ������ ���������
	uint64_t to; // ����� ���������
	int cnt; // ���-�� �������
	int state; // ���������
} interval_t;

std::vector<interval_t> res; // ��������� ��� ������� � ����������
std::mutex res_change; // ������������� ������� � res

// ����� �������
void prime_thread(int block)
{
	prime_bitmap_t pb = {0}; // �������� �������� ������
	bool calc_up = true; // ������� ��������������� �����
	while(block >= 0) {
		// ������ res[block]
		interval_t* v = &res[block];
		uint64_t x = next_prime(v->from, &pb);
		while(x <= v->to) {
			v->cnt++;
			x = next_prime(x, &pb);
		}
		// ����� ���������� �����
		res_change.lock();
		v->state = (x == 0) ? ST_ERR : ST_END;
		if(calc_up) { 
			block++; //��������� � ����� ���������
			if(block != res.size() && res[block].state == ST_INIT) {
				res[block].state = ST_CALC;
			} else {
				calc_up = false;
			}
		}
		if(!calc_up) { // ����� ������������� � ����� ����������
			for(block = (int)res.size() - 1; block >= 0; block--) {
				if(res[block].state == ST_INIT) {
					res[block].state = ST_CALC;
					break;
				}
			}
		}
		res_change.unlock();
	}
}

// ����� ������������� ���� next_prime()
void prime_init(uint64_t max)
{
	next_prime(max); 
}

// ������ ������� ������� � ���� �����������
int prime_calc_mt(uint64_t from, uint64_t to, FILE* f, int width, int thread_cnt)
{
	if(from & 1) from--; // ������ ������ ����� �� ���������� ������� from
	if(from <= 2) from = 1;
	std::thread th_init(prime_init, to);
	// ������������ ������� ������� �������� PRIME_BUF (���� ������)
	for(uint64_t i = from; i < to; i += PRIME_BUF - (i & (PRIME_BUF - 1))) {
		interval_t v = {0};
		v.from = i;
		v.to = i + PRIME_BUF - (i & (PRIME_BUF - 1)) - 1;
		if(v.to > to) v.to = to;
		v.state = ST_INIT;
		res.push_back(v);
	}
	// ������������� ����
	th_init.join();
	// ������ ������� �������
	std::vector<std::thread> th;
	for(int i = 0; i < thread_cnt; i++) {
		int first = (int)res.size() * i / thread_cnt; // ����� ������� ����� ��� ������
		res_change.lock();
		res[first].state = ST_CALC;
		res_change.unlock();
		th.push_back(std::thread(prime_thread, first));
	}
	// �������� ���������� �������
	for(int i = 0; i < thread_cnt; i++) th[i].join();
	// ����� �����������
	int cnt = 0;
	for(int i = 0; i != res.size(); i++) {
		if(res[i].state != ST_END) {
			cnt = -1;
			break;
		}
		cnt += res[i].cnt;
	}
	next_prime(0); // ������������ ������
	return cnt;
}

//***************************************************************************************
// ������ � ����� ������, ���������� ���������� �������� �������
int prime_calc(uint64_t from, uint64_t to, FILE* f, int width)
{
	if(from & 1) from--; // ������ ������ ����� �� ���������� ������� from
	if(from <= 2) from = 1;
	int cnt = 0;
	uint64_t step = (to - from) / 100; // ��� ���������� �������
	uint64_t show = from; // ��������� ����� ����������

	uint64_t x = next_prime(from);
	while(x && x < to) {
		cnt++;
		if(f) {
			if(!prime_store(x, f, width)) break;
			if(show < x) { // ����� ���������� 
				printf("complite %d%%\r", (int)((show - from) / step));
				show += step;
			}
		}
		x = next_prime(x);
	}
	next_prime(0); // ������������ ������
	if(x == 0) cnt = -1; // ������ � next_prime()
	return cnt;
}


//****************************************************************************************
int main(int argc, char* argv[])
{
	const char* file = NULL;
	uint64_t from = 0;
	uint64_t to = 0;
	int width = 0;
	int th_cnt = 0;
	for(int i = 1; i < argc; i++) {
		char* arg = argv[i];
		if(*arg != '-') {
			if(to) from = to;
			to = strtoull(arg, NULL,10);
			continue;
		}
		arg++;
		switch(*arg) {
			case 'f':
				file = arg + 2;
				break;
			case 'w':
				width = atoi(arg + 2);
				break;
			case 't':
				th_cnt = atoi(arg + 2);
				break;
		}
	}
	if(to <= from) {
		printf("\nPrime generator to 2^64\n\nprimegen [from] to [-t:threads] [-f:filename [-w:width]]\n\nfrom - start interval\nto - end interval\nthreads - threads count\nfilename - file to output primes\nwidth - minimum simbols at line\n");
		printf("\nExample:\nprimegen 10000 -t:4\nprimegen 5000 -f:result.txt\nprimegen 5000 10000 -f:result.txt -w:80\n");
	} else {
		FILE* f = NULL;
		if(file) {
			f = fopen(file, "wb");
			if(!f) {
				printf("Error create file '%s'\n", file);
				return -1;
			} 
		}
		printf("Calc primes on interval %llu..%llu\n", from, to);
		if(th_cnt > 1) printf("use %d threads\n", th_cnt);
		if(f) {
			printf("output to file '%s'", file);
			if(width > 0) printf(" %d simbols at line", width);
			printf("\n");
		}
		clock_t s = clock();
		int cnt;
		if(th_cnt > 1) {
			cnt = prime_calc_mt(from, to, f, width, th_cnt);
		} else {
			cnt = prime_calc(from, to, f, width);
		}
		if(cnt < 0) { // ������
			if(f) {
				fclose(f);
				remove(file);
			}
		} else {
			if(f) fclose(f);
			int time = (int)((clock() - s) * 1000 / CLOCKS_PER_SEC);
			printf("Count %d primes. Time: %d msec. Speed: %dK/sec\n", cnt, time, cnt / (time|1));
		}
	}
	#ifdef _DEBUG
	system("pause");
	#endif
	return 0;
}