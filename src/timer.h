/*
 * BSD licensed from http://sites.google.com/site/jivsoft/Home/ (Ken Wilder)
 */

#ifndef TIMER_H
#define TIMER_H

#include <ctime>
#include <iostream>
#include <iomanip>
#include <sys/time.h>

class timer
{
    friend std::ostream& operator<<(std::ostream& os, timer& t);

    private:
    bool running;
    timeval start_time;
    timeval acc;

    double acc_time;

    double elapsed_time();

    public:
    // 'running' is initially false.  A timer needs to be explicitly started
    // using 'start' or 'restart'
    timer() : running(false), acc_time(0) { }

    void start(const char* msg = 0);
    void restart(const char* msg = 0);
    void stop(const char* msg = 0);
    void check(const char* msg = 0);

}; // class timer

//===========================================================================
// Return the total time that the timer has been in the "running"
// state since it was first "started" or last "restarted".  For
// "short" time periods (less than an hour), the actual cpu time
// used is reported instead of the elapsed time.

inline double timer::elapsed_time()
{
    double time, seconds, useconds;

    timeval now;
    gettimeofday(&now, NULL);
    seconds  = now.tv_sec  - start_time.tv_sec;
    useconds = now.tv_usec - start_time.tv_usec;
//  mtime = ((seconds) * 1000 + useconds/1000.0) + 0.5;
    time = seconds + useconds/1000000;
    return time;
} // timer::elapsed_time

//===========================================================================
// Start a timer.  If it is already running, let it continue running.
// Print an optional message.

inline void timer::start(const char* msg)
{
    // Print an optional message, something like "Starting timer t";
    if (msg) std::cout << msg << std::endl;

    // Return immediately if the timer is already running
    if (running) return;

    // Set timer status to running and set the start time
    running = true;
    gettimeofday(&start_time, NULL);

} // timer::start

//===========================================================================
// Turn the timer off and start it again from 0.  Print an optional message.

inline void timer::restart(const char* msg)
{
    // Print an optional message, something like "Restarting timer t";
    if (msg) std::cout << msg << std::endl;

    // Set timer status to running, reset accumulated time, and set start time
    running = true;
    acc_time = 0;
    gettimeofday(&start_time, NULL);
} // timer::restart

//===========================================================================
// Stop the timer and print an optional message.

inline void timer::stop(const char* msg)
{
    // Print an optional message, something like "Stopping timer t";
    if (msg) std::cout << msg << std::endl;

    // Compute accumulated running time and set timer status to not running
    if (running) acc_time += elapsed_time();
    running = false;

} // timer::stop

//===========================================================================
// Print out an optional message followed by the current timer timing.

inline void timer::check(const char* msg)
{
    // Print an optional message, something like "Checking timer t";
    if (msg) std::cout << msg << " : ";

    std::cout << "Elapsed time [" << std::setiosflags(std::ios::fixed)
        << std::setprecision(2)
        << acc_time + (running ? elapsed_time() : 0) << "] seconds\n";

} // timer::check

//===========================================================================
// Allow timers to be printed to ostreams using the syntax 'os << t'
// for an ostream 'os' and a timer 't'.  For example, "cout << t" will
// print out the total amount of time 't' has been "running".

inline std::ostream& operator<<(std::ostream& os, timer& t)
{
    os << std::setprecision(2) << std::setiosflags(std::ios::fixed)
        << t.acc_time + (t.running ? t.elapsed_time() : 0) << "s";
    return os;
}

//===========================================================================

#endif // TIMER_H
