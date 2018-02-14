// Генератор простых чисел в заданном диапазоне до 2^64
#define _CRT_SECURE_NO_WARNINGS
#include <time.h>
#ifdef WIN32
#define strtoull(X,Y,Z) _strtoui64(X,Y,Z)
#pragma warning(disable: 4996) // fopen()
#endif
#include "next_prime64.h" 

// Сохранение результата в файл
bool prime_store(uint64_t x, FILE* f, int width)
{
	char buf[32];
	int len = sprintf(buf, "%llu", x);
	if (width > 0) {
		static int line = 0;
		buf[len++] = ',';
		line += len;
		if (line >= width) {
			line = 0;
			buf[len++] = 0xD;
			buf[len++] = 0xA;
		}
	}
	else {
		buf[len++] = 0xD;
		buf[len++] = 0xA;
	}
	if (fwrite(buf, 1, len, f) != len) {
		printf("error write data\n");
		return false;
	}
	else {
		return true;
	}
}
//***************************************************************************************
// Многопоточный расчет
#include <thread>
#include <mutex>  
#include <vector>

// состояния обработки
#define ST_INIT 1 // готов к обсчету
#define ST_CALC 2 // обсчитрывается
#define ST_END  3 // готово
#define ST_ERR  4 // были ошибки

// задание для обсчета интервала
typedef struct {
	uint64_t from; // начало интервала
	uint64_t to; // конец интервала
	int cnt; // кол-во простых
	int state; // состояние
} interval_t;

std::vector<interval_t> res; // интервалы для обсчета и результаты
std::mutex res_change; // синхронизация доступа к res

					   // Поток расчета
void prime_thread(int block)
{
	prime_bitmap_t pb = { 0 }; // биткарта текущего потока
	bool calc_up = true; // считаем последовательно вверх
	while (block >= 0) {
		// обсчет res[block]
		interval_t* v = &res[block];
		uint64_t x = next_prime(v->from, &pb);
		while (x <= v->to) {
			v->cnt++;
			x = next_prime(x, &pb);
		}
		// выбор очередного блока
		res_change.lock();
		v->state = (x == 0) ? ST_ERR : ST_END;
		if (calc_up) {
			block++; //следующий в своем интервале
			if (block != res.size() && res[block].state == ST_INIT) {
				res[block].state = ST_CALC;
			}
			else {
				calc_up = false;
			}
		}
		if (!calc_up) { // поиск необсчитанных в чужих интервалах
			for (block = (int)res.size() - 1; block >= 0; block--) {
				if (res[block].state == ST_INIT) {
					res[block].state = ST_CALC;
					break;
				}
			}
		}
		res_change.unlock();
	}
}

// поток инициализации кэша next_prime()
void prime_init(uint64_t max)
{
	next_prime(max);
}

// запуск потоков расчета и сбор результатов
int prime_calc_mt(uint64_t from, uint64_t to, FILE* f, int width, int thread_cnt)
{
	if (from & 1) from--; // делаем четным чтобы не пропустить простое from
	if (from <= 2) from = 1;
	std::thread th_init(prime_init, to);
	// формирование заданий блоками кратными PRIME_BUF (окно решета)
	for (uint64_t i = from; i < to; i += PRIME_BUF - (i & (PRIME_BUF - 1))) {
		interval_t v = { 0 };
		v.from = i;
		v.to = i + PRIME_BUF - (i & (PRIME_BUF - 1)) - 1;
		if (v.to > to) v.to = to;
		v.state = ST_INIT;
		res.push_back(v);
	}
	// инициализация кэша
	th_init.join();
	// запуск потоков расчета
	std::vector<std::thread> th;
	for (int i = 0; i < thread_cnt; i++) {
		int first = (int)res.size() * i / thread_cnt; // номер первого блока для потока
		res_change.lock();
		res[first].state = ST_CALC;
		res_change.unlock();
		th.push_back(std::thread(prime_thread, first));
	}
	// Ожидание завершения потоков
	for (int i = 0; i < thread_cnt; i++) th[i].join();
	// Вывод результатов
	int cnt = 0;
	for (int i = 0; i != res.size(); i++) {
		if (res[i].state != ST_END) {
			cnt = -1;
			break;
		}
		cnt += res[i].cnt;
	}
	next_prime(0); // освобождение памяти
	return cnt;
}

//***************************************************************************************
// Расчет в одном потоке, возвращает количество найденых простых
int prime_calc(uint64_t from, uint64_t to, FILE* f, int width)
{
	if (from & 1) from--; // делаем четным чтобы не пропустить простое from
	if (from <= 2) from = 1;
	int cnt = 0;
	uint64_t step = (to - from) / 100; // для индикатора расчета
	uint64_t show = from; // следующий вывод индикатора

	uint64_t x = next_prime(from);
	while (x && x <= to) {
		cnt++;
		if (f) {
			if (!prime_store(x, f, width)) break;
			if (show < x) { // вывод индикатора 
				printf("complite %d%%\r", (int)((show - from) / step));
				show += step;
			}
		}
		x = next_prime(x);
	}
	next_prime(0); // освобождение памяти
	if (x == 0) cnt = -1; // ошибка в next_prime()
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
	for (int i = 1; i < argc; i++) {
		char* arg = argv[i];
		if (*arg != '-') {
			if (to) from = to;
			to = strtoull(arg, NULL, 10);
			continue;
		}
		arg++;
		switch (*arg) {
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
	if (to <= from) {
		printf("\nPrime generator to 2^64\n\nprimegen [from] to [-t:threads] [-f:filename [-w:width]]\n\nfrom - start interval\nto - end interval\nthreads - threads count\nfilename - file to output primes\nwidth - minimum simbols at line\n");
		printf("\nExample:\nprimegen 10000 -t:4\nprimegen 5000 -f:result.txt\nprimegen 5000 10000 -f:result.txt -w:80\n");
	}
	else {
		FILE* f = NULL;
		if (file) {
			f = fopen(file, "wb");
			if (!f) {
				printf("Error create file '%s'\n", file);
				return -1;
			}
		}
		printf("Calc primes on interval %llu..%llu\n", from, to);
		if (th_cnt > 1) printf("use %d threads\n", th_cnt);
		if (f) {
			printf("output to file '%s'", file);
			if (width > 0) printf(" %d simbols at line", width);
			printf("\n");
		}
		clock_t s = clock();
		int cnt;
		if (th_cnt > 1) {
			cnt = prime_calc_mt(from, to, f, width, th_cnt);
		}
		else {
			cnt = prime_calc(from, to, f, width);
		}
		if (cnt < 0) { // ошибка
			if (f) {
				fclose(f);
				remove(file);
			}
		}
		else {
			if (f) fclose(f);
			int time = (int)((clock() - s) * 1000 / CLOCKS_PER_SEC);
			printf("Count %d primes. Time: %d msec. Speed: %dK/sec\n", cnt, time, cnt / (time | 1));
		}
	}
#ifdef _DEBUG
	system("pause");
#endif
	return 0;
}