#include <fstream>
#include <map>
#include <vector>
#include <string>
#include <chrono>
#include <random>

#define POSIX
#include <unistd.h>
#include <sys/stat.h>
#include <signal.h>
#include <syslog.h>

#define MAX_EVENTS 5

using namespace std;

class Logjam {
  public:

  Logjam (string _n, string _p) : name(_n), pattern(_p) {};
  string name;
  string pattern;
};

typedef vector<Logjam> log_t;

const static string pidfilename("/var/run/logjam.pid");

const log_t logfiles = {
  Logjam(string("/tmp/blah.txt"), string("blah")),
  Logjam(string("/tmp/baz.txt"), string("quux"))
};


void
sig_term(int sig) {
  signal(SIGTERM, sig_term);
  exit(0);
}

void
remove_pidfile() {
  remove(pidfilename.c_str());
  syslog(LOG_NOTICE, "removed pid file");
}


void
exit_message() {
  syslog(LOG_NOTICE, "exiting... bye bye");
}

uint64_t
occurrence() {

  random_device rd;
  mt19937 gen(rd());
  uniform_int_distribution<> occurs(1, MAX_EVENTS);
  return occurs(gen);
}


string
current_time() {

  // change to use plain old unix time
  chrono::time_point<chrono::system_clock> now;
  now = chrono::system_clock::now();
  time_t time_now = chrono::system_clock::to_time_t(now);
  struct tm * timeinfo;
  timeinfo = localtime(&time_now);
  char buf[80];
  strftime(buf, 80, "%c", timeinfo);
  return string(buf);
}


void
write_to_file(Logjam & lp) {

  ofstream of;
  of.exceptions(std::ofstream::failbit | std::ofstream::badbit);

  try {
    of.open(lp.name, ofstream::app);
  } catch (ofstream::failure e) {
    syslog(LOG_ERR, "Exception opening log file: %d", of.rdstate());
    exit(0);
  }

  string time = current_time();
  uint64_t entry_count = occurrence();

  for (uint64_t i=1; i<entry_count; ++i) {
    of << time << ": " << lp.pattern << "\n";
  }
  // TODO: log entry count to syslog
  of.close();
}


void
write_to_logs(log_t logs) {

  for (auto &lp : logs) {
    write_to_file(lp);
  }
}


void
write_pid_file(int pid) {

  ofstream pidfile;
  pidfile.exceptions(std::ofstream::failbit | std::ofstream::badbit);

  try {
    pidfile.open(pidfilename, ofstream::trunc);
  } catch (ofstream::failure e) {
    syslog(LOG_ERR, "Exception opening pidfile file: %d", pidfile.rdstate());
    exit(0);
  }

  pidfile << pid << endl;
  pidfile.close();
}


void
close_open_fd() {
  for (int fd = sysconf(_SC_OPEN_MAX); fd>0; fd--) { close (fd); }
}


void
daemonize() {

  pid_t pid;

  syslog(LOG_NOTICE, "detaching child");

  pid = fork();

  if (pid < 0) exit(EXIT_FAILURE);
  if (pid > 0) exit(EXIT_SUCCESS);

  signal(SIGCHLD, SIG_IGN);
  signal(SIGHUP, SIG_IGN);

  pid = fork();
  if (pid < 0) exit(EXIT_FAILURE);
  if (pid > 0) exit(EXIT_SUCCESS);

  umask(0);

  int result = chdir("/");

  if (setsid() < 0) exit(EXIT_FAILURE);
  close_open_fd();

  atexit(exit_message);
  atexit(remove_pidfile);
  signal(SIGTERM, sig_term);

  syslog(LOG_NOTICE, "current pid %d", getpid());
  write_pid_file(getpid());

}


void
logjam() {

  openlog("logjam", LOG_PID, LOG_DAEMON);
  daemonize();

  while(1) {
    // TODO: randomize sleep time
    sleep(30);
    write_to_logs(logfiles);
  }
}


int
main(int argc, char ** argv) {

  logjam();
  return 0;
}
