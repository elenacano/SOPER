all: Shm_concurrence Shm_concurrence_solved Shm_producer Shm_consumer Shm_producer_file Shm_consumer_file Mq_injector Mq_workers_pool Mq_workers_pool_timed

Shm_concurrence: shm_concurrence.c
	gcc -o Shm_concurrence shm_concurrence.c -lm -lrt

Shm_concurrence_solved: shm_concurrence_solved.c
	gcc -pthread -o Shm_concurrence_solved shm_concurrence_solved.c -lm -lrt

Shm_producer: shm_producer.c
	gcc -pthread -o Shm_producer shm_producer.c -lm -lrt

Shm_consumer: shm_consumer.c
	gcc -pthread -o Shm_consumer shm_consumer.c -lm -lrt

Shm_producer_file: shm_producer_file.c
	gcc -pthread -o Shm_producer_file shm_producer_file.c -lm -lrt

Shm_consumer_file: shm_consumer_file.c
	gcc -pthread -o Shm_consumer_file shm_consumer_file.c -lm -lrt

Mq_injector: mq_injector.c
	gcc -o Mq_injector mq_injector.c -lrt

Mq_workers_pool: mq_workers_pool.c
	gcc -o Mq_workers_pool mq_workers_pool.c -lrt

Mq_workers_pool_timed: mq_workers_pool_timed.c
	gcc -o Mq_workers_pool_timed mq_workers_pool_timed.c -lrt
