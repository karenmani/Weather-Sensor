#ifndef record_h_
#define record_h_

// These defines set the sizes of the record file!!!
#define RECORDLEN 36
#define NUMRECORDS (24 * 3)
#define RECORD_FILESIZE (NUMRECORDS * RECORDLEN)

int _records_so_far;

int update_record(char *record, unsigned char tmp, unsigned char prs,
                  unsigned char hmd, unsigned char rained,
                  char timeString[12]) {

  /*
   * Makes sure that we do not write over the length of the file
   */
  if (_records_so_far >= NUMRECORDS) {
    return -1;
  }

   /*
   *  Write out data in a CSV format
   *  and counts the number of records archived
   */
  //first paramter represents the point in record at which it should print the value for the datapoints
  //keeps track of the number of records in the .bin file and multiplies that by the length of the record to use as offset from the start of record
  //tells you where to write to
  sprintf((record + (_records_so_far * (sizeof(char) * RECORDLEN))), "  %03u, %03u, %03u, %03u, %.12s,\n", tmp, prs, hmd, rained, timeString);
  _records_so_far++;

  return 0;
}

/*
 * construct_record is responsible for opening the file and mmaping the file
 * the file descriptor (fd) is loaded into user provided pointer
 * the address of the mapped region (buffer) is loaded into another user provided
 * pointer return value is 0 on success
 */
int construct_record(char *record_fname, int *fd, char **buffer) {
  _records_so_far = 0;

  /*
   * Opens a file for reading and writing with permission granted to user and error checks
   */
  *fd = open(record_fname, O_RDWR | O_CREAT | O_TRUNC, (mode_t)0600);
         if (*fd == -1) {
             perror("Error opening mmapped file");
             return -1;
         }

  /*
   * Seeks to the end of the file and write a \0 to extend the file and error checks
   */
  if(lseek(*fd, RECORD_FILESIZE, SEEK_SET) == -1) {
          perror("Error calling lseek()");
          close(*fd);
          return -1;
   }

   //writes a \0 to the end of the file and error checks
  if((write(*fd, "\0", 1)) == -1) {
              perror("Error writing");
              close(*fd);
              return -1;
  }

  /*
   * Maps the proper region of the file into the user's address space and error checks
   * Casts as a char* since buffer points to the pointer record
   */
    *buffer = (char*)mmap(0, RECORD_FILESIZE, PROT_READ | PROT_WRITE, MAP_SHARED, *fd, 0);
    if(*buffer == MAP_FAILED) {
      close(*fd);
      perror("Error mapping the file");
      return -1;
    }

  return 0;
}

int deconstruct_record(int record_fd, char *record) {
  /*
   * Un-maps and closes file
   */
  
  if(munmap(record, RECORD_FILESIZE) == -1) {
      close(record_fd);
      perror("Error un-mapping the file");
      return -1;
    }

    close(record_fd);

  return 0;
}

#endif
