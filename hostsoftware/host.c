#define _GNU_SOURCE

#include "arduinocom.h"
#include "hist.h"
#include "record.h"
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>


int read_cmd(enum message *cmd, char *extra);
int matches(const char *buf, const char *prefix);
void main_loop_data(int tty_fd, char *record, char **hists);
void main_loop_cli(char *record, char **hists);

char *names[3] = {"Temperature", "Pressure", "Humidity"};
char *hist_file_names[3] = {"tmp_hist.bin", "prs_hist.bin", "hmd_hist.bin"};

/* Inter Process Communications
 * signal_pipe is for sending the user commands to the background process
 * data_pipe is for sending the data from the background to client process 
 */
int pid;
int signal_pipe[2];  
int data_pipe[2];

int main(int argc, char **argv) {
  int res = 0;

  /*
   * /dev/ttyACM0 unless the user specifies a file
   */
  char *serial_file = (argc == 2) ? argv[1] : "/dev/ttyACM0";

  /*
   * Need the following flags to open:
   * O_RDWR: to read from/write to the devices
   * O_NOCTTY: Do not become the process's controlling terminal
   * O_NDELAY: Open the resource in nonblocking mode
   */
  int serial_fd = open(serial_file, O_RDWR | O_NOCTTY | O_NDELAY);
  if (serial_fd == -1) {
    perror("Error opening serial");
    res = -1;
    goto done;
  }

  /* Configure settings on the serial port */
  if (init_tty(serial_fd) == -1) {
    perror("Issue setting up serial file");
    res = -1;
    goto done;
  }

  // Three histograms, Temp, Pressure, Humidity
  char *hists[3];
  int hist_fds[3];
  for (int i = 0; i < 3; i++) {
    /*
     * construct_hist is responsible for opening the file and mmaping the file
     * reports a file descriptor by filling a user provided pointer
     * reports the mapped region address by filling another user provided
     * pointer return value is 0 on success
     */
    if (construct_hist(hist_file_names[i], &hist_fds[i], &hists[i])) {
      res = -1;
      goto done;
    }
  }
  int record_fd = 0;
  char *record = NULL;
  /*
   * construct_record is responsible for opening the file and mmaping the file
   * the file descriptor is loaded into user provided pointer
   * the address of the mapped region is loaded into another user provided
   * pointer return value is 0 on success
   */
  if (construct_record("record.bin", &record_fd, &record)) {
    res = -1;
    goto done;
  } 

  /* Create pipes for comm between parent and child
   * Signal pipe is used by the cli loop to ask for
   * values from the data loop.  
   * Note, with pipe, on success, zero is returned
   */
   if (pipe(signal_pipe) == -1) {
            perror("Error initializing signal pipe");
            return -1;
	}
  
   if (pipe(data_pipe) == -1) {
            perror("Error initializing data pipe");
            return -1;
	}

  printf("Initializing Host-side Processes\n");
  sleep(1);   //let arduino set up

  /* Fork and call main_loop_cli in the child
   * and main_loop_data in the parent.
   */
	if((pid = fork()) == -1) {
            perror("Error forking");
            exit(1);
	}
	if(pid == 0) {
	    main_loop_cli(record, hists); 
	} else {
	    main_loop_data(serial_fd, record, hists);
	}
  /*
   * cleanup resources
   */
done:
  if (serial_fd)
    close(serial_fd);
  for (int i = 0; i < 3; i++) {
    if (hist_fds[i])
      deconstruct_hist(hist_fds[i], hists[i]);
  }
  if (record_fd)
    deconstruct_record(record_fd, record);
  return res;
}

/* Foreground process run in the child.
 * Takes care of taking user input and sending
 * the appropriate commands to the background process.
 * Some commands require a reply from the parent and
 * some just print visualization info.
 */
void main_loop_cli(char *record, char **hists) {
  /* We only want to SEND signals and READ data */
  //0 is the read side for both pipes and 1 is the write side
    close(signal_pipe[0]);
    close(data_pipe[1]);

  char* buf;
  size_t buffer_size = 10 * sizeof(char);
  buf = (char *) malloc(buffer_size);
  int print_help = 0;

  // Infinite loop for displayin the menu
  while (1) {
    printf("->");

    /* Checks user input */
    if(getline(&buf, &buffer_size, stdin) < 0) {
        perror("Could not read line");
        //return;
    }

    printf("USER INPUT: %s\n", buf);

    /* Compares user input with each option on the help menu */
    if (matches(buf, "resume")){
        write(signal_pipe[1], "resume", strlen("resume"));

    }
    else if (matches(buf, "pause")) {
        write(signal_pipe[1], "pause", strlen("pause"));

    }
    else if (matches(buf, "exit")) {
        write(signal_pipe[1], "exit", strlen("exit"));
        exit(1);

    }
    else if (buf[0] == 'b' && buf[1] == 'l' && buf[2] == 'i' 
            && buf[3] == 'n' && buf[4] == 'k' && buf[5] == ' ') {
        write(signal_pipe[1], buf, strlen(buf));

    }
    else if (matches(buf, "env")) {
        write(signal_pipe[1], "env", strlen("env"));

        /* For reading data back from the Arduino */
        char data_reply[17]; 
        data_reply[16] = '\0';
        int consumed = 0;
        int res = 0;
        while (consumed < 16) {
            res = read(data_pipe[0], data_reply + (consumed * sizeof(char)), 16 - consumed);
            if (res >= 0) {
                consumed += res;
            } else {
                perror("Issue reading from data_pipe");
                exit(1);
            }
        }
        printf("\t Arduino Reply: (consumed: %d):  %03u, %03u, %03u, %03u, %.12s\n", consumed, 
              (unsigned char)data_reply[0], (unsigned char)data_reply[1], 
              (unsigned char)data_reply[2], (unsigned char)data_reply[3], data_reply + 4);

    }
    else if (buf[0] == 'h' && buf[1] == 'i' && buf[2] == 's' 
              && buf[3] == 't' && buf[5] == 't') {
        print_hist(hists[0]);

    }
    else if (buf[0] == 'h' && buf[1] == 'i' && buf[2] == 's' 
              && buf[3] == 't' && buf[5] == 'p') {
        print_hist(hists[1]);

    }
    else if (buf[0] == 'h' && buf[1] == 'i' && buf[2] == 's' 
              && buf[3] == 't' && buf[5] == 'h') {
        print_hist(hists[2]);

    }
    else if (matches(buf, "record")) {
        printf("%s", record);

    }
    // This is for printing the menu
    else if (matches(buf, "help") ) 
    {  
      print_help = 1;
    }
    else 
    {
      print_help = 1;
    }

    if (print_help){
      printf("Available commands: \n");
      printf("\tpause\n");
      printf("\tresume\n");
      printf("\tblink X\n");
      printf("\tenv\n");
      printf("\trecord\n");
      printf("\thist t\n");
      printf("\thist p\n");
      printf("\thist h\n");
      //printf("\t*hist t X\n");
      //printf("\t*hist p X\n");
      //printf("\t*hist h X\n");
      printf("\texit\n");
      printf("\n");
      print_help = 0;
    }

  }

}    

/* Uses select to wait 1 second for changes in a set of 
 * file descriptors (signal_pipe). If there is a change, 
 * it reads all the contents, filling up buf. 
 * If the buf matches known commands: "resume", "pause", 
 * "exit", "blink X", or "request", assign the correct
 * values to output parameters cmd and extra.
 *   extra would contain the value of X for blink.
 * If select triggered, return a 1.
 * if select doesn't trigger, returns 0;
*/
int read_cmd(enum message *cmd, char *extra) {
  static char buf[1024];
  static size_t offset = 0;
  static struct timeval tv;
  static fd_set readfds;
  int _extra = 0;

  // reset these values every time we call select
  tv.tv_usec = 0;
  tv.tv_sec = 1; //Timeout is one second

  FD_ZERO(&readfds);
  FD_SET(signal_pipe[0], &readfds);
  //initialize file descriptors and only read end of signal_pipe)
 

  //   to be read from signal_pipe
  if (select(FD_SETSIZE, &readfds, NULL, NULL, &tv)) {
    // Default return parameters
    *cmd = 0;
    *extra = 0;
    // reads the data from the signal_pipe
    offset += read(signal_pipe[0], buf + offset, 1024 - offset);
    // loop for seeking any matches between "/n" in buf
    while (1) {
      // check the buffer for our key words

      if (matches(buf, "resume")) {
        *cmd = RESUME;

      } else if (matches(buf, "pause")) {
        *cmd = PAUSE;

      } else if (matches(buf, "exit")) {
        *cmd = EXIT;
        exit(1);

      } else if (sscanf(buf, "blink %d", &_extra) == 1) {
        *cmd = BLINK;
        *extra = (char)_extra; //stores blink frequency 
       
      } else if (matches(buf, "env")) {
        *cmd = REQUEST;

      }

      // reset buffer
      if (cmd != 0) {
        offset = 0;
        memset(buf, 0, 1024);
        return 1;
      }

      // If we are unable to find a key word, look for a \n
      char *nl = strstr(buf, "\n");
      if (nl) {
        // shift buf over until nl+1 is the first character
        size_t shift = nl - buf + 1;
        offset -= shift;
        memmove(buf, nl + 1, offset);
        memset(buf + offset, 0, 1024 - offset);
      }
      
    }
    return 0;
  }
  return 0;
}  

/* string compare method*/
int matches(const char *buf, const char *prefix) {
  int len = strlen(prefix) - 1;
  int res = strncmp(buf, prefix, len);
  if (res == 0)
    return 1;
  return 0;
}

/* Background process that performs the following sequence:
 * uses read_cmd to check if the user sent a commad.
 *   if there is a command, it sends it (cmd) 
 * and possibly the "extra" char over Serial to the Arduino.
 * if the command was not to pause, and a second has passed, 
 *   then request data from Serial, read the result into buf.
 *   (Always 16 bytes), and increase the num_readings.
 *   Then update each hist and the record. If the command was 
 *   "request", then Also send the reply through the data_pipe.  , 
 * This performs 24 readings over 3 days. 
 */
void main_loop_data(int tty_fd, char *record, char **hists) {
  enum message msg = 0;
  char to_send[2] = {0, 0};
  char extra = 0;
  time_t next_time = 0;
  int is_paused = 0;
  int num_readings = 0;

  /* We only want to READ signals and SEND data */
  close(signal_pipe[1]);
  close(data_pipe[0]);

  printf("Beginning Sensor Reading\n");
  while (num_readings < 3 * 24) {
    /*
     * Check if the user has written in a valid command 
     * and writes to the Arduino
     */
        if((read_cmd(&msg, &extra) == 0)) {
          if (msg == RESUME) {
            write(tty_fd, &msg, sizeof(msg));
            is_paused = 0;
      
          } else if (msg == PAUSE) {
              write(tty_fd, &msg, sizeof(msg));
              is_paused = 1;

          } else if (msg == EXIT) {
              exit(1);

          } else if (msg == BLINK) {
              to_send[0] = msg;
              to_send[1] = extra;

              send_msg(to_send, 2, tty_fd);

          } else if (msg == REQUEST) {
              write(tty_fd, &msg, sizeof(msg));

          }
        }

    /*
     * Check if we should request a reading from the user
     */
    time_t t;
    time(&t);
    
    if (!is_paused && next_time <= t) {
      next_time = t + 1;

        write(tty_fd, "2", 1);
      /*
       * Format of reply as follows
       * BYTE # | VALUE
       * 0      | Temp reading (between 0 and 255)
       * 1      | Pressure reading (0-255)
       * 2      | Humidity reading (0-255)
       * 3      | Rain - 0 means no observation, 1 means no rain, 2 means rain
       * 4      | timestamp year index 0 (ISO-8601)
       * 5      | timestamp year index 1 (ISO-8601)
       * 6      | timestamp year index 2 (ISO-8601)
       * 7      | timestamp year index 3 (ISO-8601)
       * 8      | timestamp month index 0 (ISO-8601)
       * 9      | timestamp month index 1 (ISO-8601)
       * 10     | timestamp day index 0 (ISO-8601)
       * 11     | timestamp day index 1 (ISO-8601)
       * 12     | timestamp hour index 0 (ISO-8601)
       * 13     | timestamp hour index 1 (ISO-8601)
       * 14     | timestamp minute index 0 (ISO-8601)
       * 15     | timestamp minute index 1 (ISO-8601)
       *
       * e.g.
       * 202001010600 represents January 1st 2020 at exactly 6 AM
       */

      /*
       * read and accumulate a 16 byte reply
       * (kept in a null terminated 17 byte buffer)
       */
      char reply[17];
      reply[16] = '\0';
      int consumed = 0;
      while (consumed < 16) {
        int res = read(tty_fd, reply + (consumed * sizeof(char)), 16 - consumed);
        if (res >= 0) {
          consumed += res;
        } else {
          perror("Issue reading from serial");
          return;
        }
      }

      // Debug print
      // printf("\t Got: %03u, %03u, %03u, %03u, %.12s\n", (unsigned char)reply[0], 
      // 	(unsigned char)reply[1], (unsigned char)reply[2], 
      // 	(unsigned char)reply[3], reply + 4);

      num_readings++;

      /*
       * Insert new sensor reading into hists and record data structures
       */
      int time = (reply[12] - '0') * 10 + (reply[13] - '0');
      for (int i = 0; i < 3; i++) {
        // hist only needs the hour between 0 and 23
       update_hist(hists[i], reply[i], time);

      }
      update_record(record, reply[0], reply[1], reply[2], reply[3], reply + 4);

      /* Now reply to client if the message was REQUEST */
        if(msg == REQUEST) {
            write(data_pipe[1], reply, 17);
        }
      /* Resets msg to 0 */
        msg = 0;
    }
  }
  printf("DONE with parent LOOP\n");
}
