#include <cstdio>
#include <thread>
#include <vector>

#include "caffe2/core/db.h"
#include "caffe2/core/init.h"
#include "caffe2/core/timer.h"
#include "caffe2/core/logging.h"

CAFFE2_DEFINE_string(input_db, "", "The input db.");
CAFFE2_DEFINE_string(input_db_type, "", "The input db type.");
CAFFE2_DEFINE_int(report_interval, 1000, "The report interval.");
CAFFE2_DEFINE_int(repeat, 10, "The number to repeat the throughput test.");
CAFFE2_DEFINE_bool(use_reader, false, "If true, use the reader interface.");
CAFFE2_DEFINE_int(num_read_threads, 1,
                   "The number of concurrent reading threads.");

using caffe2::db::Cursor;
using caffe2::db::DB;
using caffe2::db::DBReader;
using caffe2::string;

void TestThroughputWithDB() {
  std::unique_ptr<DB> in_db(caffe2::db::CreateDB(
      caffe2::FLAGS_input_db_type, caffe2::FLAGS_input_db, caffe2::db::READ));
  std::unique_ptr<Cursor> cursor(in_db->NewCursor());
  for (int iter_id = 0; iter_id < caffe2::FLAGS_repeat; ++iter_id) {
    caffe2::Timer timer;
    for (int i = 0; i < caffe2::FLAGS_report_interval; ++i) {
      string key = cursor->key();
      string value = cursor->value();
      //VLOG(1) << "Key " << key;
      cursor->Next();
      if (!cursor->Valid()) {
        cursor->SeekToFirst();
      }
    }
    double elapsed_seconds = timer.Seconds();
    printf("Iteration %03d, took %4.5f seconds, throughput %f items/sec.\n",
           iter_id, elapsed_seconds,
           caffe2::FLAGS_report_interval / elapsed_seconds);
  }
}

void TestThroughputWithReaderWorker(const DBReader* reader, int thread_id) {
  string key, value;
  for (int iter_id = 0; iter_id < caffe2::FLAGS_repeat; ++iter_id) {
    caffe2::Timer timer;
    for (int i = 0; i < caffe2::FLAGS_report_interval; ++i) {
      reader->Read(&key, &value);
    }
    double elapsed_seconds = timer.Seconds();
    printf("Thread %03d iteration %03d, took %4.5f seconds, "
           "throughput %f items/sec.\n",
           thread_id, iter_id, elapsed_seconds,
           caffe2::FLAGS_report_interval / elapsed_seconds);
  }
}

void TestThroughputWithReader() {
  caffe2::db::DBReader reader(
      caffe2::FLAGS_input_db_type, caffe2::FLAGS_input_db);
  std::vector<std::unique_ptr<std::thread>> reading_threads(
      caffe2::FLAGS_num_read_threads);
  for (int i = 0; i < reading_threads.size(); ++i) {
    reading_threads[i].reset(new std::thread(
        TestThroughputWithReaderWorker, &reader, i));
  }
  for (int i = 0; i < reading_threads.size(); ++i) {
    reading_threads[i]->join();
  }
}

int main(int argc, char** argv) {
  caffe2::GlobalInit(&argc, &argv);
  if (caffe2::FLAGS_use_reader) {
    TestThroughputWithReader();
  } else {
    TestThroughputWithDB();
  }
  return 0;
}
