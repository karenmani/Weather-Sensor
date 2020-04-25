#ifndef hist_h_
#define hist_h_
#include <fcntl.h>
#include <sys/mman.h>

// These defines set the sizes of the histograms!!!
#define NBUCKETS (256 / 16)
#define FILESIZE (24 * NBUCKETS)
char *mymap;


int update_hist(char *hist, unsigned char value, int time) {
  /*
   * the hist file is treated as a two dimension array of characters
   * each of these characters represents how many observations of that bucket have been made
   * the rows in this two dimensional array represent each hour 0-23, while each byte in the row
   * represents the number of observations for that bucket.  There are 16 buckets, which store
   * observation for a range of size 16.  For instance, the 0th bucket stores observation 0-15
   */

    //increments histogram index by getting the bucket at which the temp/humidity/pressure vale will fall into and
    //adds it to the time value since the histogram acts as one long array where values at the second hour can be
    //displayed from hist[17]-->hist[32] and so on.
    hist[(value/NBUCKETS) + (time * NBUCKETS)] += 1;

    return 0;
}

/*
 * Prints out the histogram. Nothing to do here (but study the code!)
 */
int print_hist(char *hist) {
  printf("Hour|   16   32   48   64   80   96  112  128  114  160  176  192  208  224  240  256\n");
  printf("-------------------------------------------------------------------------------------\n");
  for (int t = 0; t < 24; t++) {
    printf("% 3d |", t);
    for (int b = 0; b < NBUCKETS; b++) {
      printf("  % 3d", (unsigned char)hist[t * NBUCKETS + b]);
    }
    printf("\n");
  }
  printf("    --------------------------------------------------------------------------------\n\n");

  return 0;
}

/*
 * construct_hist is responsible for opening the file and mmaping the file
 * reports a file descriptor (fd) by filling a user provided pointer
 * reports the mapped region address (buffer) by filling another user provided pointer
 * return value is 0 on success
 */
int construct_hist(char *hist_fname, int *fd, char **buffer) {
  /*
   * Open a file for reading and writing with permission granted to user and error checks
   */
   *fd = open(hist_fname, O_RDWR | O_CREAT, (mode_t)0600); //allows you to get address from memory
       if (*fd == -1) {
           perror("Error opening mmapped file");
           return -1;
       }

  /*
   * Seek to the end of the file and write a \0 to extend the file and error checks
   */
   if(lseek(*fd, FILESIZE, SEEK_SET) == -1) {
        close(*fd);
        perror("Error calling lseek()");
        return -1;
   }
   if((write(*fd, "\0", 1)) == -1) {
            perror("Error calling write");
            close(*fd);
            return -1;
   }

  /*
   * Map the proper region of the file into the user's address space and error checks
   */
  *buffer = (char*)mmap(0, FILESIZE, PROT_READ | PROT_WRITE, MAP_SHARED, *fd, 0);
  if(*buffer == MAP_FAILED) {
    close(*fd);
    perror("Error mapping the file");
    return -1;
  }
  return 0;
}

int deconstruct_hist(int hist_fd, char *hist) {

  /*
   * Un-maps and closes
   */
  if(munmap(hist, FILESIZE) == -1) {
    close(hist_fd);
    perror("Error un-mapping the file");
    return -1;
  }

  close(hist_fd);
  return 0;
}
#endif
