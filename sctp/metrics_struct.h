#ifndef METRICS_STRUCT_H
	
	#define METRICS_STRUCT_H
	#define INSTANCES	1
	#define NUMBYTES	8	

	union tfby_8{
		unsigned long long int tfbyt_ll;
		uint8_t  	           tfbyt_8[NUMBYTES];
		uint64_t 			   tfbyt_64;
	};

	typedef union tfby_8 * tfbyte;
	extern tfbyte tf;

	struct metric {
		tfbyte    	transfby;
	    uint8_t		currcon;
	    uint8_t		histacc;
	    uint8_t 	connsucc; 
	};

	typedef struct metric * metrics;
	extern metrics metrstr;

#endif 
