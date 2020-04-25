#ifndef arduinocom_h_
#define arduinocom_h_

#include <stdio.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

enum message {
  BLINK = '1',
  REQUEST = '2',
  PAUSE = '3',
  RESUME = '4',
  EXIT = '5',
};


// blocking write
int send_msg(const char *msg, int len, int fd) {
  int quantity = 0;
  while (len - quantity) {
    int res = write(fd, msg + quantity, len - quantity);
    if (res >= 0) {
      quantity += res;
    } else {
      return res;
    }
  }
  tcflush(fd, TCIOFLUSH);
  return quantity;
}

// non blocking read
int recv_msg(char *buf, int max_len, int fd) { return read(fd, buf, max_len); }

int init_tty(int fd) {
  struct termios tty;
  /*
   * Configure the serial port.
   * First, get a reference to options for the tty
   * Then, set the baud rate to 9600 (same as on Arduino)
   */
  memset(&tty, 0, sizeof(tty));
  if (tcgetattr(fd, &tty) == -1) {
    perror("tcgetattr");
    return -1;
  }

  if (cfsetospeed(&tty, (speed_t)B9600) == -1) {
    perror("ctsetospeed");
    return -1;
  }
  if (cfsetispeed(&tty, (speed_t)B9600) == -1) {
    perror("ctsetispeed");
    return -1;
  }

  // 8 bits, no parity, no stop bits
  tty.c_cflag &= ~PARENB;
  tty.c_cflag &= ~CSTOPB;
  tty.c_cflag &= ~CSIZE;
  tty.c_cflag |= CS8;

  // No flow control
  tty.c_cflag &= ~CRTSCTS;

  // Set local mode and enable the receiver
  tty.c_cflag |= (CLOCAL | CREAD);

  // Disable software flow control
  tty.c_iflag &= ~(IXON | IXOFF | IXANY);

  // Make raw
  tty.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
  tty.c_oflag &= ~OPOST;

  // Infinite timeout and return from read() with >1 byte available
  tty.c_cc[VMIN] = 0;
  tty.c_cc[VTIME] = 0;

  // Update options for the port
  if (tcsetattr(fd, TCSANOW, &tty) == -1) {
    perror("tcsetattr");
    return -1;
  }
  sleep(4);
  return 0;
}

#endif
