#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>

#include "common.h"

int log_error(const char *const func, const char *const file, const int line)
{
	return fprintf(stderr, "failed %s() in %s:%d\n", func, file, line);
}

int log_error_errno(const char *const func, const char *const file, const int line)
{
	return fprintf(stderr, "failed %s() in %s:%d with: %s\n", func, file, line, strerror(errno));
}

size_t resize_buffer(uint8_t **buf, size_t length)
{
	uint8_t *buf_temp = realloc(*buf, length);
	if (!buf_temp) {
		perror("realloc() failed");
		return 0;
	}
	*buf = buf_temp;
	return length;
}

ssize_t write_all(int fd, const void *buf, size_t count)
{
	ssize_t result = 0;
	ssize_t bytes_written = 0;
	while (bytes_written < count) {
		// TODO: use a union?
		result = write(fd, (uint8_t *)buf + bytes_written, count - bytes_written);
		if (result < 1) {
			if (result < 0) {
				if (EINTR == errno) {
					fprintf(stderr, "got signal, exiting\n");
					return 0;
				}
				log_error_errno("write", __FILE__, __LINE__);
			}
			return result;
		}

		bytes_written += result;
	}

	return bytes_written;
}

ssize_t read_all(int fd, void *buf, size_t count)
{
	ssize_t result = 0;
	ssize_t bytes_read = 0;
	while (bytes_read < count) {
		// TODO: use a union?
		result = read(fd, (uint8_t *)buf + bytes_read, count - bytes_read);
		if (result < 1) {
			if (result < 0) {
				if (EINTR == errno) {
					fprintf(stderr, "got signal, exiting\n");
					return 0;
				}
				log_error_errno("read", __FILE__, __LINE__);
			}
			return result;
		}

		bytes_read += result;
	}

	return bytes_read;
}
