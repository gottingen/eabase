#include <gtest/gtest.h>
#include <pthread.h>
#include "eabase/utility/containers/mpsc_queue.h"

namespace {

const uint MAX_COUNT = 10000000;

void Consume(eabase::MPSCQueue<uint>& q, bool allow_empty) {
    uint i = 0;
    uint empty_count = 0;
    while (true) {
        uint d;
        if (!q.Dequeue(d)) {
            ASSERT_TRUE(allow_empty);
            ASSERT_LT(empty_count++, (const uint)10000);
            ::usleep(10 * 1000);
            continue;
        }
        ASSERT_EQ(i++, d);
        if (i == MAX_COUNT) {
            break;
        }
    }
}

void* ProduceThread(void* arg) {
    auto q = (eabase::MPSCQueue<uint>*)arg;
    for (uint i = 0; i < MAX_COUNT; ++i) {
        q->Enqueue(i);
    }
    return NULL;
}

void* ConsumeThread1(void* arg) {
    auto q = (eabase::MPSCQueue<uint>*)arg;
    Consume(*q, true);
    return NULL;
}

TEST(MPSCQueueTest, spsc_single_thread) {
    eabase::MPSCQueue<uint> q;
    for (uint i = 0; i < MAX_COUNT; ++i) {
        q.Enqueue(i);
    }
    Consume(q, false);
}

TEST(MPSCQueueTest, spsc_multi_thread) {
    eabase::MPSCQueue<uint> q;
    pthread_t produce_tid;
    ASSERT_EQ(0, pthread_create(&produce_tid, NULL, ProduceThread, &q));
    pthread_t consume_tid;
    ASSERT_EQ(0, pthread_create(&consume_tid, NULL, ConsumeThread1, &q));

    pthread_join(produce_tid, NULL);
    pthread_join(consume_tid, NULL);

}

eabase::atomic<uint> g_index(0);
void* MultiProduceThread(void* arg) {
    auto q = (eabase::MPSCQueue<uint>*)arg;
    while (true) {
        uint i = g_index.fetch_add(1, eabase::memory_order_relaxed);
        if (i >= MAX_COUNT) {
            break;
        }
        q->Enqueue(i);
    }
    return NULL;
}

eabase::Mutex g_mutex;
bool g_counts[MAX_COUNT];
void Consume2(eabase::MPSCQueue<uint>& q) {
    uint empty_count = 0;
    uint count = 0;
    while (true) {
        uint d;
        if (!q.Dequeue(d)) {
            ASSERT_LT(empty_count++, (const uint)10000);
            ::usleep(1 * 1000);
            continue;
        }
        ASSERT_LT(d, MAX_COUNT);
        {
            BAIDU_SCOPED_LOCK(g_mutex);
            ASSERT_FALSE(g_counts[d]);
            g_counts[d] = true;
        }
        if (++count >= MAX_COUNT) {
            break;
        }
    }
}

void* ConsumeThread2(void* arg) {
    auto q = (eabase::MPSCQueue<uint>*)arg;
    Consume2(*q);
    return NULL;
}

TEST(MPSCQueueTest, mpsc_multi_thread) {
    eabase::MPSCQueue<uint> q;

    int thread_num = 8;
    pthread_t threads[thread_num];
    for (int i = 0; i < thread_num; ++i) {
        ASSERT_EQ(0, pthread_create(&threads[i], NULL, MultiProduceThread, &q));
    }

    pthread_t consume_tid;
    ASSERT_EQ(0, pthread_create(&consume_tid, NULL, ConsumeThread2, &q));

    for (int i = 0; i < thread_num; ++i) {
        pthread_join(threads[i], NULL);
    }
    pthread_join(consume_tid, NULL);

}


}